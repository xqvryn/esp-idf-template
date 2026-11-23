#include "ota_http.hpp"

#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_status.hpp"
#include "ota/ota.hpp"
#include "sdkconfig.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

constexpr char tag[] = "camera-timelapse";
constexpr int kRebootDelayMs = 1500;
constexpr std::size_t kRecvChunk = 2048;

SemaphoreHandle_t update_lock = nullptr;
std::atomic<bool> busy{false};
ota::ImageState running_state = ota::ImageState::unknown;

ota::ImageState map_image_state(esp_ota_img_states_t state) {
    switch (state) {
    case ESP_OTA_IMG_NEW:
        return ota::ImageState::new_image;
    case ESP_OTA_IMG_PENDING_VERIFY:
        return ota::ImageState::pending_verify;
    case ESP_OTA_IMG_VALID:
        return ota::ImageState::valid;
    case ESP_OTA_IMG_INVALID:
        return ota::ImageState::invalid;
    case ESP_OTA_IMG_ABORTED:
        return ota::ImageState::aborted;
    default:
        return ota::ImageState::unknown;
    }
}

const esp_partition_t* running_partition() {
    return esp_ota_get_running_partition();
}

void schedule_reboot() {
    vTaskDelay(pdMS_TO_TICKS(kRebootDelayMs));
    esp_restart();
}

esp_err_t write_upload(httpd_req_t* req, char* message, std::size_t message_size) {
    const int content_len = req->content_len;
    const esp_partition_t* destination = esp_ota_get_next_update_partition(nullptr);
    if (destination == nullptr) {
        std::snprintf(message, message_size, "no update partition");
        return ESP_FAIL;
    }
    if (content_len == 0) {
        std::snprintf(message, message_size, "empty image");
        return ESP_FAIL;
    }
    if (content_len > 0 && static_cast<std::size_t>(content_len) > destination->size) {
        std::snprintf(message, message_size, "image larger than ota slot");
        return ESP_FAIL;
    }

    std::uint8_t chunk[kRecvChunk];
    int received = 0;
    while (received < static_cast<int>(ota::app_image_prefix_size)) {
        const int want = static_cast<int>(
            std::min(kRecvChunk, ota::app_image_prefix_size - static_cast<std::size_t>(received)));
        const int got = httpd_req_recv(req, reinterpret_cast<char*>(chunk) + received, want);
        if (got <= 0) {
            std::snprintf(message, message_size, "receive failed");
            return ESP_FAIL;
        }
        received += got;
        if (content_len > 0 && received >= content_len) {
            break;
        }
    }
    const ota::AppImagePrefix prefix = ota::inspect_app_image_prefix(
        std::span<const std::uint8_t>(chunk, static_cast<std::size_t>(received)));
    if (!prefix.valid) {
        std::snprintf(message, message_size, "not an ESP-IDF application image");
        return ESP_FAIL;
    }

    const std::size_t ota_size =
        content_len > 0 ? static_cast<std::size_t>(content_len) : OTA_SIZE_UNKNOWN;
    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(destination, ota_size, &handle);
    if (err != ESP_OK) {
        std::snprintf(message, message_size, "ota begin failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_ota_write(handle, chunk, static_cast<std::size_t>(received));
    int total = received;
    while (err == ESP_OK) {
        int want = static_cast<int>(kRecvChunk);
        if (content_len > 0) {
            const int left = content_len - total;
            if (left <= 0) {
                break;
            }
            want = std::min(want, left);
        }
        const int got = httpd_req_recv(req, reinterpret_cast<char*>(chunk), want);
        if (got == 0 && content_len <= 0) {
            break;
        }
        if (got <= 0) {
            err = ESP_FAIL;
            std::snprintf(message, message_size, "receive failed after %d bytes", total);
            break;
        }
        err = esp_ota_write(handle, chunk, static_cast<std::size_t>(got));
        total += got;
    }
    if (err == ESP_OK) {
        err = esp_ota_end(handle);
        handle = 0;
    }
    if (err != ESP_OK) {
        if (handle != 0) {
            esp_ota_abort(handle);
        }
        if (message[0] == '\0') {
            std::snprintf(message, message_size, "ota write failed: %s", esp_err_to_name(err));
        }
        return err;
    }
    err = esp_ota_set_boot_partition(destination);
    if (err != ESP_OK) {
        std::snprintf(message, message_size, "set boot partition failed: %s", esp_err_to_name(err));
        return err;
    }
    std::snprintf(message, message_size, "update accepted version=%s partition=%s rebooting",
                  prefix.version, destination->label);
    ESP_LOGI(tag, "%s", message);
    return ESP_OK;
}

} // namespace

bool ota_support_init() noexcept {
    update_lock = xSemaphoreCreateMutex();
    const esp_partition_t* partition = running_partition();
    if (partition == nullptr) {
        running_state = ota::ImageState::unknown;
        return update_lock != nullptr;
    }
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const esp_err_t err = esp_ota_get_state_partition(partition, &state);
    if (err != ESP_OK) {
        ESP_LOGW(tag, "ota state unavailable err=0x%x (%s)", static_cast<unsigned>(err),
                 esp_err_to_name(err));
        running_state = ota::ImageState::valid;
    } else {
        running_state = map_image_state(state);
    }
    return update_lock != nullptr;
}

ota::ImageState ota_running_state() noexcept {
    return running_state;
}

const char* ota_running_version() noexcept {
    const esp_app_desc_t* description = esp_app_get_description();
    return description != nullptr ? description->version : "unknown";
}

const char* ota_partition_label() noexcept {
    const esp_partition_t* partition = running_partition();
    return partition != nullptr && partition->label[0] != '\0' ? partition->label : "unknown";
}

const char* ota_image_state_name(ota::ImageState state) noexcept {
    switch (state) {
    case ota::ImageState::valid:
        return "valid";
    case ota::ImageState::pending_verify:
        return "pending_verify";
    case ota::ImageState::invalid:
        return "invalid";
    case ota::ImageState::aborted:
        return "aborted";
    case ota::ImageState::new_image:
        return "new";
    default:
        return "unknown";
    }
}

bool ota_is_busy() noexcept {
    return busy.load();
}

void ota_confirm_or_rollback(bool wifi_connected) noexcept {
    ota::BootConfirmInput input{};
    input.running_state = running_state;
    input.wifi_connected = wifi_connected;
    input.self_check_ready = true;
    input.self_check_passed = wifi_connected;
    const ota::ConfirmDecision decision = ota::decide_boot_confirm(input);
    if (decision == ota::ConfirmDecision::none) {
        led_phase.store(wifi_connected ? LedPhase::ota_ok : LedPhase::ota_failed);
        return;
    }
    if (decision == ota::ConfirmDecision::confirm) {
        const esp_err_t marked = esp_ota_mark_app_valid_cancel_rollback();
        if (marked != ESP_OK) {
            ESP_LOGE(tag, "mark valid failed err=0x%x (%s)", static_cast<unsigned>(marked),
                     esp_err_to_name(marked));
            led_phase.store(LedPhase::ota_failed);
            return;
        }
        running_state = ota::ImageState::valid;
        led_phase.store(LedPhase::ota_ok);
        ESP_LOGI(tag, "image confirmed version=%s partition=%s", ota_running_version(),
                 ota_partition_label());
        return;
    }

    led_phase.store(LedPhase::ota_failed);
    ESP_LOGE(tag, "image refused version=%s partition=%s wifi=%d", ota_running_version(),
             ota_partition_label(), wifi_connected ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_ESPKIT_CAMERA_TIMELAPSE_REFUSE_GRACE_S * 1000));
    const esp_err_t rolled = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE(tag, "rollback failed err=0x%x (%s)", static_cast<unsigned>(rolled),
             esp_err_to_name(rolled));
}

esp_err_t ota_handle_update(httpd_req_t* req) {
    if (update_lock == nullptr || xSemaphoreTake(update_lock, 0) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "update already in progress");
        return ESP_FAIL;
    }
    busy.store(true);
    led_phase.store(LedPhase::ota_updating);
    char message[160]{};
    const esp_err_t err = write_upload(req, message, sizeof(message));
    if (err != ESP_OK) {
        busy.store(false);
        led_phase.store(LedPhase::ota_ok);
        xSemaphoreGive(update_lock);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, message);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/plain");
    (void)httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
    xSemaphoreGive(update_lock);
    schedule_reboot();
    return ESP_OK;
}

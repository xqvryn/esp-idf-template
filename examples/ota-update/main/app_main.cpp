#include "configuration/network_config.hpp"
#include "configuration/nvs_config_store.hpp"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "networking/steady_time.hpp"
#include "networking/wifi_station.hpp"
#include "nvs_flash.h"
#include "ota/ota.hpp"
#include "sdkconfig.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

constexpr char tag[] = "ota-update";
constexpr int kListenPort = 80;
constexpr int kConnectBudgetMs = 90000;
constexpr int kStepDelayMs = 100;
constexpr int kMinPasswordLength = 8;
constexpr int kRebootDelayMs = 1500;
constexpr std::size_t kRecvChunk = 2048;
constexpr std::size_t kUrlCapacity = 256;

enum class LedPhase {
    booting,
    pending,
    updating,
    ok,
    failed
};

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

constexpr Color kYellow{16, 8, 0};
constexpr Color kBlue{0, 0, 16};
constexpr Color kGreen{0, 16, 0};
constexpr Color kRed{16, 0, 0};
constexpr Color kOff{0, 0, 0};

class EspSteadyTime final : public networking::SteadyTime {
    public:
    std::int64_t now() const noexcept override {
        return esp_timer_get_time() / 1000;
    }
};

std::atomic<LedPhase> led_phase{LedPhase::booting};
std::atomic<int> slot_flashes{1};
led_strip_handle_t led_strip = nullptr;
SemaphoreHandle_t update_lock = nullptr;
ota::ImageState running_image_state = ota::ImageState::unknown;

const char* disconnect_reason_name(std::uint8_t reason) {
    switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
        return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
        return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_FAIL:
        return "AUTH_FAIL";
    case WIFI_REASON_NO_AP_FOUND:
        return "NO_AP_FOUND";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
        return "CONNECTION_FAIL";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "BEACON_TIMEOUT";
    default:
        return "UNKNOWN";
    }
}

void log_wifi_failure(std::uint8_t reason) {
    if (reason == 0) {
        ESP_LOGE(tag, "wifi failed reason=timeout");
        return;
    }
    ESP_LOGE(tag, "wifi failed reason=%u/%s", static_cast<unsigned>(reason),
             disconnect_reason_name(reason));
}

esp_err_t init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        const esp_err_t erased = nvs_flash_erase();
        if (erased != ESP_OK) {
            return erased;
        }
        err = nvs_flash_init();
    }
    return err;
}

enum class CredentialSource {
    missing,
    invalid,
    kconfig
};

CredentialSource load_kconfig(configuration::NetworkConfig& config) {
    const char* ssid = CONFIG_ESPKIT_OTA_UPDATE_SSID;
    const char* password = CONFIG_ESPKIT_OTA_UPDATE_PASSWORD;
    if (ssid[0] == '\0') {
        return CredentialSource::missing;
    }
    if (std::strlen(ssid) >= configuration::wifi_ssid_capacity) {
        ESP_LOGE(tag, "wifi failed reason=ssid_too_long");
        return CredentialSource::invalid;
    }
    if (std::strlen(password) >= configuration::wifi_password_capacity) {
        ESP_LOGE(tag, "wifi failed reason=password_too_long");
        return CredentialSource::invalid;
    }
    std::strncpy(config.wifi_ssid, ssid, configuration::wifi_ssid_capacity - 1);
    std::strncpy(config.wifi_password, password, configuration::wifi_password_capacity - 1);
    return CredentialSource::kconfig;
}

const char* running_version() {
    const esp_app_desc_t* description = esp_app_get_description();
    return description != nullptr ? description->version : "unknown";
}

const esp_partition_t* running_partition() {
    return esp_ota_get_running_partition();
}

const char* partition_label() {
    const esp_partition_t* partition = running_partition();
    return partition != nullptr && partition->label[0] != '\0' ? partition->label : "unknown";
}

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

const char* image_state_name(ota::ImageState state) {
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

ota::ImageState read_running_image_state() {
    const esp_partition_t* partition = running_partition();
    if (partition == nullptr) {
        return ota::ImageState::unknown;
    }
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const esp_err_t err = esp_ota_get_state_partition(partition, &state);
    if (err != ESP_OK) {
        // Fresh USB flash often has erased otadata; treat that as settled.
        ESP_LOGW(tag, "ota state unavailable err=0x%x (%s)", static_cast<unsigned>(err),
                 esp_err_to_name(err));
        return ota::ImageState::valid;
    }
    return map_image_state(state);
}

int flashes_for_partition() {
    return std::strcmp(partition_label(), "ota_1") == 0 ? 2 : 1;
}

esp_err_t set_led_color(const Color& color) {
    if (led_strip == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const int count = CONFIG_ESPKIT_OTA_UPDATE_LED_COUNT;
    for (int index = 0; index < count; ++index) {
        const esp_err_t err = led_strip_set_pixel(led_strip, static_cast<std::uint32_t>(index),
                                                  color.red, color.green, color.blue);
        if (err != ESP_OK) {
            return err;
        }
    }
    return led_strip_refresh(led_strip);
}

void flash_slot_count() {
    const int flashes = slot_flashes.load();
    for (int i = 0; i < flashes; ++i) {
        (void)set_led_color(kOff);
        vTaskDelay(pdMS_TO_TICKS(120));
        (void)set_led_color(kGreen);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
}

void led_task(void*) {
    bool lit = true;
    while (true) {
        const LedPhase phase = led_phase.load();
        switch (phase) {
        case LedPhase::booting:
            (void)set_led_color(kYellow);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        case LedPhase::pending:
            (void)set_led_color(lit ? kYellow : kOff);
            lit = !lit;
            vTaskDelay(pdMS_TO_TICKS(300));
            break;
        case LedPhase::updating:
            (void)set_led_color(lit ? kBlue : kOff);
            lit = !lit;
            vTaskDelay(pdMS_TO_TICKS(150));
            break;
        case LedPhase::failed:
            (void)set_led_color(kRed);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        case LedPhase::ok:
            (void)set_led_color(kGreen);
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (led_phase.load() == LedPhase::ok) {
                flash_slot_count();
            }
            break;
        }
    }
}

bool init_led() {
    led_strip_config_t strip_config{};
    strip_config.strip_gpio_num = CONFIG_ESPKIT_OTA_UPDATE_LED_GPIO;
    strip_config.max_leds = static_cast<std::uint32_t>(CONFIG_ESPKIT_OTA_UPDATE_LED_COUNT);
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format.format.r_pos = 1;
    strip_config.color_component_format.format.g_pos = 0;
    strip_config.color_component_format.format.b_pos = 2;
    strip_config.color_component_format.format.w_pos = 3;
    strip_config.color_component_format.format.num_components = 3;
    strip_config.flags.invert_out = false;

    led_strip_rmt_config_t rmt_config{};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    rmt_config.mem_block_symbols = 64;
    rmt_config.flags.with_dma = false;

    const esp_err_t created = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (created != ESP_OK) {
        ESP_LOGE(tag, "led strip init failed err=0x%x (%s)", static_cast<unsigned>(created),
                 esp_err_to_name(created));
        return false;
    }
    slot_flashes.store(flashes_for_partition());
    if (xTaskCreate(led_task, "ota-led", 2048, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(tag, "led task failed");
        return false;
    }
    return true;
}

bool connect_wifi(networking::WifiStation& station, const configuration::NetworkConfig& config,
                  const EspSteadyTime& clock) {
    ESP_LOGI(tag, "wifi connecting ssid=%s", config.wifi_ssid);
    if (!station.start(config)) {
        ESP_LOGE(tag, "wifi failed reason=start");
        return false;
    }
    // Incoming HTTP fails if the station stays in modem sleep.
    const esp_err_t ps = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps != ESP_OK) {
        ESP_LOGW(tag, "wifi ps disable failed err=0x%x (%s)", static_cast<unsigned>(ps),
                 esp_err_to_name(ps));
    }

    std::uint32_t seen = 0;
    const std::int64_t deadline = clock.now() + kConnectBudgetMs;
    while (!station.connected() && clock.now() < deadline) {
        station.step();
        const std::uint32_t disconnects = station.disconnect_count();
        if (disconnects > seen) {
            seen = disconnects;
            log_wifi_failure(station.last_disconnect_reason());
        }
        vTaskDelay(pdMS_TO_TICKS(kStepDelayMs));
    }
    if (!station.connected()) {
        log_wifi_failure(station.last_disconnect_reason());
        return false;
    }

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip{};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(tag, "wifi connected but ip unavailable");
        return false;
    }
    wifi_ap_record_t access_point{};
    int rssi = 0;
    unsigned channel = 0;
    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        rssi = access_point.rssi;
        channel = access_point.primary;
    }
    ESP_LOGI(tag, "wifi connected ip=" IPSTR " rssi=%d channel=%u", IP2STR(&ip.ip), rssi, channel);
    return true;
}

void log_urls() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip{};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        return;
    }
    char addr[16];
    std::snprintf(addr, sizeof(addr), IPSTR, IP2STR(&ip.ip));
    ESP_LOGI(tag, "ota url=http://%s/", addr);
    ESP_LOGI(tag, "status url=http://%s/status", addr);
}

void fill_status_json(char* buffer, std::size_t size) {
    std::snprintf(buffer, size, "{\"version\":\"%s\",\"partition\":\"%s\",\"state\":\"%s\"}",
                  running_version(), partition_label(), image_state_name(running_image_state));
}

esp_err_t handle_status(httpd_req_t* req) {
    char body[192];
    fill_status_json(body, sizeof(body));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_index(httpd_req_t* req) {
    static const char page[] =
        "<!doctype html><html><head><meta charset='utf-8'><title>ESPKit OTA</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;margin:1rem;max-width:40rem}"
        "code{background:#eee;padding:.1rem .3rem}</style></head><body>"
        "<h1>ESPKit OTA update</h1>"
        "<p>Running <code>%s</code> from partition <code>%s</code> (state %s).</p>"
        "<p>LED: yellow = booting or waiting to confirm, blue = writing an image, "
        "green = confirmed (one blink is ota_0, two blinks is ota_1), red = this "
        "image failed its self-check and will roll back.</p>"
        "<form id='upload'><p><input type='file' name='firmware' required></p>"
        "<p><button type='submit'>Upload firmware</button></p></form>"
        "<form method='post' action='/pull'><p>"
        "<input name='url' placeholder='https://.../firmware.bin' size='40'>"
        "<button type='submit'>Pull from URL</button></p>"
        "<p>Plain HTTP pulls stay disabled unless this image was built with the "
        "LAN-testing option.</p></form>"
        "<script>document.getElementById('upload').addEventListener('submit',async(e)=>{"
        "e.preventDefault();const f=e.target.firmware.files[0];if(!f)return;"
        "const r=await fetch('/update',{method:'POST',"
        "headers:{'Content-Type':'application/octet-stream'},body:f});"
        "alert(await r.text());});</script></body></html>";
    char body[2048];
    const int written = std::snprintf(body, sizeof(body), page, running_version(),
                                      partition_label(), image_state_name(running_image_state));
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "page too large");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body, written);
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
    int remaining = content_len > 0 ? content_len : static_cast<int>(kRecvChunk);
    while (received < static_cast<int>(ota::app_image_prefix_size) && remaining != 0) {
        const int want = static_cast<int>(
            std::min(kRecvChunk, ota::app_image_prefix_size - static_cast<std::size_t>(received)));
        const int got = httpd_req_recv(req, reinterpret_cast<char*>(chunk) + received, want);
        if (got <= 0) {
            std::snprintf(message, message_size, "receive failed");
            return ESP_FAIL;
        }
        received += got;
        if (content_len > 0) {
            remaining -= got;
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

esp_err_t handle_update(httpd_req_t* req) {
    if (xSemaphoreTake(update_lock, 0) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "update already in progress");
        return ESP_FAIL;
    }
    led_phase.store(LedPhase::updating);
    char message[160]{};
    const esp_err_t err = write_upload(req, message, sizeof(message));
    if (err != ESP_OK) {
        led_phase.store(LedPhase::ok);
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

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

void percent_decode(char* text) {
    char* output = text;
    for (const char* input = text; *input != '\0'; ++input) {
        if (input[0] == '%' && input[1] != '\0' && input[2] != '\0') {
            const int high = hex_value(input[1]);
            const int low = hex_value(input[2]);
            if (high >= 0 && low >= 0) {
                *output++ = static_cast<char>((high << 4) | low);
                input += 2;
                continue;
            }
        }
        *output++ = *input == '+' ? ' ' : *input;
    }
    *output = '\0';
}

bool url_is_http(const char* url) {
    return std::strncmp(url, "http://", 7) == 0;
}

bool url_is_https(const char* url) {
    return std::strncmp(url, "https://", 8) == 0;
}

esp_err_t pull_image(const char* url, char* message, std::size_t message_size) {
    if (!url_is_http(url) && !url_is_https(url)) {
        std::snprintf(message, message_size, "url must start with http:// or https://");
        return ESP_FAIL;
    }
    if (url_is_http(url)) {
#if CONFIG_ESPKIT_OTA_UPDATE_ALLOW_HTTP
        ESP_LOGW(tag, "pulling firmware over plaintext HTTP");
#else
        std::snprintf(message, message_size, "plaintext HTTP pull is disabled");
        return ESP_FAIL;
#endif
    }

    esp_http_client_config_t http_config{};
    http_config.url = url;
    http_config.timeout_ms = 15000;
    if (url_is_https(url)) {
        http_config.crt_bundle_attach = esp_crt_bundle_attach;
    }
    esp_https_ota_config_t ota_config{};
    ota_config.http_config = &http_config;
    const esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        std::snprintf(message, message_size, "https ota failed: %s", esp_err_to_name(err));
        return err;
    }
    std::snprintf(message, message_size, "pull accepted rebooting");
    ESP_LOGI(tag, "%s", message);
    return ESP_OK;
}

esp_err_t handle_pull(httpd_req_t* req) {
    if (xSemaphoreTake(update_lock, 0) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "update already in progress");
        return ESP_FAIL;
    }
    char body[kUrlCapacity + 16]{};
    const int got = httpd_req_recv(req, body, sizeof(body) - 1);
    if (got <= 0) {
        xSemaphoreGive(update_lock);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing url");
        return ESP_FAIL;
    }
    char url[kUrlCapacity]{};
    if (httpd_query_key_value(body, "url", url, sizeof(url)) != ESP_OK) {
        // Also accept a raw URL as the body.
        if (url_is_http(body) || url_is_https(body)) {
            std::strncpy(url, body, sizeof(url) - 1);
        } else {
            xSemaphoreGive(update_lock);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing url");
            return ESP_FAIL;
        }
    }
    percent_decode(url);
    led_phase.store(LedPhase::updating);
    char message[160]{};
    const esp_err_t err = pull_image(url, message, sizeof(message));
    if (err != ESP_OK) {
        led_phase.store(LedPhase::ok);
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

bool start_httpd() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kListenPort;
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(tag, "httpd start failed");
        return false;
    }
    const httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = handle_index, .user_ctx = nullptr};
    const httpd_uri_t status_uri = {
        .uri = "/status", .method = HTTP_GET, .handler = handle_status, .user_ctx = nullptr};
    const httpd_uri_t update_uri = {
        .uri = "/update", .method = HTTP_POST, .handler = handle_update, .user_ctx = nullptr};
    const httpd_uri_t pull_uri = {
        .uri = "/pull", .method = HTTP_POST, .handler = handle_pull, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &update_uri);
    httpd_register_uri_handler(server, &pull_uri);
    ESP_LOGI(tag, "httpd listening on port %d", kListenPort);
    return true;
}

void confirm_or_rollback(bool wifi_connected) {
    const bool fail_requested = CONFIG_ESPKIT_OTA_UPDATE_FAIL_SELF_CHECK;
    ota::BootConfirmInput input{};
    input.running_state = running_image_state;
    input.wifi_connected = wifi_connected;
    input.self_check_ready = true;
    input.self_check_passed = wifi_connected && !fail_requested;
    const ota::ConfirmDecision decision = ota::decide_boot_confirm(input);
    if (decision == ota::ConfirmDecision::none) {
        led_phase.store(wifi_connected ? LedPhase::ok : LedPhase::failed);
        return;
    }
    if (decision == ota::ConfirmDecision::confirm) {
        const esp_err_t marked = esp_ota_mark_app_valid_cancel_rollback();
        if (marked != ESP_OK) {
            ESP_LOGE(tag, "mark valid failed err=0x%x (%s)", static_cast<unsigned>(marked),
                     esp_err_to_name(marked));
            led_phase.store(LedPhase::failed);
            return;
        }
        running_image_state = ota::ImageState::valid;
        led_phase.store(LedPhase::ok);
        ESP_LOGI(tag, "image confirmed version=%s partition=%s", running_version(),
                 partition_label());
        return;
    }

    led_phase.store(LedPhase::failed);
    ESP_LOGE(tag, "image refused version=%s partition=%s wifi=%d fail_self_check=%d",
             running_version(), partition_label(), wifi_connected ? 1 : 0, fail_requested ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_ESPKIT_OTA_UPDATE_REFUSE_GRACE_S * 1000));
    const esp_err_t rolled = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE(tag, "rollback failed err=0x%x (%s)", static_cast<unsigned>(rolled),
             esp_err_to_name(rolled));
}

} // namespace

extern "C" void app_main() {
    const esp_err_t nvs = init_nvs();
    if (nvs != ESP_OK) {
        ESP_LOGE(tag, "nvs init failed err=0x%x (%s)", static_cast<unsigned>(nvs),
                 esp_err_to_name(nvs));
        return;
    }

    running_image_state = read_running_image_state();
    ESP_LOGI(tag, "running version=%s partition=%s state=%s", running_version(), partition_label(),
             image_state_name(running_image_state));

    if (!init_led()) {
        return;
    }

    configuration::NetworkConfig config{};
    configuration::NvsConfigStore store;
    const CredentialSource source = load_kconfig(config);
    if (source == CredentialSource::invalid) {
        led_phase.store(LedPhase::failed);
        confirm_or_rollback(false);
        return;
    }
    if (source == CredentialSource::missing && !store.load_wifi(config)) {
        ESP_LOGW(tag, "wifi not configured");
        led_phase.store(LedPhase::failed);
        confirm_or_rollback(false);
        return;
    }
    if (std::strlen(config.wifi_password) < kMinPasswordLength) {
        ESP_LOGE(tag, "wifi failed reason=password_too_short");
        led_phase.store(LedPhase::failed);
        confirm_or_rollback(false);
        return;
    }

    const bool pending = running_image_state == ota::ImageState::pending_verify ||
                         running_image_state == ota::ImageState::new_image;
    led_phase.store(pending ? LedPhase::pending : LedPhase::booting);

    update_lock = xSemaphoreCreateMutex();
    if (update_lock == nullptr) {
        ESP_LOGE(tag, "update lock failed");
        confirm_or_rollback(false);
        return;
    }

    EspSteadyTime clock;
    networking::WifiStation station{clock};
    const bool wifi_ok = connect_wifi(station, config, clock);
    if (wifi_ok && !start_httpd()) {
        confirm_or_rollback(false);
        return;
    }
    if (wifi_ok) {
        log_urls();
    }
    confirm_or_rollback(wifi_ok);
}

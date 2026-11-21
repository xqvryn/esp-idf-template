#include "sdkconfig.h"

#if CONFIG_ESPKIT_QEMU_HARNESS

#include "application/command_envelope.hpp"
#include "application/command_path.hpp"
#include "configuration/nvs_config_store.hpp"
#include "configuration/settings_migration.hpp"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "drivers/simulated_output.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "qemu_harness.hpp"
#include "timed_output/timed_output.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>

namespace qemu_harness {

namespace {

constexpr char tag[] = "espkit";
constexpr char nvs_namespace[] = "espkit";

Context* active = nullptr;
std::uint64_t next_id = 1;
std::uint32_t next_sequence = 1;

const char* status_name(domain::CommandStatus status) noexcept {
    switch (status) {
    case domain::CommandStatus::accepted:
        return "accepted";
    case domain::CommandStatus::rejected_invalid:
        return "rejected_invalid";
    case domain::CommandStatus::rejected_expired:
        return "rejected_expired";
    case domain::CommandStatus::rejected_duplicate:
        return "rejected_duplicate";
    case domain::CommandStatus::rejected_stale_sequence:
        return "rejected_stale_sequence";
    case domain::CommandStatus::rejected_unauthorized:
        return "rejected_unauthorized";
    case domain::CommandStatus::rejected_full:
        return "rejected_full";
    case domain::CommandStatus::applied:
        return "applied";
    case domain::CommandStatus::not_applied:
        return "not_applied";
    case domain::CommandStatus::dropped_by_stop:
        return "dropped_by_stop";
    case domain::CommandStatus::expired_before_apply:
        return "expired_before_apply";
    case domain::CommandStatus::superseded:
        return "superseded";
    }
    return "unknown";
}

const char* reason_name(application::CommandEnvelopeReason reason) noexcept {
    switch (reason) {
    case application::CommandEnvelopeReason::submitted:
        return "submitted";
    case application::CommandEnvelopeReason::rejected_oversize:
        return "oversize";
    case application::CommandEnvelopeReason::rejected_malformed:
        return "malformed";
    case application::CommandEnvelopeReason::rejected_unknown_version:
        return "unknown_version";
    case application::CommandEnvelopeReason::rejected_expired:
        return "expired";
    }
    return "unknown";
}

const char* state_name(timed_output::OutputState state) noexcept {
    switch (state) {
    case timed_output::OutputState::off:
        return "off";
    case timed_output::OutputState::running:
        return "running";
    case timed_output::OutputState::fault:
        return "fault";
    }
    return "unknown";
}

void reply_status() {
    const auto reading = active->sensor.read(active->clock.now());
    ESP_LOGI(tag, "output=%d fault=%d state=%s value=%.1f valid=%d uptime=%lld",
             active->output.enabled(), active->policy.fault(), state_name(active->policy.state()),
             static_cast<double>(reading.value), reading.valid,
             static_cast<long long>(active->clock.now()));
}

void submit_output(bool enabled, domain::Milliseconds duration) {
    const auto now = active->clock.now();
    if (now > std::numeric_limits<domain::Milliseconds>::max() - 10000) {
        ESP_LOGI(tag, "command status=rejected_invalid now=%lld", static_cast<long long>(now));
        return;
    }
    domain::Command command{};
    command.id = next_id++;
    command.sequence = next_sequence++;
    command.identity = 7;
    command.type = domain::CommandType::set_output;
    command.source = domain::CommandSource::local;
    command.enabled = enabled;
    command.expires_at = now + 10000;
    command.max_duration_ms = duration;
    const auto status = active->ingress.submit(command, now);
    ESP_LOGI(tag, "command status=%s now=%lld", status_name(status), static_cast<long long>(now));
}

bool write_unversioned_nvs() {
    if (nvs_flash_init() != ESP_OK) {
        return false;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_namespace, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    const auto erased = nvs_erase_key(handle, "schema");
    const bool wrote = (erased == ESP_OK || erased == ESP_ERR_NVS_NOT_FOUND) &&
                       nvs_set_str(handle, "wifi_ssid", "qemu-net") == ESP_OK &&
                       nvs_set_str(handle, "wifi_pass", "qemu-pass") == ESP_OK &&
                       nvs_set_str(handle, "mqtt_uri", "mqtts://broker.example") == ESP_OK &&
                       nvs_set_str(handle, "device_id", "qemu-device") == ESP_OK &&
                       nvs_set_str(handle, "site_id", "qemu-site") == ESP_OK &&
                       nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return wrote;
}

void report_nvs() {
    if (nvs_flash_init() != ESP_OK) {
        ESP_LOGI(tag, "nvs rejected");
        return;
    }
    configuration::NetworkConfig config{};
    configuration::NvsConfigStore store;
    if (!store.load(config, false)) {
        ESP_LOGI(tag, "nvs rejected");
        return;
    }
    // load() already logged the migration outcome. Do not rename that result here.
    ESP_LOGI(tag, "nvs ssid=%s", config.wifi_ssid);
}

void handle_envelope(std::string_view payload) {
    const auto now = active->clock.now();
    const auto decoded =
        application::decode_command_envelope(payload, now, 0, 7, next_sequence, active->ingress);
    if (decoded.reason == application::CommandEnvelopeReason::submitted) {
        if (decoded.admission == domain::CommandStatus::accepted) {
            ++next_sequence;
        }
        ESP_LOGI(tag, "envelope reason=submitted admission=%s", status_name(decoded.admission));
        return;
    }
    ESP_LOGI(tag, "envelope reason=%s", reason_name(decoded.reason));
}

void handle_line(char* line) {
    while (*line == ' ') {
        ++line;
    }
    if (std::strncmp(line, "espkit", 6) != 0) {
        return;
    }
    char* cursor = line + 6;
    while (*cursor == ' ') {
        ++cursor;
    }
    if (std::strcmp(cursor, "ping") == 0) {
        ESP_LOGI(tag, "pong");
    } else if (std::strcmp(cursor, "status") == 0) {
        reply_status();
    } else if (std::strcmp(cursor, "off") == 0) {
        submit_output(false, 0);
    } else if (std::strncmp(cursor, "on ", 3) == 0) {
        char* end = nullptr;
        const long duration = std::strtol(cursor + 3, &end, 10);
        if (end == cursor + 3 || duration <= 0) {
            ESP_LOGI(tag, "command status=rejected_invalid");
        } else {
            submit_output(true, duration);
        }
    } else if (std::strcmp(cursor, "sensor fail") == 0) {
        active->sensor.fail();
        ESP_LOGI(tag, "sensor valid=0");
    } else if (std::strncmp(cursor, "sensor ", 7) == 0) {
        char* end = nullptr;
        const float value = std::strtof(cursor + 7, &end);
        if (end == cursor + 7) {
            ESP_LOGI(tag, "error sensor");
        } else {
            active->sensor.set_value(value);
            ESP_LOGI(tag, "sensor valid=1 value=%.1f", static_cast<double>(value));
        }
    } else if (std::strncmp(cursor, "envelope ", 9) == 0) {
        handle_envelope(cursor + 9);
    } else if (std::strcmp(cursor, "nvs-unversioned") == 0) {
        if (write_unversioned_nvs()) {
            ESP_LOGI(tag, "nvs-written");
        } else {
            ESP_LOGI(tag, "nvs rejected");
        }
    } else if (std::strcmp(cursor, "restart") == 0) {
        esp_restart();
    } else if (std::strcmp(cursor, "panic") == 0) {
        esp_system_abort("qemu harness panic");
    } else {
        ESP_LOGI(tag, "error unknown");
    }
}

void reader(void*) {
    char line[700];
    for (;;) {
        if (std::fgets(line, sizeof(line), stdin) == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        line[std::strcspn(line, "\r\n")] = '\0';
        handle_line(line);
    }
}

} // namespace

void start(const Context& context) {
    static Context stored = context;
    active = &stored;
    const uart_port_t port = static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM);
    if (uart_driver_install(port, 1024, 0, 0, nullptr, 0) == ESP_OK) {
        uart_vfs_dev_use_driver(port);
    }
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    if (xTaskCreate(reader, "qemu-harness", 6144, nullptr, 1, nullptr) != pdPASS) {
        std::abort();
    }
    report_nvs();
    ESP_LOGI(tag, "ready");
}

} // namespace qemu_harness

#endif

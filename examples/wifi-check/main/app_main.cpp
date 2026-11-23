#include "configuration/network_config.hpp"
#include "configuration/nvs_config_store.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "networking/sntp_time.hpp"
#include "networking/steady_time.hpp"
#include "networking/wifi_station.hpp"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <cstdint>
#include <cstring>
#include <ctime>

namespace {

constexpr char tag[] = "wifi-check";
constexpr int step_delay_ms = 100;
constexpr int connect_budget_ms = 90000;
constexpr int sntp_budget_ms = 15000;
constexpr int min_password_length = 8;

class EspSteadyTime final : public networking::SteadyTime {
    public:
    std::int64_t now() const noexcept override {
        return esp_timer_get_time() / 1000;
    }
};

const char* disconnect_reason_name(std::uint8_t reason) {
    switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
        return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
        return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
        return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:
        return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:
        return "ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED:
        return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED:
        return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE:
        return "ASSOC_LEAVE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
        return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
        return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
        return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
        return "CONNECTION_FAIL";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        return "NO_AP_FOUND_W_COMPATIBLE_SECURITY";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return "NO_AP_FOUND_IN_AUTHMODE_THRESHOLD";
    default:
        return "UNKNOWN";
    }
}

void log_failure(std::uint8_t reason) {
    if (reason == 0) {
        ESP_LOGE(tag, "wifi failed reason=timeout");
        return;
    }
    ESP_LOGE(tag, "wifi failed reason=%u/%s", static_cast<unsigned>(reason),
             disconnect_reason_name(reason));
}

enum class CredentialSource {
    missing,
    invalid,
    kconfig
};

CredentialSource load_kconfig(configuration::NetworkConfig& config) {
    const char* ssid = CONFIG_ESPKIT_WIFI_CHECK_SSID;
    const char* password = CONFIG_ESPKIT_WIFI_CHECK_PASSWORD;
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

void log_connected() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip{};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(tag, "wifi connected but ip unavailable");
        return;
    }
    wifi_ap_record_t access_point{};
    int rssi = 0;
    unsigned channel = 0;
    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        rssi = access_point.rssi;
        channel = access_point.primary;
    }
    ESP_LOGI(tag, "wifi connected ip=" IPSTR " rssi=%d channel=%u", IP2STR(&ip.ip), rssi, channel);
}

void sync_time(const EspSteadyTime& clock) {
#if CONFIG_ESPKIT_WIFI_CHECK_SNTP
    networking::SntpTime time;
    time.begin();
    const std::int64_t deadline = clock.now() + sntp_budget_ms;
    while (!time.ready() && clock.now() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }
    if (!time.ready()) {
        ESP_LOGW(tag, "wifi time sync failed");
        return;
    }
    ESP_LOGI(tag, "wifi time synced unix=%ld", static_cast<long>(std::time(nullptr)));
#else
    (void)clock;
#endif
}

} // namespace

extern "C" void app_main() {
    const esp_err_t nvs = init_nvs();
    if (nvs != ESP_OK) {
        ESP_LOGE(tag, "nvs init failed err=0x%x (%s)", static_cast<unsigned>(nvs),
                 esp_err_to_name(nvs));
        return;
    }

    configuration::NetworkConfig config{};
    configuration::NvsConfigStore store;
    const CredentialSource source = load_kconfig(config);
    if (source == CredentialSource::invalid) {
        return;
    }
    const bool from_kconfig = source == CredentialSource::kconfig;
    if (!from_kconfig && !store.load_wifi(config)) {
        ESP_LOGW(tag, "wifi not configured");
        return;
    }
    if (std::strlen(config.wifi_password) < min_password_length) {
        ESP_LOGE(tag, "wifi failed reason=password_too_short");
        return;
    }

    ESP_LOGI(tag, "wifi connecting ssid=%s", config.wifi_ssid);

    EspSteadyTime clock;
    networking::WifiStation station{clock};
    if (!station.start(config)) {
        ESP_LOGE(tag, "wifi failed reason=start");
        return;
    }

    std::uint32_t seen_disconnects = 0;
    const std::int64_t deadline = clock.now() + connect_budget_ms;
    while (!station.connected() && clock.now() < deadline) {
        station.step();
        const std::uint32_t disconnects = station.disconnect_count();
        if (disconnects > seen_disconnects) {
            seen_disconnects = disconnects;
            log_failure(station.last_disconnect_reason());
        }
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    if (!station.connected()) {
        log_failure(station.last_disconnect_reason());
        station.stop();
        return;
    }

    log_connected();

#if CONFIG_ESPKIT_WIFI_CHECK_SAVE_TO_NVS
    if (from_kconfig) {
        if (store.save_wifi(config)) {
            ESP_LOGI(tag, "wifi credentials saved to nvs");
        } else {
            ESP_LOGW(tag, "wifi nvs save failed");
        }
    }
#endif

    sync_time(clock);
}

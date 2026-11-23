#include "configuration/network_config.hpp"
#include "configuration/nvs_config_store.hpp"
#include "esp_camera.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "networking/steady_time.hpp"
#include "networking/wifi_station.hpp"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr char tag[] = "camera-stream";
constexpr int kListenPort = 80;
constexpr int kConnectBudgetMs = 90000;
constexpr int kStepDelayMs = 100;
constexpr int kWarmupFrames = 8;
constexpr int kMinPasswordLength = 8;

SemaphoreHandle_t camera_lock;

class EspSteadyTime final : public networking::SteadyTime {
    public:
    std::int64_t now() const noexcept override {
        return esp_timer_get_time() / 1000;
    }
};

enum class CredentialSource {
    missing,
    invalid,
    kconfig,
    nvs
};

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

CredentialSource load_kconfig(configuration::NetworkConfig& config) {
    const char* ssid = CONFIG_ESPKIT_CAMERA_STREAM_SSID;
    const char* password = CONFIG_ESPKIT_CAMERA_STREAM_PASSWORD;
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

void apply_pins(camera_config_t& config) {
#if CONFIG_ESPKIT_CAMERA_STREAM_PRESET_CUSTOM
    config.pin_pwdn = CONFIG_ESPKIT_CAMERA_STREAM_PIN_PWDN;
    config.pin_reset = CONFIG_ESPKIT_CAMERA_STREAM_PIN_RESET;
    config.pin_xclk = CONFIG_ESPKIT_CAMERA_STREAM_PIN_XCLK;
    config.pin_sccb_sda = CONFIG_ESPKIT_CAMERA_STREAM_PIN_SIOD;
    config.pin_sccb_scl = CONFIG_ESPKIT_CAMERA_STREAM_PIN_SIOC;
    config.pin_d0 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D0;
    config.pin_d1 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D1;
    config.pin_d2 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D2;
    config.pin_d3 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D3;
    config.pin_d4 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D4;
    config.pin_d5 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D5;
    config.pin_d6 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D6;
    config.pin_d7 = CONFIG_ESPKIT_CAMERA_STREAM_PIN_D7;
    config.pin_vsync = CONFIG_ESPKIT_CAMERA_STREAM_PIN_VSYNC;
    config.pin_href = CONFIG_ESPKIT_CAMERA_STREAM_PIN_HREF;
    config.pin_pclk = CONFIG_ESPKIT_CAMERA_STREAM_PIN_PCLK;
#else
    // ESP32-S3-EYE / common ESP32-S3-WROOM CAM pin map.
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = 15;
    config.pin_sccb_sda = 4;
    config.pin_sccb_scl = 5;
    config.pin_d0 = 11;
    config.pin_d1 = 9;
    config.pin_d2 = 8;
    config.pin_d3 = 10;
    config.pin_d4 = 12;
    config.pin_d5 = 18;
    config.pin_d6 = 17;
    config.pin_d7 = 16;
    config.pin_vsync = 6;
    config.pin_href = 7;
    config.pin_pclk = 13;
#endif
}

framesize_t frame_size() {
#if CONFIG_ESPKIT_CAMERA_STREAM_FRAMESIZE_QVGA
    return FRAMESIZE_QVGA;
#else
    return FRAMESIZE_VGA;
#endif
}

const char* sensor_name(sensor_t* sensor) {
    if (sensor == nullptr) {
        return "unknown";
    }
    camera_sensor_info_t* info = esp_camera_sensor_get_info(&sensor->id);
    if (info != nullptr && info->name != nullptr) {
        return info->name;
    }
    return "unknown";
}

bool init_camera() {
    camera_config_t config{};
    apply_pins(config);
    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = frame_size();
    config.jpeg_quality = CONFIG_ESPKIT_CAMERA_STREAM_JPEG_QUALITY;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    const esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "camera init failed err=0x%x (%s)", static_cast<unsigned>(err),
                 esp_err_to_name(err));
        return false;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor == nullptr) {
        ESP_LOGE(tag, "camera sensor missing after init");
        return false;
    }
    ESP_LOGI(tag, "camera sensor pid=0x%x model=%s", static_cast<unsigned>(sensor->id.PID),
             sensor_name(sensor));
    for (int i = 0; i < kWarmupFrames; ++i) {
        camera_fb_t* frame = esp_camera_fb_get();
        if (frame != nullptr) {
            esp_camera_fb_return(frame);
        }
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
    ESP_LOGI(tag, "stream url=http://%s/stream", addr);
    ESP_LOGI(tag, "capture url=http://%s/capture", addr);
    ESP_LOGI(tag, "index url=http://%s/", addr);
}

esp_err_t handle_index(httpd_req_t* req) {
    static const char page[] =
        "<!doctype html><html><head><meta charset='utf-8'><title>ESP32-S3 camera</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;margin:1rem}img{max-width:100%;height:auto}</style>"
        "</head><body><h1>ESP32-S3 camera</h1>"
        "<p><a href='/stream'>/stream</a> &middot; <a href='/capture'>/capture</a></p>"
        "<img src='/stream' alt='live stream'></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_capture(httpd_req_t* req) {
    if (xSemaphoreTake(camera_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera busy");
        return ESP_FAIL;
    }
    camera_fb_t* frame = esp_camera_fb_get();
    if (frame == nullptr) {
        xSemaphoreGive(camera_lock);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    const esp_err_t sent = httpd_resp_send(req, reinterpret_cast<const char*>(frame->buf),
                                           static_cast<ssize_t>(frame->len));
    esp_camera_fb_return(frame);
    xSemaphoreGive(camera_lock);
    return sent;
}

esp_err_t handle_stream(httpd_req_t* req) {
    static const char* boundary = "frame";
    char type[80];
    std::snprintf(type, sizeof(type), "multipart/x-mixed-replace;boundary=%s", boundary);
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    char header[128];
    while (true) {
        if (xSemaphoreTake(camera_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
            continue;
        }
        camera_fb_t* frame = esp_camera_fb_get();
        if (frame == nullptr) {
            xSemaphoreGive(camera_lock);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const int header_len =
            std::snprintf(header, sizeof(header),
                          "--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                          boundary, static_cast<unsigned>(frame->len));
        esp_err_t err = httpd_resp_send_chunk(req, header, header_len);
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(frame->buf),
                                        static_cast<ssize_t>(frame->len));
        }
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, "\r\n", 2);
        }
        esp_camera_fb_return(frame);
        xSemaphoreGive(camera_lock);
        if (err != ESP_OK) {
            return err;
        }
    }
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
    const httpd_uri_t capture_uri = {
        .uri = "/capture", .method = HTTP_GET, .handler = handle_capture, .user_ctx = nullptr};
    const httpd_uri_t stream_uri = {
        .uri = "/stream", .method = HTTP_GET, .handler = handle_stream, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &capture_uri);
    httpd_register_uri_handler(server, &stream_uri);
    ESP_LOGI(tag, "httpd listening on port %d", kListenPort);
    return true;
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
    if (source == CredentialSource::missing && !store.load_wifi(config)) {
        ESP_LOGW(tag, "wifi not configured");
        return;
    }
    if (std::strlen(config.wifi_password) < kMinPasswordLength) {
        ESP_LOGE(tag, "wifi failed reason=password_too_short");
        return;
    }

    camera_lock = xSemaphoreCreateMutex();
    if (camera_lock == nullptr) {
        ESP_LOGE(tag, "camera lock failed");
        return;
    }
    if (!init_camera()) {
        return;
    }

    EspSteadyTime clock;
    networking::WifiStation station{clock};
    if (!connect_wifi(station, config, clock)) {
        return;
    }
    if (!start_httpd()) {
        return;
    }
    log_urls();
}

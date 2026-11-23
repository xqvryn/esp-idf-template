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
#include "image_motion/detector.hpp"
#include "img_converters.h"
#include "led_status.hpp"
#include "led_strip.h"
#include "networking/steady_time.hpp"
#include "networking/wifi_station.hpp"
#include "nvs_flash.h"
#include "ota_http.hpp"
#include "photo_store.hpp"
#include "sdkconfig.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

namespace {

constexpr char tag[] = "camera-timelapse";
constexpr int kListenPort = 80;
constexpr int kConnectBudgetMs = 90000;
constexpr int kStepDelayMs = 100;
constexpr int kWarmupFrames = 8;
constexpr int kMinPasswordLength = 8;
constexpr int kMotionSettleFrames = 4;

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

constexpr Color kOff{0, 0, 0};
constexpr Color kYellow{16, 8, 0};
constexpr Color kBlue{0, 0, 16};
constexpr Color kGreen{0, 8, 0};
constexpr Color kWhite{12, 12, 12};
constexpr Color kRed{16, 0, 0};

enum class CaptureMode {
    timelapse,
    motion
};

struct RuntimeSettings {
    CaptureMode mode{
#if CONFIG_ESPKIT_CAMERA_TIMELAPSE_MODE_MOTION
        CaptureMode::motion
#else
        CaptureMode::timelapse
#endif
    };
    int interval_s{CONFIG_ESPKIT_CAMERA_TIMELAPSE_INTERVAL_S};
    int motion_period_ms{CONFIG_ESPKIT_CAMERA_TIMELAPSE_MOTION_PERIOD_MS};
    image_motion::MotionConfig motion{};
};

class EspSteadyTime final : public networking::SteadyTime {
    public:
    std::int64_t now() const noexcept override {
        return esp_timer_get_time() / 1000;
    }
};

SemaphoreHandle_t camera_lock = nullptr;
SemaphoreHandle_t settings_lock = nullptr;
PhotoStore photos;
RuntimeSettings settings{};
image_motion::Detector detector{{}};
led_strip_handle_t led_strip = nullptr;
std::atomic<int> motion_settle{kMotionSettleFrames};

const char* mode_name(CaptureMode mode) {
    return mode == CaptureMode::motion ? "motion" : "timelapse";
}

RuntimeSettings load_settings() {
    RuntimeSettings copy = settings;
    if (settings_lock != nullptr) {
        if (xSemaphoreTake(settings_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
            copy = settings;
            xSemaphoreGive(settings_lock);
        }
    }
    return copy;
}

void store_settings(const RuntimeSettings& next) {
    if (settings_lock != nullptr) {
        xSemaphoreTake(settings_lock, portMAX_DELAY);
        settings = next;
        detector.set_config(next.motion);
        xSemaphoreGive(settings_lock);
    } else {
        settings = next;
        detector.set_config(next.motion);
    }
    motion_settle.store(kMotionSettleFrames);
}

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
    const char* ssid = CONFIG_ESPKIT_CAMERA_TIMELAPSE_SSID;
    const char* password = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PASSWORD;
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
#if CONFIG_ESPKIT_CAMERA_TIMELAPSE_PRESET_CUSTOM
    config.pin_pwdn = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_PWDN;
    config.pin_reset = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_RESET;
    config.pin_xclk = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_XCLK;
    config.pin_sccb_sda = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_SIOD;
    config.pin_sccb_scl = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_SIOC;
    config.pin_d0 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D0;
    config.pin_d1 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D1;
    config.pin_d2 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D2;
    config.pin_d3 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D3;
    config.pin_d4 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D4;
    config.pin_d5 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D5;
    config.pin_d6 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D6;
    config.pin_d7 = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_D7;
    config.pin_vsync = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_VSYNC;
    config.pin_href = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_HREF;
    config.pin_pclk = CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIN_PCLK;
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
#if CONFIG_ESPKIT_CAMERA_TIMELAPSE_FRAMESIZE_QVGA
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
    config.jpeg_quality = CONFIG_ESPKIT_CAMERA_TIMELAPSE_JPEG_QUALITY;
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

esp_err_t set_led_color(const Color& color) {
    if (led_strip == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const int count = CONFIG_ESPKIT_CAMERA_TIMELAPSE_LED_COUNT;
    for (int index = 0; index < count; ++index) {
        const esp_err_t err = led_strip_set_pixel(led_strip, static_cast<std::uint32_t>(index),
                                                  color.red, color.green, color.blue);
        if (err != ESP_OK) {
            return err;
        }
    }
    return led_strip_refresh(led_strip);
}

void led_task(void*) {
    bool lit = true;
    while (true) {
        const LedPhase phase = led_phase.load();
        switch (phase) {
        case LedPhase::ota_pending:
            (void)set_led_color(lit ? kYellow : kOff);
            lit = !lit;
            vTaskDelay(pdMS_TO_TICKS(300));
            break;
        case LedPhase::ota_updating:
            (void)set_led_color(lit ? kBlue : kOff);
            lit = !lit;
            vTaskDelay(pdMS_TO_TICKS(150));
            break;
        case LedPhase::ota_failed:
            (void)set_led_color(kRed);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        case LedPhase::capture:
            (void)set_led_color(kWhite);
            vTaskDelay(pdMS_TO_TICKS(180));
            if (led_phase.load() == LedPhase::capture) {
                led_phase.store(LedPhase::idle);
            }
            break;
        case LedPhase::motion:
            (void)set_led_color(kRed);
            vTaskDelay(pdMS_TO_TICKS(350));
            if (led_phase.load() == LedPhase::motion) {
                led_phase.store(LedPhase::idle);
            }
            break;
        case LedPhase::ota_ok:
        case LedPhase::idle:
        default:
            (void)set_led_color(kGreen);
            vTaskDelay(pdMS_TO_TICKS(400));
            break;
        }
    }
}

bool init_led() {
    led_strip_config_t strip_config{};
    strip_config.strip_gpio_num = CONFIG_ESPKIT_CAMERA_TIMELAPSE_LED_GPIO;
    strip_config.max_leds = static_cast<std::uint32_t>(CONFIG_ESPKIT_CAMERA_TIMELAPSE_LED_COUNT);
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
    if (xTaskCreate(led_task, "tl-led", 2048, nullptr, 5, nullptr) != pdPASS) {
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
    ESP_LOGI(tag, "timelapse url=http://%s/", addr);
    ESP_LOGI(tag, "status url=http://%s/status", addr);
}

bool jpeg_to_gray(const std::uint8_t* jpeg, std::size_t length, std::size_t width,
                  std::size_t height, std::vector<std::uint8_t>& gray) {
    if (width < 8 || height < 8) {
        return false;
    }
    const std::size_t out_w = width / 8;
    const std::size_t out_h = height / 8;
    std::vector<std::uint8_t> rgb(out_w * out_h * 2);
    if (!jpg2rgb565(jpeg, length, rgb.data(), JPG_SCALE_8X)) {
        return false;
    }
    gray.resize(out_w * out_h);
    for (std::size_t i = 0; i < gray.size(); ++i) {
        const std::uint16_t pixel = static_cast<std::uint16_t>(
            rgb[i * 2] | (static_cast<std::uint16_t>(rgb[i * 2 + 1]) << 8));
        const int red = (pixel >> 11) << 3;
        const int green = ((pixel >> 5) & 0x3f) << 2;
        const int blue = (pixel & 0x1f) << 3;
        gray[i] = static_cast<std::uint8_t>((red * 77 + green * 150 + blue * 29) >> 8);
    }
    return true;
}

camera_fb_t* take_frame() {
    if (camera_lock == nullptr || xSemaphoreTake(camera_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return nullptr;
    }
    camera_fb_t* frame = esp_camera_fb_get();
    xSemaphoreGive(camera_lock);
    return frame;
}

std::uint32_t store_frame(camera_fb_t* frame, const char* reason, std::uint16_t score,
                          std::int64_t now) {
    if (frame == nullptr) {
        return 0;
    }
    const std::uint32_t id = photos.store(frame->buf, frame->len, reason, score, now);
    if (id != 0) {
        led_phase.store(std::strcmp(reason, "motion") == 0 ? LedPhase::motion : LedPhase::capture);
        ESP_LOGI(tag, "stored id=%u reason=%s bytes=%u score=%u", static_cast<unsigned>(id), reason,
                 static_cast<unsigned>(frame->len), static_cast<unsigned>(score));
    }
    return id;
}

void capture_task(void*) {
    std::int64_t last_timelapse = 0;
    std::int64_t last_motion = 0;
    detector.set_config(load_settings().motion);
    while (true) {
        if (ota_is_busy()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        const RuntimeSettings current = load_settings();
        const std::int64_t now = esp_timer_get_time() / 1000;
        const bool due_timelapse =
            current.mode == CaptureMode::timelapse &&
            (last_timelapse == 0 ||
             now - last_timelapse >= static_cast<std::int64_t>(current.interval_s) * 1000);
        const bool due_motion = current.mode == CaptureMode::motion &&
                                (last_motion == 0 || now - last_motion >= current.motion_period_ms);
        if (!due_timelapse && !due_motion) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        camera_fb_t* frame = take_frame();
        if (frame == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (due_timelapse) {
            (void)store_frame(frame, "timelapse", 0, now);
            last_timelapse = now;
        } else if (due_motion) {
            std::vector<std::uint8_t> gray;
            if (jpeg_to_gray(frame->buf, frame->len, frame->width, frame->height, gray)) {
                const image_motion::MotionResult result =
                    detector.evaluate(gray, frame->width / 8, frame->height / 8, now);
                photos.set_last_motion_score(result.score_permille);
                const int settle = motion_settle.load();
                if (settle > 0) {
                    motion_settle.store(settle - 1);
                } else if (result.decision == image_motion::MotionDecision::triggered) {
                    (void)store_frame(frame, "motion", result.score_permille, now);
                }
            }
            last_motion = now;
        }
        esp_camera_fb_return(frame);
    }
}

void fill_status_json(char* buffer, std::size_t size) {
    const RuntimeSettings current = load_settings();
    std::snprintf(
        buffer, size,
        "{\"version\":\"%s\",\"partition\":\"%s\",\"state\":\"%s\","
        "\"mode\":\"%s\",\"interval_s\":%d,\"motion_period_ms\":%d,"
        "\"pixel_threshold\":%u,\"changed_permille\":%u,\"cooldown_ms\":%ld,"
        "\"photos\":%d,\"capacity\":%d,\"captures\":%u,\"motion_events\":%u,"
        "\"last_motion_score\":%u}",
        ota_running_version(), ota_partition_label(), ota_image_state_name(ota_running_state()),
        mode_name(current.mode), current.interval_s, current.motion_period_ms,
        static_cast<unsigned>(current.motion.pixel_threshold),
        static_cast<unsigned>(current.motion.changed_permille),
        static_cast<long>(current.motion.cooldown_ms), photos.count(), photos.capacity(),
        static_cast<unsigned>(photos.captures()), static_cast<unsigned>(photos.motion_events()),
        static_cast<unsigned>(photos.last_motion_score()));
}

esp_err_t handle_status(httpd_req_t* req) {
    char body[512];
    fill_status_json(body, sizeof(body));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_list(httpd_req_t* req) {
    char body[2048];
    int used = std::snprintf(body, sizeof(body), "{\"photos\":[");
    bool first = true;
    photos.for_each([&](const PhotoInfo& info) {
        if (used < 0 || static_cast<std::size_t>(used) + 96 >= sizeof(body)) {
            return;
        }
        used += std::snprintf(body + used, sizeof(body) - static_cast<std::size_t>(used),
                              "%s{\"id\":%u,\"reason\":\"%s\",\"bytes\":%u,\"score\":%u}",
                              first ? "" : ",", static_cast<unsigned>(info.id), info.reason,
                              static_cast<unsigned>(info.length),
                              static_cast<unsigned>(info.score_permille));
        first = false;
    });
    if (used < 0 || static_cast<std::size_t>(used) + 3 >= sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "list too large");
        return ESP_FAIL;
    }
    used += std::snprintf(body + used, sizeof(body) - static_cast<std::size_t>(used), "]}");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, used);
}

esp_err_t handle_photo(httpd_req_t* req) {
    unsigned id = 0;
    if (std::sscanf(req->uri, "/photo/%u", &id) != 1 || id == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "missing photo");
        return ESP_FAIL;
    }
    esp_err_t sent = ESP_FAIL;
    const bool found = photos.with_photo(
        static_cast<std::uint32_t>(id), [&](const PhotoInfo& info, const std::uint8_t* data) {
            httpd_resp_set_type(req, "image/jpeg");
            sent = httpd_resp_send(req, reinterpret_cast<const char*>(data),
                                   static_cast<ssize_t>(info.length));
        });
    if (!found) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "photo not found");
        return ESP_FAIL;
    }
    return sent;
}

esp_err_t handle_preview(httpd_req_t* req) {
    camera_fb_t* frame = take_frame();
    if (frame == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    const esp_err_t sent = httpd_resp_send(req, reinterpret_cast<const char*>(frame->buf),
                                           static_cast<ssize_t>(frame->len));
    esp_camera_fb_return(frame);
    return sent;
}

esp_err_t handle_capture(httpd_req_t* req) {
    camera_fb_t* frame = take_frame();
    if (frame == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
        return ESP_FAIL;
    }
    const std::uint32_t id = store_frame(frame, "manual", 0, esp_timer_get_time() / 1000);
    httpd_resp_set_type(req, "image/jpeg");
    if (id != 0) {
        char location[32];
        std::snprintf(location, sizeof(location), "/photo/%u", static_cast<unsigned>(id));
        httpd_resp_set_hdr(req, "Location", location);
    }
    const esp_err_t sent = httpd_resp_send(req, reinterpret_cast<const char*>(frame->buf),
                                           static_cast<ssize_t>(frame->len));
    esp_camera_fb_return(frame);
    return sent;
}

bool parse_int_field(const char* body, const char* key, int& value, int lo, int hi) {
    char text[24]{};
    if (httpd_query_key_value(body, key, text, sizeof(text)) != ESP_OK) {
        return true;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || parsed < lo || parsed > hi) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

esp_err_t handle_settings(httpd_req_t* req) {
    char body[512]{};
    const int got = httpd_req_recv(req, body, sizeof(body) - 1);
    if (got <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing settings");
        return ESP_FAIL;
    }
    RuntimeSettings next = load_settings();
    char mode[16]{};
    if (httpd_query_key_value(body, "mode", mode, sizeof(mode)) == ESP_OK) {
        if (std::strcmp(mode, "motion") == 0) {
            next.mode = CaptureMode::motion;
            detector.reset();
        } else if (std::strcmp(mode, "timelapse") == 0) {
            next.mode = CaptureMode::timelapse;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "mode must be timelapse or motion");
            return ESP_FAIL;
        }
    }
    int interval = next.interval_s;
    int period = next.motion_period_ms;
    int threshold = next.motion.pixel_threshold;
    int changed = next.motion.changed_permille;
    int cooldown = static_cast<int>(next.motion.cooldown_ms);
    if (!parse_int_field(body, "interval_s", interval, 1, 3600) ||
        !parse_int_field(body, "motion_period_ms", period, 100, 5000) ||
        !parse_int_field(body, "pixel_threshold", threshold, 1, 64) ||
        !parse_int_field(body, "changed_permille", changed, 1, 500) ||
        !parse_int_field(body, "cooldown_ms", cooldown, 0, 60000)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid setting");
        return ESP_FAIL;
    }
    next.interval_s = interval;
    next.motion_period_ms = period;
    next.motion.pixel_threshold = static_cast<std::uint8_t>(threshold);
    next.motion.changed_permille = static_cast<std::uint16_t>(changed);
    next.motion.cooldown_ms = cooldown;
    store_settings(next);
    ESP_LOGI(tag, "settings mode=%s interval_s=%d threshold=%d changed=%d cooldown=%d",
             mode_name(next.mode), next.interval_s, threshold, changed, cooldown);
    return handle_status(req);
}

esp_err_t handle_index(httpd_req_t* req) {
    const RuntimeSettings current = load_settings();
    char body[6144];
    int used = std::snprintf(
        body, sizeof(body),
        "<!doctype html><html><head><meta charset='utf-8'><title>ESPKit timelapse</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;margin:1rem;max-width:48rem}"
        "code{background:#eee;padding:.1rem .3rem}img{max-width:11rem;margin:.25rem}"
        "form,p{margin:.6rem 0}</style></head><body>"
        "<h1>ESPKit camera timelapse</h1>"
        "<p>Mode <code>%s</code> &middot; interval %d s &middot; photos %d/%d &middot; "
        "motion events %u &middot; last score %u &middot; firmware <code>%s</code> "
        "on <code>%s</code>.</p>"
        "<p><img src='/preview' alt='preview' style='max-width:20rem'></p>"
        "<p><a href='/preview'>preview</a> &middot; <a href='/capture'>capture</a> &middot; "
        "<a href='/status'>status</a> &middot; <a href='/list'>list</a></p>"
        "<form method='post' action='/settings'>"
        "<label>Mode <select name='mode'>"
        "<option value='timelapse'%s>timelapse</option>"
        "<option value='motion'%s>motion</option></select></label> "
        "<label>Interval <input name='interval_s' type='number' min='1' max='3600' "
        "value='%d'></label> "
        "<label>Pixel threshold <input name='pixel_threshold' type='number' min='1' max='64' "
        "value='%u'></label> "
        "<label>Changed &permil; <input name='changed_permille' type='number' min='1' max='500' "
        "value='%u'></label> "
        "<button type='submit'>Save</button></form>"
        "<form id='upload'><p>OTA: <input type='file' name='firmware' required> "
        "<button type='submit'>Upload firmware</button></p></form>"
        "<div>",
        mode_name(current.mode), current.interval_s, photos.count(), photos.capacity(),
        static_cast<unsigned>(photos.motion_events()),
        static_cast<unsigned>(photos.last_motion_score()), ota_running_version(),
        ota_partition_label(), current.mode == CaptureMode::timelapse ? " selected" : "",
        current.mode == CaptureMode::motion ? " selected" : "", current.interval_s,
        static_cast<unsigned>(current.motion.pixel_threshold),
        static_cast<unsigned>(current.motion.changed_permille));
    photos.for_each([&](const PhotoInfo& info) {
        if (used < 0 || static_cast<std::size_t>(used) + 160 >= sizeof(body)) {
            return;
        }
        used += std::snprintf(body + used, sizeof(body) - static_cast<std::size_t>(used),
                              "<a href='/photo/%u'><img src='/photo/%u' alt='%s %u'></a>",
                              static_cast<unsigned>(info.id), static_cast<unsigned>(info.id),
                              info.reason, static_cast<unsigned>(info.id));
    });
    if (used < 0 || static_cast<std::size_t>(used) + 360 >= sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "page too large");
        return ESP_FAIL;
    }
    used += std::snprintf(
        body + used, sizeof(body) - static_cast<std::size_t>(used),
        "</div><script>document.getElementById('upload').addEventListener('submit',async(e)=>{"
        "e.preventDefault();const f=e.target.firmware.files[0];if(!f)return;"
        "const r=await fetch('/update',{method:'POST',"
        "headers:{'Content-Type':'application/octet-stream'},body:f});"
        "alert(await r.text());});</script></body></html>");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body, used);
}

bool start_httpd() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kListenPort;
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 12;
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(tag, "httpd start failed");
        return false;
    }
    const httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = handle_index, .user_ctx = nullptr};
    const httpd_uri_t status_uri = {
        .uri = "/status", .method = HTTP_GET, .handler = handle_status, .user_ctx = nullptr};
    const httpd_uri_t list_uri = {
        .uri = "/list", .method = HTTP_GET, .handler = handle_list, .user_ctx = nullptr};
    const httpd_uri_t photo_uri = {
        .uri = "/photo/*", .method = HTTP_GET, .handler = handle_photo, .user_ctx = nullptr};
    const httpd_uri_t preview_uri = {
        .uri = "/preview", .method = HTTP_GET, .handler = handle_preview, .user_ctx = nullptr};
    const httpd_uri_t capture_uri = {
        .uri = "/capture", .method = HTTP_GET, .handler = handle_capture, .user_ctx = nullptr};
    const httpd_uri_t settings_uri = {
        .uri = "/settings", .method = HTTP_POST, .handler = handle_settings, .user_ctx = nullptr};
    const httpd_uri_t update_uri = {
        .uri = "/update", .method = HTTP_POST, .handler = ota_handle_update, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &list_uri);
    httpd_register_uri_handler(server, &photo_uri);
    httpd_register_uri_handler(server, &preview_uri);
    httpd_register_uri_handler(server, &capture_uri);
    httpd_register_uri_handler(server, &settings_uri);
    httpd_register_uri_handler(server, &update_uri);
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
    if (!ota_support_init()) {
        ESP_LOGE(tag, "ota init failed");
        return;
    }
    ESP_LOGI(tag, "running version=%s partition=%s state=%s", ota_running_version(),
             ota_partition_label(), ota_image_state_name(ota_running_state()));

    settings.motion.pixel_threshold =
        static_cast<std::uint8_t>(CONFIG_ESPKIT_CAMERA_TIMELAPSE_PIXEL_THRESHOLD);
    settings.motion.changed_permille =
        static_cast<std::uint16_t>(CONFIG_ESPKIT_CAMERA_TIMELAPSE_CHANGED_PERMILLE);
    settings.motion.cooldown_ms = CONFIG_ESPKIT_CAMERA_TIMELAPSE_COOLDOWN_MS;
    settings.motion.compensate_brightness = true;
    detector = image_motion::Detector{settings.motion};

    if (!init_led()) {
        return;
    }
    const bool pending = ota_running_state() == ota::ImageState::pending_verify ||
                         ota_running_state() == ota::ImageState::new_image;
    led_phase.store(pending ? LedPhase::ota_pending : LedPhase::idle);

    camera_lock = xSemaphoreCreateMutex();
    settings_lock = xSemaphoreCreateMutex();
    if (camera_lock == nullptr || settings_lock == nullptr ||
        !photos.init(CONFIG_ESPKIT_CAMERA_TIMELAPSE_RING_SLOTS)) {
        ESP_LOGE(tag, "store or lock init failed");
        ota_confirm_or_rollback(false);
        return;
    }

    configuration::NetworkConfig config{};
    configuration::NvsConfigStore store;
    const CredentialSource source = load_kconfig(config);
    if (source == CredentialSource::invalid) {
        led_phase.store(LedPhase::ota_failed);
        ota_confirm_or_rollback(false);
        return;
    }
    if (source == CredentialSource::missing && !store.load_wifi(config)) {
        ESP_LOGW(tag, "wifi not configured");
        led_phase.store(LedPhase::ota_failed);
        ota_confirm_or_rollback(false);
        return;
    }
    if (std::strlen(config.wifi_password) < kMinPasswordLength) {
        ESP_LOGE(tag, "wifi failed reason=password_too_short");
        led_phase.store(LedPhase::ota_failed);
        ota_confirm_or_rollback(false);
        return;
    }

    if (!init_camera()) {
        ota_confirm_or_rollback(false);
        return;
    }

    EspSteadyTime clock;
    networking::WifiStation station{clock};
    const bool wifi_ok = connect_wifi(station, config, clock);
    if (wifi_ok && !start_httpd()) {
        ota_confirm_or_rollback(false);
        return;
    }
    if (wifi_ok) {
        log_urls();
    }
    ota_confirm_or_rollback(wifi_ok);
    if (wifi_ok && led_phase.load() == LedPhase::ota_ok) {
        led_phase.store(LedPhase::idle);
    }
    if (xTaskCreate(capture_task, "tl-capture", 8192, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGE(tag, "capture task failed");
    }
}

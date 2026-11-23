#include "esp_camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <cstdint>

namespace {

constexpr char tag[] = "camera";

framesize_t frame_size() {
#if CONFIG_ESPKIT_CAMERA_FRAMESIZE_VGA
    return FRAMESIZE_VGA;
#else
    return FRAMESIZE_QVGA;
#endif
}

const char* format_name(pixformat_t format) {
    return format == PIXFORMAT_JPEG ? "jpeg" : "other";
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

} // namespace

extern "C" void app_main() {
    const esp_err_t nvs = init_nvs();
    if (nvs != ESP_OK) {
        ESP_LOGE(tag, "nvs init failed err=0x%x (%s)", static_cast<unsigned>(nvs),
                 esp_err_to_name(nvs));
        return;
    }

    camera_config_t config{};
#if CONFIG_ESPKIT_CAMERA_PRESET_CUSTOM
    config.pin_pwdn = CONFIG_ESPKIT_CAMERA_PIN_PWDN;
    config.pin_reset = CONFIG_ESPKIT_CAMERA_PIN_RESET;
    config.pin_xclk = CONFIG_ESPKIT_CAMERA_PIN_XCLK;
    config.pin_sccb_sda = CONFIG_ESPKIT_CAMERA_PIN_SIOD;
    config.pin_sccb_scl = CONFIG_ESPKIT_CAMERA_PIN_SIOC;
    config.pin_d0 = CONFIG_ESPKIT_CAMERA_PIN_D0;
    config.pin_d1 = CONFIG_ESPKIT_CAMERA_PIN_D1;
    config.pin_d2 = CONFIG_ESPKIT_CAMERA_PIN_D2;
    config.pin_d3 = CONFIG_ESPKIT_CAMERA_PIN_D3;
    config.pin_d4 = CONFIG_ESPKIT_CAMERA_PIN_D4;
    config.pin_d5 = CONFIG_ESPKIT_CAMERA_PIN_D5;
    config.pin_d6 = CONFIG_ESPKIT_CAMERA_PIN_D6;
    config.pin_d7 = CONFIG_ESPKIT_CAMERA_PIN_D7;
    config.pin_vsync = CONFIG_ESPKIT_CAMERA_PIN_VSYNC;
    config.pin_href = CONFIG_ESPKIT_CAMERA_PIN_HREF;
    config.pin_pclk = CONFIG_ESPKIT_CAMERA_PIN_PCLK;
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
    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = frame_size();
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    const esp_err_t started = esp_camera_init(&config);
    if (started != ESP_OK) {
        ESP_LOGE(tag, "camera init failed err=0x%x (%s)", static_cast<unsigned>(started),
                 esp_err_to_name(started));
        return;
    }

    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor == nullptr) {
        ESP_LOGE(tag, "camera sensor missing after init");
        return;
    }
    ESP_LOGI(tag, "camera sensor pid=0x%x model=%s", static_cast<unsigned>(sensor->id.PID),
             sensor_name(sensor));

    const int wanted = CONFIG_ESPKIT_CAMERA_FRAME_COUNT;
    int captured = 0;
    const std::int64_t started_us = esp_timer_get_time();
    for (int index = 1; index <= wanted; ++index) {
        camera_fb_t* frame = esp_camera_fb_get();
        if (frame == nullptr) {
            ESP_LOGE(tag, "camera frame n=%d failed", index);
            continue;
        }
        ++captured;
        ESP_LOGI(tag, "camera frame n=%d bytes=%u w=%u h=%u fmt=%s", index,
                 static_cast<unsigned>(frame->len), static_cast<unsigned>(frame->width),
                 static_cast<unsigned>(frame->height), format_name(frame->format));
        esp_camera_fb_return(frame);
    }
    const std::int64_t elapsed_us = esp_timer_get_time() - started_us;
    if (captured > 0 && elapsed_us > 0) {
        const double fps =
            static_cast<double>(captured) * 1000000.0 / static_cast<double>(elapsed_us);
        ESP_LOGI(tag, "camera capture done frames=%d avg_fps=%.2f", captured, fps);
    } else {
        ESP_LOGE(tag, "camera capture failed frames=0");
    }
}

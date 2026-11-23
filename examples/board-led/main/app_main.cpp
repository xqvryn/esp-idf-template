#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include <cstdint>

namespace {

constexpr char tag[] = "board-led";
constexpr int color_hold_ms = 800;

struct Color {
    const char* name;
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

constexpr Color kColors[] = {
    {"red", 16, 0, 0},  {"green", 0, 16, 0}, {"blue", 0, 0, 16},
    {"white", 8, 8, 8}, {"off", 0, 0, 0},
};

esp_err_t set_color(led_strip_handle_t strip, const Color& color) {
    const int count = CONFIG_ESPKIT_BOARD_LED_COUNT;
    for (int index = 0; index < count; ++index) {
        const esp_err_t err = led_strip_set_pixel(strip, static_cast<std::uint32_t>(index),
                                                  color.red, color.green, color.blue);
        if (err != ESP_OK) {
            return err;
        }
    }
    return led_strip_refresh(strip);
}

} // namespace

extern "C" void app_main() {
    ESP_LOGI(tag, "board-led gpio=%d count=%d", CONFIG_ESPKIT_BOARD_LED_GPIO,
             CONFIG_ESPKIT_BOARD_LED_COUNT);

    led_strip_config_t strip_config{};
    strip_config.strip_gpio_num = CONFIG_ESPKIT_BOARD_LED_GPIO;
    strip_config.max_leds = static_cast<std::uint32_t>(CONFIG_ESPKIT_BOARD_LED_COUNT);
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

    led_strip_handle_t strip = nullptr;
    const esp_err_t created = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (created != ESP_OK) {
        ESP_LOGE(tag, "led strip init failed err=0x%x (%s)", static_cast<unsigned>(created),
                 esp_err_to_name(created));
        return;
    }

    while (true) {
        for (const Color& color : kColors) {
            const esp_err_t err = set_color(strip, color);
            if (err != ESP_OK) {
                ESP_LOGE(tag, "led color=%s failed err=0x%x (%s)", color.name,
                         static_cast<unsigned>(err), esp_err_to_name(err));
                return;
            }
            ESP_LOGI(tag, "led color=%s", color.name);
            vTaskDelay(pdMS_TO_TICKS(color_hold_ms));
        }
    }
}

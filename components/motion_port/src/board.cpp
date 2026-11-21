#include "motion_port/board.hpp"

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_timer.h"
#include "driver/rmt_tx.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "hal/rmt_types.h"
#include "motion/pwm.hpp"

namespace motion_port {

namespace {

constexpr int k_resolution_bits = 14;
i2c_master_bus_handle_t i2c_bus = nullptr;
spi_device_handle_t spi_device = nullptr;
mcpwm_timer_handle_t mcpwm_timer = nullptr;
mcpwm_cmpr_handle_t mcpwm_compare = nullptr;
rmt_channel_handle_t dshot_channel[4] = {};
bool ledc_ready = false;
bool sbus_ready = false;

bool gpio_ok(int gpio) noexcept {
    return gpio >= 0 && gpio <= 48;
}

std::uint32_t duty_for(int pulse_us) noexcept {
    std::uint32_t duty = 0;
    if (!motion::pulse_to_duty(pulse_us, motion::servo_period_us, k_resolution_bits, duty)) {
        return 0;
    }
    return duty;
}

} // namespace

bool install_i2c(const motion::MotionPins& pins) noexcept {
    if (pins.i2c_sda < 0 && pins.i2c_scl < 0) {
        return true;
    }
    if (!gpio_ok(pins.i2c_sda) || !gpio_ok(pins.i2c_scl) || pins.i2c_hz < 1000) {
        return false;
    }
    i2c_master_bus_config_t config = {};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = static_cast<gpio_num_t>(pins.i2c_sda);
    config.scl_io_num = static_cast<gpio_num_t>(pins.i2c_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&config, &i2c_bus) == ESP_OK;
}

bool install_spi(const motion::MotionPins& pins) noexcept {
    if (pins.spi_cs < 0 && pins.spi_sclk < 0 && pins.spi_mosi < 0 && pins.spi_miso < 0) {
        return true;
    }
    if (!gpio_ok(pins.spi_cs) || !gpio_ok(pins.spi_sclk) || !gpio_ok(pins.spi_mosi) ||
        !gpio_ok(pins.spi_miso)) {
        return false;
    }
    spi_bus_config_t bus = {};
    bus.mosi_io_num = pins.spi_mosi;
    bus.miso_io_num = pins.spi_miso;
    bus.sclk_io_num = pins.spi_sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }
    spi_device_interface_config_t device = {};
    device.clock_speed_hz = 1000000;
    device.mode = 0;
    device.spics_io_num = pins.spi_cs;
    device.queue_size = 1;
    return spi_bus_add_device(SPI2_HOST, &device, &spi_device) == ESP_OK;
}

bool install_ledc(const motion::MotionPins& pins) noexcept {
    bool any = false;
    for (int gpio : pins.ledc_gpio) {
        if (gpio >= 0) {
            any = true;
        }
    }
    if (!any) {
        return true;
    }
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 50;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        if (pins.ledc_gpio[index] < 0) {
            continue;
        }
        if (!gpio_ok(pins.ledc_gpio[index])) {
            return false;
        }
        ledc_channel_config_t channel = {};
        channel.gpio_num = pins.ledc_gpio[index];
        channel.speed_mode = LEDC_LOW_SPEED_MODE;
        channel.channel = static_cast<ledc_channel_t>(index);
        channel.intr_type = LEDC_INTR_DISABLE;
        channel.timer_sel = LEDC_TIMER_0;
        channel.duty = 0;
        if (ledc_channel_config(&channel) != ESP_OK) {
            return false;
        }
    }
    ledc_ready = true;
    return true;
}

bool write_ledc_pulse(int channel, int pulse_us) noexcept {
    if (!ledc_ready) {
        return channel < 0;
    }
    if (channel < 0 || channel > 3) {
        return false;
    }
    const auto duty =
        duty_for(motion::clamp_pulse_us(pulse_us, motion::servo_min_us, motion::servo_max_us));
    return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel), duty,
                                    0) == ESP_OK;
}

bool install_mcpwm(const motion::MotionPins& pins) noexcept {
    if (pins.mcpwm_gpio < 0) {
        return true;
    }
    if (!gpio_ok(pins.mcpwm_gpio)) {
        return false;
    }
    mcpwm_timer_config_t timer_config = {};
    timer_config.group_id = 0;
    timer_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timer_config.resolution_hz = 1000000;
    timer_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timer_config.period_ticks = motion::servo_period_us;
    if (mcpwm_new_timer(&timer_config, &mcpwm_timer) != ESP_OK) {
        return false;
    }
    mcpwm_operator_config_t operator_config = {};
    operator_config.group_id = 0;
    mcpwm_oper_handle_t mcpwm_operator = nullptr;
    if (mcpwm_new_operator(&operator_config, &mcpwm_operator) != ESP_OK ||
        mcpwm_operator_connect_timer(mcpwm_operator, mcpwm_timer) != ESP_OK) {
        return false;
    }
    mcpwm_comparator_config_t compare_config = {};
    if (mcpwm_new_comparator(mcpwm_operator, &compare_config, &mcpwm_compare) != ESP_OK) {
        return false;
    }
    mcpwm_generator_config_t generator_config = {};
    generator_config.gen_gpio_num = pins.mcpwm_gpio;
    mcpwm_gen_handle_t generator = nullptr;
    if (mcpwm_new_generator(mcpwm_operator, &generator_config, &generator) != ESP_OK) {
        return false;
    }
    if (mcpwm_generator_set_action_on_timer_event(
            generator,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY,
                                         MCPWM_GEN_ACTION_HIGH)) != ESP_OK ||
        mcpwm_generator_set_action_on_compare_event(
            generator, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, mcpwm_compare,
                                                      MCPWM_GEN_ACTION_LOW)) != ESP_OK ||
        mcpwm_timer_enable(mcpwm_timer) != ESP_OK ||
        mcpwm_timer_start_stop(mcpwm_timer, MCPWM_TIMER_START_NO_STOP) != ESP_OK) {
        return false;
    }
    return true;
}

bool write_mcpwm_pulse(int pulse_us) noexcept {
    if (mcpwm_compare == nullptr) {
        return pulse_us < 0;
    }
    const auto ticks = static_cast<std::uint32_t>(
        motion::clamp_pulse_us(pulse_us, motion::servo_min_us, motion::servo_max_us));
    return mcpwm_comparator_set_compare_value(mcpwm_compare, ticks) == ESP_OK;
}

bool install_dshot(const motion::MotionPins& pins) noexcept {
    bool any = false;
    for (int index = 0; index < 4; ++index) {
        if (pins.dshot_gpio[index] < 0) {
            continue;
        }
        any = true;
        if (!gpio_ok(pins.dshot_gpio[index])) {
            return false;
        }
        rmt_tx_channel_config_t config = {};
        config.gpio_num = static_cast<gpio_num_t>(pins.dshot_gpio[index]);
        config.clk_src = RMT_CLK_SRC_DEFAULT;
        config.resolution_hz = 10000000;
        config.mem_block_symbols = 64;
        config.trans_queue_depth = 2;
        if (rmt_new_tx_channel(&config, &dshot_channel[index]) != ESP_OK ||
            rmt_enable(dshot_channel[index]) != ESP_OK) {
            return false;
        }
    }
    return any || pins.dshot_gpio[0] < 0;
}

bool write_dshot(int channel, std::uint16_t frame) noexcept {
    if (channel < 0 || channel > 3 || dshot_channel[channel] == nullptr) {
        return false;
    }
    motion::DshotSymbol symbols[16] = {};
    if (!motion::dshot_symbols(frame, 16, symbols)) {
        return false;
    }
    rmt_symbol_word_t words[16] = {};
    for (int bit = 0; bit < 16; ++bit) {
        words[bit].duration0 = symbols[bit].high_ticks;
        words[bit].level0 = 1;
        words[bit].duration1 = symbols[bit].low_ticks;
        words[bit].level1 = 0;
    }
    rmt_transmit_config_t config = {};
    config.loop_count = 0;
    return rmt_transmit(dshot_channel[channel], nullptr, words, sizeof(words), &config) == ESP_OK;
}

bool install_sbus(const motion::MotionPins& pins) noexcept {
    if (pins.sbus_gpio < 0) {
        return true;
    }
    if (!gpio_ok(pins.sbus_gpio)) {
        return false;
    }
    uart_config_t config = {};
    config.baud_rate = 100000;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_EVEN;
    config.stop_bits = UART_STOP_BITS_2;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    if (uart_driver_install(UART_NUM_1, 256, 0, 0, nullptr, 0) != ESP_OK ||
        uart_param_config(UART_NUM_1, &config) != ESP_OK ||
        uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, pins.sbus_gpio, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_set_line_inverse(UART_NUM_1, UART_SIGNAL_RXD_INV) != ESP_OK) {
        return false;
    }
    sbus_ready = true;
    return true;
}

} // namespace motion_port

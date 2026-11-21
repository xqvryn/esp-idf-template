#pragma once

namespace motion {

// GPIOs stay -1 until a board profile assigns them. -1 means the simulated backend.
struct MotionPins {
    int i2c_sda{-1};
    int i2c_scl{-1};
    int i2c_hz{400000};
    int spi_mosi{-1};
    int spi_miso{-1};
    int spi_sclk{-1};
    int spi_cs{-1};
    int ledc_gpio[4]{-1, -1, -1, -1};
    int mcpwm_gpio{-1};
    int dshot_gpio[4]{-1, -1, -1, -1};
    int sbus_gpio{-1};
};

bool hardware_pins_selected(const MotionPins& pins) noexcept;

} // namespace motion

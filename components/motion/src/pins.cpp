#include "motion/pins.hpp"

namespace motion {

bool hardware_pins_selected(const MotionPins& pins) noexcept {
    if (pins.i2c_sda >= 0 || pins.i2c_scl >= 0 || pins.spi_mosi >= 0 || pins.spi_miso >= 0 ||
        pins.spi_sclk >= 0 || pins.spi_cs >= 0 || pins.mcpwm_gpio >= 0 || pins.sbus_gpio >= 0) {
        return true;
    }
    for (int gpio : pins.ledc_gpio) {
        if (gpio >= 0) {
            return true;
        }
    }
    for (int gpio : pins.dshot_gpio) {
        if (gpio >= 0) {
            return true;
        }
    }
    return false;
}

} // namespace motion

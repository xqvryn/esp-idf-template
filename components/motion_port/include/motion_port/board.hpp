#pragma once

#include "motion/dshot.hpp"
#include "motion/pins.hpp"

#include <cstdint>

namespace motion_port {

// Each call returns true without touching a peripheral when every GPIO is -1.
// A selected pin is programmed. No board in this repository selects one.
bool install_i2c(const motion::MotionPins& pins) noexcept;
bool install_spi(const motion::MotionPins& pins) noexcept;
bool install_ledc(const motion::MotionPins& pins) noexcept;
bool write_ledc_pulse(int channel, int pulse_us) noexcept;
bool install_mcpwm(const motion::MotionPins& pins) noexcept;
bool write_mcpwm_pulse(int pulse_us) noexcept;
bool install_dshot(const motion::MotionPins& pins) noexcept;
bool write_dshot(int channel, std::uint16_t frame) noexcept;
bool install_sbus(const motion::MotionPins& pins) noexcept;

} // namespace motion_port

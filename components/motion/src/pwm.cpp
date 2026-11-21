#include "motion/pwm.hpp"

#include <cstdint>

namespace motion {

int clamp_pulse_us(int pulse_us, int min_us, int max_us) noexcept {
    if (min_us > max_us) {
        return min_us;
    }
    if (pulse_us < min_us) {
        return min_us;
    }
    if (pulse_us > max_us) {
        return max_us;
    }
    return pulse_us;
}

bool pulse_to_duty(int pulse_us, int period_us, int resolution_bits, std::uint32_t& duty) noexcept {
    if (pulse_us < 0 || period_us <= 0 || pulse_us > period_us || resolution_bits < 1 ||
        resolution_bits > 16) {
        return false;
    }
    const auto full = (1U << static_cast<unsigned>(resolution_bits)) - 1U;
    duty = (static_cast<std::uint32_t>(pulse_us) * full) / static_cast<std::uint32_t>(period_us);
    return true;
}

} // namespace motion

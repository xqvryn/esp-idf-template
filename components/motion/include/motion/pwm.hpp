#pragma once

#include <cstdint>

namespace motion {

inline constexpr int servo_period_us = 20000;
inline constexpr int servo_min_us = 1000;
inline constexpr int servo_max_us = 2000;

class PwmOut {
    public:
    virtual ~PwmOut() = default;
    virtual bool write_pulse_us(int channel, int pulse_us) noexcept = 0;
};

class SimulatedPwm final : public PwmOut {
    public:
    bool write_pulse_us(int channel, int pulse_us) noexcept override {
        if (channel < 0 || channel >= 4) {
            return false;
        }
        pulse_us_[channel] = pulse_us;
        written_[channel] = true;
        return true;
    }

    bool written(int channel) const noexcept {
        return channel >= 0 && channel < 4 && written_[channel];
    }

    int pulse_us(int channel) const noexcept {
        return channel >= 0 && channel < 4 ? pulse_us_[channel] : 0;
    }

    private:
    int pulse_us_[4]{};
    bool written_[4]{};
};

int clamp_pulse_us(int pulse_us, int min_us, int max_us) noexcept;
// Duty counts for a period of period_us. resolution_bits is 1..16.
bool pulse_to_duty(int pulse_us, int period_us, int resolution_bits, std::uint32_t& duty) noexcept;

} // namespace motion

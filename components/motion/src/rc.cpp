#include "motion/rc.hpp"

namespace motion {

bool decode_sbus(std::span<const std::uint8_t> frame, RcSample& sample) noexcept {
    sample = {};
    if (frame.size() != 25 || frame[0] != 0x0F || frame[24] != 0x00) {
        return false;
    }
    std::uint32_t bit = 0;
    for (int channel = 0; channel < 16; ++channel) {
        int value = 0;
        for (int offset = 0; offset < 11; ++offset) {
            const auto index = static_cast<std::size_t>(1 + (bit / 8));
            const auto mask = 1U << (bit % 8);
            if ((frame[index] & mask) != 0) {
                value |= 1 << offset;
            }
            ++bit;
        }
        sample.channel_us[channel] = value;
    }
    sample.count = 16;
    sample.failsafe = (frame[23] & 0x08) != 0;
    const bool frame_lost = (frame[23] & 0x04) != 0;
    sample.valid = !frame_lost && !sample.failsafe;
    return true;
}

bool decode_ppm(std::span<const int> pulses_us, RcSample& sample) noexcept {
    sample = {};
    int count = 0;
    for (int pulse : pulses_us) {
        if (pulse >= 3000) {
            break;
        }
        if (pulse < 800 || pulse > 2200 || count >= 16) {
            sample = {};
            return false;
        }
        sample.channel_us[count++] = pulse;
    }
    if (count == 0) {
        return false;
    }
    sample.count = count;
    sample.valid = true;
    return true;
}

} // namespace motion

#include "motion/dshot.hpp"

namespace motion {

bool encode_dshot(std::uint16_t throttle, bool telemetry, std::uint16_t& frame) noexcept {
    if (throttle > 2047) {
        return false;
    }
    const auto packet = static_cast<std::uint16_t>((throttle << 1) | (telemetry ? 1U : 0U));
    const auto crc = static_cast<std::uint16_t>((packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0F);
    frame = static_cast<std::uint16_t>((packet << 4) | crc);
    return true;
}

bool dshot_symbols(std::uint16_t frame, std::uint16_t ticks_per_bit,
                   DshotSymbol (&out)[16]) noexcept {
    if (ticks_per_bit < 8 || ticks_per_bit > 20000) {
        return false;
    }
    for (int bit = 0; bit < 16; ++bit) {
        const bool one = ((frame >> (15 - bit)) & 1U) != 0;
        const auto high = static_cast<std::uint16_t>((ticks_per_bit * (one ? 6 : 3)) / 8);
        out[bit].high_ticks = high;
        out[bit].low_ticks = static_cast<std::uint16_t>(ticks_per_bit - high);
    }
    return true;
}

} // namespace motion

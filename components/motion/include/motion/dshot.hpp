#pragma once

#include <cstdint>

namespace motion {

struct DshotSymbol {
    std::uint16_t high_ticks{0};
    std::uint16_t low_ticks{0};
};

// 11-bit throttle (0..2047) and the telemetry bit. CRC is the XOR of the three nibbles.
bool encode_dshot(std::uint16_t throttle, bool telemetry, std::uint16_t& frame) noexcept;
// MSB first. A zero bit is high for 3/8 of the bit, a one bit for 6/8. ticks_per_bit >= 8.
bool dshot_symbols(std::uint16_t frame, std::uint16_t ticks_per_bit,
                   DshotSymbol (&out)[16]) noexcept;

} // namespace motion

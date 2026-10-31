#include "networking/telemetry.hpp"

#include <algorithm>

namespace networking {

std::uint32_t ReconnectBackoff::next_delay(std::uint32_t random) noexcept {
    constexpr std::uint32_t base_ms = 500;
    constexpr std::uint32_t maximum_ms = 30000;
    const auto base = std::min(maximum_ms, base_ms << std::min(attempt_, 6U));
    attempt_ = std::min(attempt_ + 1, 6U);
    return std::min(maximum_ms, base + random % (base / 4 + 1));
}

} // namespace networking

#pragma once

#include <cstdint>
#include <limits>

namespace runtime {

// Monotonic clock a host test can step. Not a board clock and not a HAL.
struct ScriptedClock {
    std::int64_t now_us{0};

    void advance(std::int64_t delta_us) noexcept {
        if (delta_us <= 0) {
            return;
        }
        if (now_us > std::numeric_limits<std::int64_t>::max() - delta_us) {
            now_us = std::numeric_limits<std::int64_t>::max();
            return;
        }
        now_us += delta_us;
    }
};

} // namespace runtime

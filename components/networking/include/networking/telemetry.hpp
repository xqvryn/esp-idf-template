#pragma once

#include <cstdint>

namespace networking {

class ReconnectBackoff {
    public:
    std::uint32_t next_delay(std::uint32_t random) noexcept;

    void reset() noexcept {
        attempt_ = 0;
    }

    private:
    unsigned attempt_{0};
};

} // namespace networking

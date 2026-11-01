#pragma once

#include <cstdint>

namespace platform {

class EspClock {
    public:
    std::int64_t now() const noexcept;
};

} // namespace platform

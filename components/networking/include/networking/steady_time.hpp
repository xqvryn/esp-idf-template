#pragma once

#include <cstdint>

namespace networking {

// Monotonic milliseconds for link timeouts. Not a shared hardware abstraction.
class SteadyTime {
    public:
    virtual ~SteadyTime() = default;
    virtual std::int64_t now() const noexcept = 0;
};

} // namespace networking

#pragma once

#include "application/ports/device.hpp"
#include "platform/clock.hpp"

namespace wiring {

class Clock final : public application::Clock {
    public:
    domain::Milliseconds now() const noexcept override {
        return clock_.now();
    }

    private:
    platform::EspClock clock_;
};

} // namespace wiring

#include "platform/clock.hpp"

#include "esp_timer.h"
#include "platform/constants.hpp"

namespace platform {

std::int64_t EspClock::now() const noexcept {
    return esp_timer_get_time() / runtime_constants::microseconds_per_millisecond;
}

} // namespace platform

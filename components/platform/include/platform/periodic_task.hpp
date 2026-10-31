#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/constants.hpp"

#include <cstdint>
#include <limits>

namespace platform {

struct TaskOptions {
    const char* name;
    std::uint32_t stack_bytes;
    unsigned priority;
    std::int64_t period_ms;
};

// Borrows a worker. Both objects must outlive the running FreeRTOS task.
// Work only needs void step() noexcept. This is not a shared event bus.
template <typename Work> class PeriodicTask {
    public:
    PeriodicTask(Work& work, TaskOptions options) : work_(work), options_(options) {
    }

    PeriodicTask(const PeriodicTask&) = delete;
    PeriodicTask& operator=(const PeriodicTask&) = delete;

    bool start() noexcept {
        if (handle_ || !options_.name || !*options_.name || options_.period_ms <= 0 ||
            options_.stack_bytes == 0 || options_.priority >= configMAX_PRIORITIES) {
            return false;
        }
        // Calculate before narrowing; reject sub-tick periods and conversion overflow.
        const auto max_ms =
            (std::numeric_limits<TickType_t>::max() * runtime_constants::milliseconds_per_second) /
            configTICK_RATE_HZ;
        if (static_cast<std::uint64_t>(options_.period_ms) > max_ms) {
            return false;
        }
        period_ticks_ = static_cast<TickType_t>(
            (static_cast<std::uint64_t>(options_.period_ms) * configTICK_RATE_HZ) /
            runtime_constants::milliseconds_per_second);
        if (!period_ticks_) {
            return false;
        }
        return xTaskCreate(entry, options_.name, options_.stack_bytes, this, options_.priority,
                           &handle_) == pdPASS;
    }

    private:
    static void entry(void* context) {
        auto& task = *static_cast<PeriodicTask*>(context);
        auto wake = xTaskGetTickCount();
        for (;;) {
            task.work_.step();
            vTaskDelayUntil(&wake, task.period_ticks_);
        }
    }

    Work& work_;
    TaskOptions options_;
    TaskHandle_t handle_{nullptr};
    TickType_t period_ticks_{0};
};

} // namespace platform

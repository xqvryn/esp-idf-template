#pragma once

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "runtime/loop_budget.hpp"
#include "runtime/task_spec.hpp"

#include <cstdint>
#include <limits>

namespace runtime_task {

// FreeRTOS periodic task. Work only needs void step() noexcept.
// stop() deletes the task. step() must not own a lock stop() needs.
template <typename Work> class PinnedPeriodicTask {
    public:
    explicit PinnedPeriodicTask(Work& work) : work_(work) {
    }

    PinnedPeriodicTask(const PinnedPeriodicTask&) = delete;
    PinnedPeriodicTask& operator=(const PinnedPeriodicTask&) = delete;

    ~PinnedPeriodicTask() {
        stop();
    }

    bool start(const runtime::TaskSpec& spec, std::int64_t period_us) noexcept {
        if (handle_ != nullptr || period_us <= 0) {
            return false;
        }
        if (!runtime::task_spec_valid(spec, configMAX_PRIORITIES)) {
            return false;
        }
        const auto ticks_per_second = static_cast<std::uint64_t>(configTICK_RATE_HZ);
        const auto period_ticks =
            (static_cast<std::uint64_t>(period_us) * ticks_per_second) / 1000000ULL;
        if (period_ticks == 0 ||
            period_ticks > static_cast<std::uint64_t>(std::numeric_limits<TickType_t>::max())) {
            return false;
        }
        if (!budget_.arm(esp_timer_get_time(), period_us)) {
            return false;
        }
        period_ticks_ = static_cast<TickType_t>(period_ticks);
        const BaseType_t core = spec.core < 0 ? tskNO_AFFINITY : static_cast<BaseType_t>(spec.core);
        if (xTaskCreatePinnedToCore(entry, spec.name, spec.stack_bytes, this, spec.priority,
                                    &handle_, core) != pdPASS) {
            handle_ = nullptr;
            return false;
        }
        return true;
    }

    void stop() noexcept {
        if (handle_ == nullptr) {
            return;
        }
        TaskHandle_t handle = handle_;
        handle_ = nullptr;
        if (xTaskGetCurrentTaskHandle() == handle) {
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelete(handle);
    }

    bool running() const noexcept {
        return handle_ != nullptr;
    }

    runtime::LoopBudget& budget() noexcept {
        return budget_;
    }

    private:
    static void entry(void* context) {
        auto& task = *static_cast<PinnedPeriodicTask*>(context);
        auto wake = xTaskGetTickCount();
        for (;;) {
            task.budget_.observe(esp_timer_get_time());
            task.work_.step();
            vTaskDelayUntil(&wake, task.period_ticks_);
        }
    }

    Work& work_;
    runtime::LoopBudget budget_{};
    TaskHandle_t handle_{nullptr};
    TickType_t period_ticks_{0};
};

} // namespace runtime_task

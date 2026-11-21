#pragma once

#include <cstdint>

namespace runtime {

// Records lateness of a periodic loop. The caller supplies the time.
// One late wake counts every deadline it skipped. An early wake is not a miss.
class LoopBudget {
    public:
    bool arm(std::int64_t now_us, std::int64_t period_us) noexcept;
    void observe(std::int64_t now_us) noexcept;

    bool armed() const noexcept {
        return armed_;
    }

    bool clock_fault() const noexcept {
        return clock_fault_;
    }

    std::int64_t last_jitter_us() const noexcept {
        return last_jitter_us_;
    }

    std::int64_t max_jitter_us() const noexcept {
        return max_jitter_us_;
    }

    std::uint32_t deadline_misses() const noexcept {
        return deadline_misses_;
    }

    std::int64_t period_us() const noexcept {
        return period_us_;
    }

    private:
    std::int64_t period_us_{0};
    std::int64_t deadline_us_{0};
    std::int64_t last_us_{0};
    std::int64_t last_jitter_us_{0};
    std::int64_t max_jitter_us_{0};
    std::uint32_t deadline_misses_{0};
    bool armed_{false};
    bool clock_fault_{false};
    bool have_sample_{false};
};

} // namespace runtime

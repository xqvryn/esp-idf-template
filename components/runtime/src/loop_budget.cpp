#include "runtime/loop_budget.hpp"

#include <limits>

namespace runtime {

namespace {

bool add_nonnegative(std::int64_t left, std::int64_t right, std::int64_t& sum) noexcept {
    if (right < 0 || left > std::numeric_limits<std::int64_t>::max() - right) {
        return false;
    }
    sum = left + right;
    return true;
}

} // namespace

bool LoopBudget::arm(std::int64_t now_us, std::int64_t period_us) noexcept {
    if (now_us < 0 || period_us <= 0) {
        return false;
    }
    std::int64_t deadline = 0;
    if (!add_nonnegative(now_us, period_us, deadline)) {
        return false;
    }
    period_us_ = period_us;
    deadline_us_ = deadline;
    last_us_ = now_us;
    last_jitter_us_ = 0;
    max_jitter_us_ = 0;
    deadline_misses_ = 0;
    armed_ = true;
    clock_fault_ = false;
    have_sample_ = false;
    return true;
}

void LoopBudget::observe(std::int64_t now_us) noexcept {
    if (!armed_ || clock_fault_ || now_us < 0) {
        return;
    }
    if (have_sample_ && now_us < last_us_) {
        clock_fault_ = true;
        return;
    }
    have_sample_ = true;
    last_us_ = now_us;
    if (now_us < deadline_us_) {
        last_jitter_us_ = now_us - deadline_us_;
        std::int64_t next = 0;
        if (!add_nonnegative(deadline_us_, period_us_, next)) {
            clock_fault_ = true;
            return;
        }
        deadline_us_ = next;
        return;
    }
    if (now_us == deadline_us_) {
        last_jitter_us_ = 0;
        std::int64_t next = 0;
        if (!add_nonnegative(deadline_us_, period_us_, next)) {
            clock_fault_ = true;
            return;
        }
        deadline_us_ = next;
        return;
    }

    const auto last_missed = now_us - 1;
    const auto span = last_missed - deadline_us_;
    const auto count = static_cast<std::uint64_t>(span / period_us_) + 1U;
    last_jitter_us_ = now_us - deadline_us_;
    if (last_jitter_us_ > max_jitter_us_) {
        max_jitter_us_ = last_jitter_us_;
    }
    if (count > std::numeric_limits<std::uint32_t>::max() - deadline_misses_) {
        deadline_misses_ = std::numeric_limits<std::uint32_t>::max();
        clock_fault_ = true;
        return;
    }
    deadline_misses_ += static_cast<std::uint32_t>(count);

    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / period_us_)) {
        clock_fault_ = true;
        return;
    }
    const auto advance = static_cast<std::int64_t>(count) * period_us_;
    std::int64_t landed = 0;
    if (!add_nonnegative(deadline_us_, advance, landed)) {
        clock_fault_ = true;
        return;
    }
    if (landed == now_us) {
        std::int64_t next = 0;
        if (!add_nonnegative(landed, period_us_, next)) {
            clock_fault_ = true;
            return;
        }
        deadline_us_ = next;
        return;
    }
    deadline_us_ = landed;
}

} // namespace runtime

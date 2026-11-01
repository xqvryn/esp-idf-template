#pragma once

#include <cstdint>

namespace timed_output {

using Milliseconds = std::int64_t;

inline constexpr Milliseconds default_max_run_ms = 5000;
inline constexpr Milliseconds default_max_sample_age_ms = 2000;

// Freshness of one observation. The policy does not know what was measured.
struct SampleTime {
    Milliseconds sampled_at{0};
    bool valid{false};
};

struct OutputPolicy {
    Milliseconds max_run_ms{default_max_run_ms};
    Milliseconds max_sample_age_ms{default_max_sample_age_ms};
};

enum class OutputState {
    off,
    running,
    fault
};

// Single-owner state machine. Repeated ON requests cannot extend a running interval.
class TimedOutput {
    public:
    explicit TimedOutput(OutputPolicy policy) noexcept;
    bool update(bool requested, SampleTime sample, Milliseconds now) noexcept;
    bool reset_fault() noexcept;

    void trip_fault() noexcept {
        state_ = OutputState::fault;
    }

    bool fault() const noexcept {
        return state_ == OutputState::fault;
    }

    OutputState state() const noexcept {
        return state_;
    }

    private:
    OutputPolicy policy_;
    Milliseconds started_at_{0};
    Milliseconds last_update_{0};
    bool has_update_{false};
    OutputState state_{OutputState::off};
};

} // namespace timed_output

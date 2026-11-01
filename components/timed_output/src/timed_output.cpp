#include "timed_output/timed_output.hpp"

namespace timed_output {

TimedOutput::TimedOutput(OutputPolicy policy) noexcept : policy_(policy) {
}

bool TimedOutput::update(bool requested, SampleTime sample, Milliseconds now) noexcept {
    const bool invalid = policy_.max_run_ms <= 0 || policy_.max_sample_age_ms <= 0 || now < 0 ||
                         (has_update_ && now < last_update_) || !sample.valid ||
                         sample.sampled_at < 0 || sample.sampled_at > now ||
                         now - sample.sampled_at > policy_.max_sample_age_ms;
    has_update_ = true;
    last_update_ = now;
    if (invalid || (state_ == OutputState::running && now - started_at_ >= policy_.max_run_ms)) {
        trip_fault();
    }
    if (fault()) {
        return false;
    }
    if (!requested) {
        state_ = OutputState::off;
        return false;
    }
    if (state_ == OutputState::off) {
        started_at_ = now;
    }
    state_ = OutputState::running;
    return true;
}

bool TimedOutput::reset_fault() noexcept {
    if (state_ == OutputState::running) {
        return false;
    }
    state_ = OutputState::off;
    return true;
}

} // namespace timed_output

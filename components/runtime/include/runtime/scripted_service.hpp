#pragma once

#include "runtime/service.hpp"

namespace runtime {

// Test double with a scripted start failure. Not a hardware abstraction.
class ScriptedService final : public Service {
    public:
    bool start() noexcept override {
        ++start_calls_;
        if (fail_start_) {
            health_ = {ServiceState::failed, fault_code_};
            return false;
        }
        health_ = {ServiceState::running, 0};
        return true;
    }

    void stop() noexcept override {
        ++stop_calls_;
        health_ = {ServiceState::stopped, 0};
    }

    ServiceHealth health() const noexcept override {
        return health_;
    }

    void fail_next_start(std::uint32_t fault_code) noexcept {
        fail_start_ = true;
        fault_code_ = fault_code;
    }

    int start_calls() const noexcept {
        return start_calls_;
    }

    int stop_calls() const noexcept {
        return stop_calls_;
    }

    private:
    ServiceHealth health_{};
    bool fail_start_{false};
    std::uint32_t fault_code_{1};
    int start_calls_{0};
    int stop_calls_{0};
};

} // namespace runtime

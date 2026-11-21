#pragma once

#include <cstdint>

namespace runtime {

enum class ServiceState : std::uint8_t {
    stopped,
    running,
    failed
};

struct ServiceHealth {
    ServiceState state{ServiceState::stopped};
    std::uint32_t fault_code{0};
};

// One object the composition root starts and stops. Not a service locator.
class Service {
    public:
    virtual ~Service() = default;
    virtual bool start() noexcept = 0;
    virtual void stop() noexcept = 0;
    virtual ServiceHealth health() const noexcept = 0;
};

} // namespace runtime

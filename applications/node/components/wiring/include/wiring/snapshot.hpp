#pragma once

#include "application/ports/device.hpp"
#include "platform/snapshot.hpp"

namespace wiring {

template <typename T>
class Snapshot final : public application::SnapshotReader<T>,
                       public application::SnapshotWriter<T> {
    public:
    bool ready() const noexcept {
        return queue_.ready();
    }

    bool latest(T& value) const noexcept override {
        return queue_.latest(value);
    }

    void publish(const T& value) noexcept override {
        queue_.publish(value);
    }

    private:
    platform::FreeRtosSnapshot<T> queue_{};
};

} // namespace wiring

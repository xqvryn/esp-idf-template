#pragma once

#include "application/ports/command.hpp"
#include "platform/command_queue.hpp"

#include <cstddef>

namespace wiring {

template <typename T> class Fifo final : public application::BoundedFifo<T> {
    public:
    explicit Fifo(std::size_t capacity) : queue_(capacity) {
    }

    bool ready() const noexcept {
        return queue_.ready();
    }

    bool try_push(const T& value) noexcept override {
        return queue_.try_push(value);
    }

    bool try_pop(T& value) noexcept override {
        return queue_.try_pop(value);
    }

    std::size_t size() const noexcept override {
        return queue_.size();
    }

    std::size_t capacity() const noexcept override {
        return queue_.capacity();
    }

    private:
    platform::FreeRtosFifo<T> queue_;
};

} // namespace wiring

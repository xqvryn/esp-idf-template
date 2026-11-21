#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace runtime {

// Bounded single-producer single-consumer queue. Capacity must be a power of two.
// One slot stays empty so head and tail can tell full from empty. try_push fails
// when the queue is full. It does not block and it does not grow.
template <typename T, std::size_t Capacity> class SpscQueue {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0);

    public:
    SpscQueue() = default;
    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    bool try_push(const T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        slots_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        item = slots_[tail];
        tail_.store((tail + 1) & mask, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() noexcept {
        return Capacity - 1;
    }

    private:
    static constexpr std::size_t mask = Capacity - 1;
    std::array<T, Capacity> slots_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace runtime

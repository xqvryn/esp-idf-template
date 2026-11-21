#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace runtime {

// Latest-value slot for one writer and one reader. A new publish replaces the
// previous value. load returns false until the first successful publish.
// T is copied as 64-bit words so the reader does not tear a multi-word value.
template <typename T> class LatestMailbox {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) > 0 && sizeof(T) <= 32);

    public:
    LatestMailbox() = default;
    LatestMailbox(const LatestMailbox&) = delete;
    LatestMailbox& operator=(const LatestMailbox&) = delete;

    void publish(const T& value) noexcept {
        std::uint64_t raw[words] = {};
        std::memcpy(raw, &value, sizeof(T));
        seq_.fetch_add(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t index = 0; index < words; ++index) {
            word_[index].store(raw[index], std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_release);
        seq_.fetch_add(1, std::memory_order_release);
    }

    bool load(T& value) const noexcept {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const auto before = seq_.load(std::memory_order_acquire);
            if (before == 0 || (before & 1U) != 0) {
                continue;
            }
            std::uint64_t raw[words] = {};
            for (std::size_t index = 0; index < words; ++index) {
                raw[index] = word_[index].load(std::memory_order_relaxed);
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            if (before != seq_.load(std::memory_order_acquire)) {
                continue;
            }
            std::memcpy(&value, raw, sizeof(T));
            return true;
        }
        return false;
    }

    private:
    static constexpr std::size_t words =
        (sizeof(T) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    std::atomic<std::uint64_t> word_[words]{};
    std::atomic<std::uint64_t> seq_{0};
};

} // namespace runtime

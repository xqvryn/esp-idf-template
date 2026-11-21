#pragma once

#include "runtime/spsc_queue.hpp"

#include <cstdint>

namespace channels {

enum class Severity : std::uint8_t {
    debug,
    info,
    warn,
    error
};

// One structured record. This is not ESP_LOG and it does not print.
struct Record {
    std::int64_t time_us{0};
    Severity severity{Severity::info};
    std::uint16_t code{0};
    float a{0};
    float b{0};
};

// Bounded channel between one writer and one reader. A full channel drops the
// new record and counts it. Capacity is the usable SPSC depth plus one slot.
template <std::size_t Capacity> class RecordChannel {
    static_assert(Capacity >= 2);

    public:
    bool emit(const Record& record) noexcept {
        if (!queue_.try_push(record)) {
            ++dropped_;
            return false;
        }
        return true;
    }

    bool take(Record& record) noexcept {
        return queue_.try_pop(record);
    }

    std::uint32_t dropped() const noexcept {
        return dropped_;
    }

    private:
    runtime::SpscQueue<Record, Capacity> queue_{};
    std::uint32_t dropped_{0};
};

} // namespace channels

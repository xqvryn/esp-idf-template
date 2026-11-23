#pragma once

#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct PhotoInfo {
    std::uint32_t id{0};
    const char* reason{"manual"};
    std::uint16_t score_permille{0};
    std::uint32_t length{0};
    std::int64_t captured_at_ms{0};
};

class PhotoStore {
    public:
    static constexpr int kMaxSlots = 32;

    bool init(int capacity) noexcept;
    std::uint32_t store(const std::uint8_t* jpeg, std::size_t length, const char* reason,
                        std::uint16_t score_permille, std::int64_t captured_at_ms) noexcept;
    int count() const noexcept;

    int capacity() const noexcept {
        return capacity_;
    }

    std::uint32_t captures() const noexcept {
        return captures_;
    }

    std::uint32_t motion_events() const noexcept {
        return motion_events_;
    }

    std::uint16_t last_motion_score() const noexcept {
        return last_motion_score_;
    }

    void set_last_motion_score(std::uint16_t score) noexcept {
        last_motion_score_ = score;
    }

    // Calls visitor(info, data) under the store lock for one id. Returns false
    // if the id has been overwritten or was never stored.
    template <typename Visitor> bool with_photo(std::uint32_t id, Visitor&& visitor) const {
        if (lock_ == nullptr || xSemaphoreTake(lock_, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return false;
        }
        bool found = false;
        for (int i = 0; i < count_; ++i) {
            const Slot& slot = slots_[i];
            if (slot.info.id == id && slot.data != nullptr) {
                visitor(slot.info, slot.data);
                found = true;
                break;
            }
        }
        xSemaphoreGive(lock_);
        return found;
    }

    template <typename Visitor> void for_each(Visitor&& visitor) const {
        if (lock_ == nullptr || xSemaphoreTake(lock_, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return;
        }
        for (int i = 0; i < count_; ++i) {
            const int index = (head_ + i) % capacity_;
            if (slots_[index].data != nullptr) {
                visitor(slots_[index].info);
            }
        }
        xSemaphoreGive(lock_);
    }

    private:
    struct Slot {
        PhotoInfo info{};
        std::uint8_t* data{nullptr};
    };

    void evict(Slot& slot) noexcept;

    Slot slots_[kMaxSlots]{};
    int capacity_{0};
    int count_{0};
    int next_{0};
    int head_{0};
    std::uint32_t next_id_{1};
    std::uint32_t captures_{0};
    std::uint32_t motion_events_{0};
    std::uint16_t last_motion_score_{0};
    mutable SemaphoreHandle_t lock_{nullptr};
};

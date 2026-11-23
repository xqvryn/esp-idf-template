#include "photo_store.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstring>

namespace {

constexpr char tag[] = "camera-timelapse";
}

bool PhotoStore::init(int capacity) noexcept {
    if (capacity < 1 || capacity > kMaxSlots) {
        return false;
    }
    capacity_ = capacity;
    lock_ = xSemaphoreCreateMutex();
    return lock_ != nullptr;
}

void PhotoStore::evict(Slot& slot) noexcept {
    if (slot.data != nullptr) {
        heap_caps_free(slot.data);
        slot.data = nullptr;
    }
    slot.info = PhotoInfo{};
}

std::uint32_t PhotoStore::store(const std::uint8_t* jpeg, std::size_t length, const char* reason,
                                std::uint16_t score_permille,
                                std::int64_t captured_at_ms) noexcept {
    if (jpeg == nullptr || length == 0 || lock_ == nullptr) {
        return 0;
    }
    std::uint8_t* data =
        static_cast<std::uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (data == nullptr) {
        data = static_cast<std::uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_8BIT));
    }
    if (data == nullptr) {
        ESP_LOGE(tag, "photo store alloc failed bytes=%u", static_cast<unsigned>(length));
        return 0;
    }
    std::memcpy(data, jpeg, length);

    if (xSemaphoreTake(lock_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        heap_caps_free(data);
        return 0;
    }
    Slot& slot = slots_[next_];
    if (slot.data != nullptr) {
        evict(slot);
        head_ = (head_ + 1) % capacity_;
    } else {
        ++count_;
    }
    slot.data = data;
    slot.info.id = next_id_++;
    slot.info.reason = reason;
    slot.info.score_permille = score_permille;
    slot.info.length = static_cast<std::uint32_t>(length);
    slot.info.captured_at_ms = captured_at_ms;
    const std::uint32_t id = slot.info.id;
    next_ = (next_ + 1) % capacity_;
    ++captures_;
    if (reason != nullptr && std::strcmp(reason, "motion") == 0) {
        ++motion_events_;
    }
    xSemaphoreGive(lock_);
    return id;
}

int PhotoStore::count() const noexcept {
    return count_;
}

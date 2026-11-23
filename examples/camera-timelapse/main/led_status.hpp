#pragma once

#include <atomic>
#include <cstdint>

enum class LedPhase : std::uint8_t {
    idle,
    capture,
    motion,
    ota_pending,
    ota_updating,
    ota_ok,
    ota_failed
};

inline std::atomic<LedPhase> led_phase{LedPhase::idle};

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ota {

inline constexpr std::size_t app_image_prefix_size = 80;

enum class ImageState {
    unknown,
    valid,
    pending_verify,
    invalid,
    aborted,
    new_image
};

enum class ConfirmDecision {
    none,
    hold,
    confirm,
    refuse
};

struct BootConfirmInput {
    ImageState running_state{ImageState::unknown};
    bool wifi_connected{false};
    bool self_check_ready{false};
    bool self_check_passed{false};
};

// Decide whether a freshly booted image may cancel rollback.
// Confirmation requires a pending/new image, a completed self-check, and
// Wi-Fi. A completed check that failed, or a missing association, refuses
// the image. Broker reachability is not an input.
ConfirmDecision decide_boot_confirm(const BootConfirmInput& input) noexcept;

struct AppImagePrefix {
    bool valid{false};
    char version[32]{};
};

// Inspect the first bytes of an ESP-IDF application image: 0xE9 magic,
// app-descriptor magic 0xABCD5432 at offset 32, and a NUL-terminated
// ASCII version at offset 48.
AppImagePrefix inspect_app_image_prefix(std::span<const std::uint8_t> prefix) noexcept;

} // namespace ota

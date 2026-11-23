#include "ota/ota.hpp"

namespace ota {

ConfirmDecision decide_boot_confirm(const BootConfirmInput& input) noexcept {
    const bool pending = input.running_state == ImageState::pending_verify ||
                         input.running_state == ImageState::new_image;
    if (!pending) {
        return ConfirmDecision::none;
    }
    if (!input.self_check_ready) {
        return ConfirmDecision::hold;
    }
    if (input.wifi_connected && input.self_check_passed) {
        return ConfirmDecision::confirm;
    }
    return ConfirmDecision::refuse;
}

AppImagePrefix inspect_app_image_prefix(std::span<const std::uint8_t> prefix) noexcept {
    AppImagePrefix out{};
    if (prefix.size() < app_image_prefix_size || prefix[0] != 0xE9) {
        return out;
    }
    const std::uint32_t magic = static_cast<std::uint32_t>(prefix[32]) |
                                (static_cast<std::uint32_t>(prefix[33]) << 8) |
                                (static_cast<std::uint32_t>(prefix[34]) << 16) |
                                (static_cast<std::uint32_t>(prefix[35]) << 24);
    if (magic != 0xABCD5432u) {
        return out;
    }
    const auto field = prefix.subspan(48, 32);
    if (field[0] == 0) {
        return out;
    }
    std::size_t length = 0;
    bool saw_nul = false;
    for (; length < field.size(); ++length) {
        if (field[length] == 0) {
            saw_nul = true;
            break;
        }
        if (field[length] < 0x20 || field[length] > 0x7E) {
            return out;
        }
        out.version[length] = static_cast<char>(field[length]);
    }
    if (!saw_nul) {
        out.version[0] = '\0';
        return out;
    }
    out.version[length] = '\0';
    out.valid = length > 0;
    return out;
}

} // namespace ota

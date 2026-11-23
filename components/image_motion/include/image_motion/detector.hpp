#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace image_motion {

using Milliseconds = std::int64_t;

struct MotionConfig {
    std::uint8_t pixel_threshold{24};
    std::uint16_t changed_permille{40};
    Milliseconds cooldown_ms{5000};
    bool compensate_brightness{true};
};

enum class MotionDecision {
    none,
    triggered,
    cooldown,
    ignored
};

struct MotionResult {
    MotionDecision decision{MotionDecision::none};
    std::uint32_t changed_pixels{0};
    std::uint32_t total_pixels{0};
    std::uint16_t score_permille{0};
    bool has_previous{false};
};

// Scores motion between consecutive grayscale frames. A global brightness
// shift can be subtracted first so auto-exposure is less likely to look like
// movement. The first frame only seeds the previous image.
class Detector {
    public:
    explicit Detector(MotionConfig config) noexcept;

    void set_config(MotionConfig config) noexcept {
        config_ = config;
    }

    MotionConfig config() const noexcept {
        return config_;
    }

    void reset() noexcept;

    MotionResult evaluate(std::span<const std::uint8_t> frame, std::size_t width,
                          std::size_t height, Milliseconds now) noexcept;

    private:
    MotionConfig config_{};
    std::vector<std::uint8_t> previous_{};
    std::size_t width_{0};
    std::size_t height_{0};
    bool has_previous_{false};
    Milliseconds last_trigger_{-1};
};

} // namespace image_motion

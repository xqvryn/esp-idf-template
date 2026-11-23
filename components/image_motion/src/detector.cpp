#include "image_motion/detector.hpp"

namespace image_motion {

namespace {

std::int32_t mean_luma(std::span<const std::uint8_t> pixels) noexcept {
    if (pixels.empty()) {
        return 0;
    }
    std::uint64_t sum = 0;
    for (const std::uint8_t pixel : pixels) {
        sum += pixel;
    }
    return static_cast<std::int32_t>(sum / pixels.size());
}

} // namespace

Detector::Detector(MotionConfig config) noexcept : config_(config) {
}

void Detector::reset() noexcept {
    previous_.clear();
    width_ = 0;
    height_ = 0;
    has_previous_ = false;
    last_trigger_ = -1;
}

MotionResult Detector::evaluate(std::span<const std::uint8_t> frame, std::size_t width,
                                std::size_t height, Milliseconds now) noexcept {
    MotionResult result{};
    const std::size_t total = width * height;
    if (width == 0 || height == 0 || frame.size() != total) {
        result.decision = MotionDecision::ignored;
        return result;
    }

    if (!has_previous_ || width_ != width || height_ != height || previous_.size() != total) {
        previous_.assign(frame.begin(), frame.end());
        width_ = width;
        height_ = height;
        has_previous_ = true;
        result.total_pixels = static_cast<std::uint32_t>(total);
        return result;
    }

    const std::int32_t bias =
        config_.compensate_brightness ? (mean_luma(frame) - mean_luma(previous_)) : 0;
    std::uint32_t changed = 0;
    for (std::size_t i = 0; i < total; ++i) {
        const std::int32_t adjusted = static_cast<std::int32_t>(frame[i]) - bias;
        std::int32_t diff = adjusted - static_cast<std::int32_t>(previous_[i]);
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= static_cast<std::int32_t>(config_.pixel_threshold)) {
            ++changed;
        }
    }

    result.has_previous = true;
    result.changed_pixels = changed;
    result.total_pixels = static_cast<std::uint32_t>(total);
    result.score_permille =
        static_cast<std::uint16_t>((static_cast<std::uint64_t>(changed) * 1000U) / total);
    previous_.assign(frame.begin(), frame.end());

    const bool would_trigger = result.score_permille >= config_.changed_permille;
    if (!would_trigger) {
        result.decision = MotionDecision::none;
        return result;
    }
    if (last_trigger_ >= 0 && now >= last_trigger_ && (now - last_trigger_) < config_.cooldown_ms) {
        result.decision = MotionDecision::cooldown;
        return result;
    }
    last_trigger_ = now;
    result.decision = MotionDecision::triggered;
    return result;
}

} // namespace image_motion

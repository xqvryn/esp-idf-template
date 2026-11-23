#include "image_motion/detector.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

constexpr std::size_t kWidth = 16;
constexpr std::size_t kHeight = 16;
constexpr std::size_t kSize = kWidth * kHeight;

std::vector<std::uint8_t> solid(std::uint8_t value) {
    return std::vector<std::uint8_t>(kSize, value);
}

void paint_block(std::vector<std::uint8_t>& frame, std::size_t left, std::size_t top,
                 std::size_t block, std::uint8_t value) {
    for (std::size_t y = 0; y < block; ++y) {
        for (std::size_t x = 0; x < block; ++x) {
            frame[(top + y) * kWidth + (left + x)] = value;
        }
    }
}

image_motion::Detector make_detector(bool compensate = true) {
    image_motion::MotionConfig config{};
    config.pixel_threshold = 16;
    config.changed_permille = 40;
    config.cooldown_ms = 1000;
    config.compensate_brightness = compensate;
    return image_motion::Detector{config};
}

} // namespace

TEST(ImageMotion, SeedsOnTheFirstFrameAndIgnoresABadSize) {
    auto detector = make_detector();
    const auto frame = solid(80);
    const image_motion::MotionResult first = detector.evaluate(frame, kWidth, kHeight, 0);
    EXPECT_EQ(first.decision, image_motion::MotionDecision::none);
    EXPECT_FALSE(first.has_previous);
    EXPECT_EQ(first.score_permille, 0);

    const image_motion::MotionResult bad = detector.evaluate(frame, kWidth, kHeight - 1, 10);
    EXPECT_EQ(bad.decision, image_motion::MotionDecision::ignored);
}

TEST(ImageMotion, IdenticalFramesAreNotMotion) {
    auto detector = make_detector();
    auto frame = solid(80);
    paint_block(frame, 2, 2, 4, 200);
    (void)detector.evaluate(frame, kWidth, kHeight, 0);
    const image_motion::MotionResult second = detector.evaluate(frame, kWidth, kHeight, 100);
    EXPECT_EQ(second.decision, image_motion::MotionDecision::none);
    EXPECT_TRUE(second.has_previous);
    EXPECT_EQ(second.changed_pixels, 0);
    EXPECT_EQ(second.score_permille, 0);
}

TEST(ImageMotion, NoiseBelowThePixelThresholdIsNotMotion) {
    auto detector = make_detector();
    const auto previous = solid(80);
    auto noisy = solid(80 + 5);
    paint_block(noisy, 0, 0, 2, 80 + 8);
    (void)detector.evaluate(previous, kWidth, kHeight, 0);
    const image_motion::MotionResult result = detector.evaluate(noisy, kWidth, kHeight, 100);
    EXPECT_EQ(result.decision, image_motion::MotionDecision::none);
    EXPECT_EQ(result.changed_pixels, 0);
    EXPECT_EQ(result.score_permille, 0);
}

TEST(ImageMotion, AMovedBlockTriggersMotion) {
    auto detector = make_detector();
    auto previous = solid(80);
    paint_block(previous, 1, 1, 6, 200);
    auto current = solid(80);
    paint_block(current, 7, 7, 6, 200);
    (void)detector.evaluate(previous, kWidth, kHeight, 0);
    const image_motion::MotionResult result = detector.evaluate(current, kWidth, kHeight, 200);
    EXPECT_EQ(result.decision, image_motion::MotionDecision::triggered);
    EXPECT_GE(result.score_permille, 40);
    EXPECT_GT(result.changed_pixels, 0);
}

TEST(ImageMotion, AUniformBrightnessShiftIsCompensated) {
    auto detector = make_detector(true);
    auto previous = solid(80);
    paint_block(previous, 2, 2, 4, 160);
    auto shifted = previous;
    for (auto& pixel : shifted) {
        pixel = static_cast<std::uint8_t>(pixel + 20);
    }
    (void)detector.evaluate(previous, kWidth, kHeight, 0);
    const image_motion::MotionResult result = detector.evaluate(shifted, kWidth, kHeight, 200);
    EXPECT_EQ(result.decision, image_motion::MotionDecision::none);
    EXPECT_EQ(result.score_permille, 0);
}

TEST(ImageMotion, CooldownSuppressesASecondTrigger) {
    auto detector = make_detector();
    auto previous = solid(80);
    paint_block(previous, 1, 1, 6, 200);
    auto moved = solid(80);
    paint_block(moved, 7, 7, 6, 200);
    auto moved_again = solid(80);
    paint_block(moved_again, 1, 9, 6, 200);

    (void)detector.evaluate(previous, kWidth, kHeight, 0);
    const image_motion::MotionResult first = detector.evaluate(moved, kWidth, kHeight, 200);
    EXPECT_EQ(first.decision, image_motion::MotionDecision::triggered);

    const image_motion::MotionResult during =
        detector.evaluate(moved_again, kWidth, kHeight, 200 + 500);
    EXPECT_EQ(during.decision, image_motion::MotionDecision::cooldown);

    auto settled = moved_again;
    (void)detector.evaluate(settled, kWidth, kHeight, 200 + 600);
    auto later = solid(80);
    paint_block(later, 9, 1, 6, 200);
    const image_motion::MotionResult after = detector.evaluate(later, kWidth, kHeight, 200 + 1600);
    EXPECT_EQ(after.decision, image_motion::MotionDecision::triggered);
}

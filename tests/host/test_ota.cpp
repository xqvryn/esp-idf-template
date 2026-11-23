#include "ota/ota.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

namespace {

ota::ConfirmDecision decide(ota::ImageState running_state, bool wifi_connected,
                            bool self_check_ready, bool self_check_passed) {
    ota::BootConfirmInput input{};
    input.running_state = running_state;
    input.wifi_connected = wifi_connected;
    input.self_check_ready = self_check_ready;
    input.self_check_passed = self_check_passed;
    return ota::decide_boot_confirm(input);
}

std::array<std::uint8_t, ota::app_image_prefix_size> prefix_with_version(std::string_view version) {
    std::array<std::uint8_t, ota::app_image_prefix_size> bytes{};
    bytes[0] = 0xE9;
    bytes[32] = 0x32;
    bytes[33] = 0x54;
    bytes[34] = 0xCD;
    bytes[35] = 0xAB;
    if (version.size() < 32) {
        std::memcpy(bytes.data() + 48, version.data(), version.size());
    }
    return bytes;
}

} // namespace

TEST(OtaBootConfirm, NoneWhenTheRunningImageIsAlreadySettled) {
    const ota::ImageState settled[] = {
        ota::ImageState::unknown,
        ota::ImageState::valid,
        ota::ImageState::invalid,
        ota::ImageState::aborted,
    };
    for (const ota::ImageState state : settled) {
        EXPECT_EQ(decide(state, true, true, true), ota::ConfirmDecision::none);
        EXPECT_EQ(decide(state, false, true, false), ota::ConfirmDecision::none);
        EXPECT_EQ(decide(state, false, false, false), ota::ConfirmDecision::none);
    }
}

TEST(OtaBootConfirm, HoldsAPendingImageUntilTheSelfCheckIsReady) {
    for (const auto state : {ota::ImageState::pending_verify, ota::ImageState::new_image}) {
        EXPECT_EQ(decide(state, false, false, false), ota::ConfirmDecision::hold);
        EXPECT_EQ(decide(state, true, false, false), ota::ConfirmDecision::hold);
        EXPECT_EQ(decide(state, true, false, true), ota::ConfirmDecision::hold);
    }
}

TEST(OtaBootConfirm, ConfirmsOnlyWhenWifiIsUpAndTheSelfCheckPassed) {
    for (const auto state : {ota::ImageState::pending_verify, ota::ImageState::new_image}) {
        EXPECT_EQ(decide(state, true, true, true), ota::ConfirmDecision::confirm);
    }
}

TEST(OtaBootConfirm, RefusesAReadyCheckThatFailedOrHasNoWifi) {
    for (const auto state : {ota::ImageState::pending_verify, ota::ImageState::new_image}) {
        EXPECT_EQ(decide(state, false, true, false), ota::ConfirmDecision::refuse);
        EXPECT_EQ(decide(state, false, true, true), ota::ConfirmDecision::refuse);
        EXPECT_EQ(decide(state, true, true, false), ota::ConfirmDecision::refuse);
    }
}

TEST(OtaAppImagePrefix, RejectsAShortOrNonImagePrefix) {
    const std::uint8_t too_short[] = {0xE9, 0x00, 0x00};
    EXPECT_FALSE(ota::inspect_app_image_prefix(too_short).valid);

    auto bytes = prefix_with_version("1.0.0");
    bytes[0] = 0xEA;
    EXPECT_FALSE(ota::inspect_app_image_prefix(bytes).valid);

    bytes = prefix_with_version("1.0.0");
    bytes[32] = 0x00;
    EXPECT_FALSE(ota::inspect_app_image_prefix(bytes).valid);
}

TEST(OtaAppImagePrefix, RejectsAMissingOrNonAsciiVersion) {
    auto empty = prefix_with_version("");
    EXPECT_FALSE(ota::inspect_app_image_prefix(empty).valid);

    auto unterminated = prefix_with_version("1.0.0");
    std::memset(unterminated.data() + 48, 'A', 32);
    EXPECT_FALSE(ota::inspect_app_image_prefix(unterminated).valid);

    auto bad = prefix_with_version("1.0.0");
    bad[50] = 0x01;
    EXPECT_FALSE(ota::inspect_app_image_prefix(bad).valid);
}

TEST(OtaAppImagePrefix, ReadsAValidApplicationVersion) {
    const auto bytes = prefix_with_version("1.0.0-dev.2");
    const ota::AppImagePrefix prefix = ota::inspect_app_image_prefix(bytes);
    ASSERT_TRUE(prefix.valid);
    EXPECT_STREQ(prefix.version, "1.0.0-dev.2");
}

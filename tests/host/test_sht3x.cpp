#include "drivers/sht3x.hpp"

#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

class Bus final : public drivers::I2cBus {
    public:
    bool write_ok{true};
    bool read_ok{true};
    std::vector<std::uint8_t> command;
    std::array<std::uint8_t, 6> frame{0xbe, 0xef, 0x92, 0xbe, 0xef, 0x92};

    bool write(std::span<const std::uint8_t> bytes) noexcept override {
        command.assign(bytes.begin(), bytes.end());
        return write_ok;
    }

    bool read(std::span<std::uint8_t> bytes) noexcept override {
        std::copy(frame.begin(), frame.end(), bytes.begin());
        return read_ok;
    }
};

class Wait final : public drivers::Delay {
    public:
    std::int64_t duration{0};

    void sleep_for(std::int64_t value) noexcept override {
        duration = value;
    }
};

TEST(Sht3x, DecodesManufacturerCrcExampleAndPhysicalUnits) {
    Bus bus;
    const auto sample = drivers::decode_sht3x(bus.frame);
    ASSERT_TRUE(sample.valid);
    EXPECT_NEAR(sample.temperature_celsius, 85.527, 0.01);
    EXPECT_NEAR(sample.relative_humidity_percent, 74.585, 0.01);
}

TEST(Sht3x, RejectsEitherCorruptedChecksumAndWrongLength) {
    Bus bus;
    bus.frame[2] ^= 1;
    EXPECT_FALSE(drivers::decode_sht3x(bus.frame).valid);
    bus.frame[2] ^= 1;
    bus.frame[5] ^= 1;
    EXPECT_FALSE(drivers::decode_sht3x(bus.frame).valid);
    EXPECT_FALSE(drivers::decode_sht3x(std::span(bus.frame).first(5)).valid);
}

TEST(Sht3x, SendsSingleShotCommandAndWaitsOutsideControl) {
    Bus bus;
    Wait wait;
    drivers::Sht3xSensor sensor(bus, wait);
    const auto sample = sensor.measure();
    EXPECT_EQ(bus.command, (std::vector<std::uint8_t>{0x24, 0x00}));
    EXPECT_GE(wait.duration, 16);
    EXPECT_TRUE(sample.valid);
    EXPECT_NEAR(sample.temperature_celsius, 85.527, 0.01);
}

TEST(Sht3x, BusFailureNeverReturnsAValidSample) {
    Bus bus;
    Wait wait;
    drivers::Sht3xSensor sensor(bus, wait);
    bus.write_ok = false;
    EXPECT_FALSE(sensor.measure().valid);
    EXPECT_EQ(wait.duration, 0);
    bus.write_ok = true;
    bus.read_ok = false;
    EXPECT_FALSE(sensor.measure().valid);
}

} // namespace

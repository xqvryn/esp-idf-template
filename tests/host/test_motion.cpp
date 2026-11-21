#include "motion/dshot.hpp"
#include "motion/imu.hpp"
#include "motion/pins.hpp"
#include "motion/pwm.hpp"
#include "motion/rc.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>

namespace {

class ScriptedBus final : public motion::ByteBus {
    public:
    explicit ScriptedBus(std::uint8_t expect, std::span<const std::uint8_t> frame)
        : expect_(expect), frame_(frame) {
    }

    bool write_read(std::uint8_t reg, std::span<std::uint8_t> out) noexcept override {
        if (reg != expect_ || out.size() != frame_.size()) {
            return false;
        }
        for (std::size_t index = 0; index < frame_.size(); ++index) {
            out[index] = frame_[index];
        }
        return true;
    }

    private:
    std::uint8_t expect_;
    std::span<const std::uint8_t> frame_;
};

TEST(ImuDecode, Mpu6050OneGAndIcmBurstUseTheDatasheetScales) {
    std::uint8_t mpu[14] = {0, 0, 0, 0, 0x40, 0x00, 0, 0, 0, 0, 0, 0, 0x00, 0x83};
    motion::ImuSample sample;
    ASSERT_TRUE(motion::decode_mpu6050(mpu, motion::mpu6050_scale_2g_250dps, sample));
    EXPECT_NEAR(sample.accel_m_s2[2], 9.80665F, 0.001F);
    EXPECT_NEAR(sample.gyro_rad_s[2], 0.0174533F, 0.0001F);
    EXPECT_TRUE(sample.valid);

    std::uint8_t icm[12] = {0, 0, 0, 0, 0x08, 0x00, 0, 0, 0, 0, 0, 0};
    ASSERT_TRUE(motion::decode_icm42688(icm, motion::icm42688_scale_16g_2000dps, sample));
    EXPECT_NEAR(sample.accel_m_s2[2], 9.80665F, 0.01F);
    EXPECT_FALSE(motion::decode_mpu6050(std::span<const std::uint8_t>(mpu, 13),
                                        motion::mpu6050_scale_2g_250dps, sample));
}

TEST(ImuDevice, ScriptedBusFeedsTheMpuDriver) {
    std::uint8_t frame[14] = {};
    frame[4] = 0x40;
    ScriptedBus bus(0x3B, frame);
    motion::Mpu6050 imu(bus, motion::mpu6050_scale_2g_250dps);
    motion::ImuSample sample;
    ASSERT_TRUE(imu.read(sample));
    EXPECT_GT(sample.accel_m_s2[2], 9.0F);
    motion::SimulatedImu simulated;
    simulated.set(sample);
    motion::ImuSample again;
    ASSERT_TRUE(simulated.read(again));
    EXPECT_NEAR(again.accel_m_s2[2], sample.accel_m_s2[2], 0.001F);
}

TEST(Pwm, ClampsAndConvertsA1500UsPulse) {
    EXPECT_EQ(motion::clamp_pulse_us(900, 1000, 2000), 1000);
    EXPECT_EQ(motion::clamp_pulse_us(2500, 1000, 2000), 2000);
    std::uint32_t duty = 0;
    ASSERT_TRUE(motion::pulse_to_duty(1500, 20000, 14, duty));
    EXPECT_EQ(duty, (1500U * 16383U) / 20000U);
    EXPECT_FALSE(motion::pulse_to_duty(20001, 20000, 14, duty));
    motion::SimulatedPwm pwm;
    ASSERT_TRUE(pwm.write_pulse_us(1, 1500));
    EXPECT_EQ(pwm.pulse_us(1), 1500);
    EXPECT_FALSE(pwm.write_pulse_us(4, 1500));
}

TEST(Dshot, EncodesThrottleAndBitWidths) {
    std::uint16_t frame = 1;
    ASSERT_TRUE(motion::encode_dshot(0, false, frame));
    EXPECT_EQ(frame, 0);
    ASSERT_TRUE(motion::encode_dshot(48, false, frame));
    EXPECT_EQ(frame, 0x606);
    EXPECT_FALSE(motion::encode_dshot(2048, false, frame));
    motion::DshotSymbol symbols[16] = {};
    ASSERT_TRUE(motion::dshot_symbols(0x8000, 16, symbols));
    EXPECT_EQ(symbols[0].high_ticks, 12);
    EXPECT_EQ(symbols[0].low_ticks, 4);
    EXPECT_EQ(symbols[1].high_ticks, 6);
    EXPECT_EQ(symbols[1].low_ticks, 10);
}

TEST(Rc, SbusFailsafeAndPpmSync) {
    std::array<std::uint8_t, 25> frame{};
    frame[0] = 0x0F;
    frame[1] = 0xE8;
    frame[2] = 0x03;
    frame[23] = 0x08;
    motion::RcSample sample;
    ASSERT_TRUE(motion::decode_sbus(frame, sample));
    EXPECT_EQ(sample.channel_us[0], 1000);
    EXPECT_TRUE(sample.failsafe);
    EXPECT_FALSE(sample.valid);
    frame[23] = 0;
    ASSERT_TRUE(motion::decode_sbus(frame, sample));
    EXPECT_TRUE(sample.valid);
    const int pulses[] = {1100, 1500, 3000, 1600};
    ASSERT_TRUE(motion::decode_ppm(pulses, sample));
    EXPECT_EQ(sample.count, 2);
    EXPECT_EQ(sample.channel_us[1], 1500);
    const int bad[] = {100};
    EXPECT_FALSE(motion::decode_ppm(bad, sample));
}

TEST(MotionPins, DefaultsDoNotSelectHardware) {
    EXPECT_FALSE(motion::hardware_pins_selected({}));
    motion::MotionPins pins;
    pins.ledc_gpio[2] = 4;
    EXPECT_TRUE(motion::hardware_pins_selected(pins));
}

} // namespace

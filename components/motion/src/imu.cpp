#include "motion/imu.hpp"

#include <cmath>

namespace motion {

namespace {

constexpr float g_m_s2 = 9.80665F;
constexpr float deg_to_rad = 0.0174532925F;
constexpr std::uint8_t mpu6050_burst = 0x3B;
constexpr std::uint8_t icm42688_burst = 0x1F;

std::int16_t be_i16(std::uint8_t hi, std::uint8_t lo) noexcept {
    return static_cast<std::int16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}

bool scale_ok(ImuScale scale) noexcept {
    return scale.accel_lsb_per_g > 0 && std::isfinite(scale.gyro_lsb_per_dps) &&
           scale.gyro_lsb_per_dps > 0.0F;
}

float accel(std::int16_t raw, int lsb_per_g) noexcept {
    return (static_cast<float>(raw) / static_cast<float>(lsb_per_g)) * g_m_s2;
}

float gyro(std::int16_t raw, float lsb_per_dps) noexcept {
    return (static_cast<float>(raw) / lsb_per_dps) * deg_to_rad;
}

bool decode_axes(std::span<const std::uint8_t> accel_bytes,
                 std::span<const std::uint8_t> gyro_bytes, ImuScale scale,
                 ImuSample& sample) noexcept {
    if (!scale_ok(scale) || accel_bytes.size() < 6 || gyro_bytes.size() < 6) {
        sample = {};
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        const auto offset = static_cast<std::size_t>(axis) * 2;
        sample.accel_m_s2[axis] =
            accel(be_i16(accel_bytes[offset], accel_bytes[offset + 1]), scale.accel_lsb_per_g);
        sample.gyro_rad_s[axis] =
            gyro(be_i16(gyro_bytes[offset], gyro_bytes[offset + 1]), scale.gyro_lsb_per_dps);
    }
    sample.valid = true;
    return true;
}

} // namespace

bool decode_mpu6050(std::span<const std::uint8_t> frame, ImuScale scale,
                    ImuSample& sample) noexcept {
    if (frame.size() != 14) {
        sample = {};
        return false;
    }
    return decode_axes(frame.first(6), frame.subspan(8), scale, sample);
}

bool decode_icm42688(std::span<const std::uint8_t> frame, ImuScale scale,
                     ImuSample& sample) noexcept {
    if (frame.size() != 12) {
        sample = {};
        return false;
    }
    return decode_axes(frame.first(6), frame.subspan(6), scale, sample);
}

bool Mpu6050::read(ImuSample& sample) noexcept {
    std::uint8_t frame[14] = {};
    if (!bus_.write_read(mpu6050_burst, frame)) {
        sample = {};
        return false;
    }
    return decode_mpu6050(frame, scale_, sample);
}

bool Icm42688::read(ImuSample& sample) noexcept {
    std::uint8_t frame[12] = {};
    if (!bus_.write_read(icm42688_burst, frame)) {
        sample = {};
        return false;
    }
    return decode_icm42688(frame, scale_, sample);
}

} // namespace motion

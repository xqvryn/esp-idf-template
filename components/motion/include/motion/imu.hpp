#pragma once

#include <cstdint>
#include <span>

namespace motion {

struct ImuSample {
    float accel_m_s2[3]{0, 0, 0};
    float gyro_rad_s[3]{0, 0, 0};
    bool valid{false};
};

// Datasheet counts, not a board setting. The caller names the full-scale it programmed.
struct ImuScale {
    int accel_lsb_per_g{0};
    float gyro_lsb_per_dps{0};
};

inline constexpr ImuScale mpu6050_scale_2g_250dps{16384, 131.0F};
inline constexpr ImuScale icm42688_scale_16g_2000dps{2048, 16.4F};

class ByteBus {
    public:
    virtual ~ByteBus() = default;
    virtual bool write_read(std::uint8_t reg, std::span<std::uint8_t> out) noexcept = 0;
};

class Imu {
    public:
    virtual ~Imu() = default;
    virtual bool read(ImuSample& sample) noexcept = 0;
};

class SimulatedImu final : public Imu {
    public:
    void set(const ImuSample& sample) noexcept {
        sample_ = sample;
    }

    bool read(ImuSample& sample) noexcept override {
        sample = sample_;
        return sample_.valid;
    }

    private:
    ImuSample sample_{};
};

// MPU-6050 burst at ACCEL_XOUT_H: accel, temperature, gyro. Big-endian int16.
bool decode_mpu6050(std::span<const std::uint8_t> frame, ImuScale scale,
                    ImuSample& sample) noexcept;
// ICM-42688 burst at ACCEL_DATA_X1: accel then gyro, no temperature between them.
bool decode_icm42688(std::span<const std::uint8_t> frame, ImuScale scale,
                     ImuSample& sample) noexcept;

class Mpu6050 final : public Imu {
    public:
    Mpu6050(ByteBus& bus, ImuScale scale) noexcept : bus_(bus), scale_(scale) {
    }

    bool read(ImuSample& sample) noexcept override;

    private:
    ByteBus& bus_;
    ImuScale scale_;
};

class Icm42688 final : public Imu {
    public:
    Icm42688(ByteBus& bus, ImuScale scale) noexcept : bus_(bus), scale_(scale) {
    }

    bool read(ImuSample& sample) noexcept override;

    private:
    ByteBus& bus_;
    ImuScale scale_;
};

} // namespace motion

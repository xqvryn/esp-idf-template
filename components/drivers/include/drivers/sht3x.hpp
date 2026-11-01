#pragma once

#include <cstdint>
#include <span>

namespace drivers {

struct EnvironmentSample {
    float temperature_celsius{0};
    float relative_humidity_percent{0};
    bool valid{false};
};

// Bus contract for this driver. Not a board HAL.
class I2cBus {
    public:
    virtual ~I2cBus() = default;
    virtual bool write(std::span<const std::uint8_t> bytes) noexcept = 0;
    virtual bool read(std::span<std::uint8_t> bytes) noexcept = 0;
};

class Delay {
    public:
    virtual ~Delay() = default;
    virtual void sleep_for(std::int64_t duration_ms) noexcept = 0;
};

// SHT30/31/35-DIS single-shot, high repeatability, no clock stretching.
EnvironmentSample decode_sht3x(std::span<const std::uint8_t> frame) noexcept;

class Sht3xSensor {
    public:
    Sht3xSensor(I2cBus& bus, Delay& delay) : bus_(bus), delay_(delay) {
    }

    EnvironmentSample measure() noexcept;

    private:
    I2cBus& bus_;
    Delay& delay_;
};

} // namespace drivers

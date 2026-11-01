#pragma once

#include <cstdint>
#include <span>

namespace platform {

// Boot-lifetime adapter. begin() is explicit; invalid/unset pins never touch GPIO.
class EspI2cDevice {
    public:
    ~EspI2cDevice();
    EspI2cDevice() = default;
    EspI2cDevice(const EspI2cDevice&) = delete;
    EspI2cDevice& operator=(const EspI2cDevice&) = delete;
    bool begin(int sda, int scl, unsigned address, unsigned clock_hz, int timeout_ms) noexcept;
    bool write(std::span<const std::uint8_t> bytes) noexcept;
    bool read(std::span<std::uint8_t> bytes) noexcept;

    private:
    void* bus_{nullptr};
    void* device_{nullptr};
    int timeout_ms_{0};
};

class FreeRtosDelay {
    public:
    static void sleep_for(std::int64_t duration_ms) noexcept;
};

} // namespace platform

#pragma once

#include <cstdint>
#include <span>

namespace motion {

struct RcSample {
    int channel_us[16]{};
    int count{0};
    bool failsafe{false};
    bool valid{false};
};

class Rc {
    public:
    virtual ~Rc() = default;
    virtual bool read(RcSample& sample) noexcept = 0;
};

class SimulatedRc final : public Rc {
    public:
    void set(const RcSample& sample) noexcept {
        sample_ = sample;
    }

    bool read(RcSample& sample) noexcept override {
        sample = sample_;
        return sample_.valid;
    }

    private:
    RcSample sample_{};
};

// 25-byte SBUS frame. Channels are the raw 11-bit values, not a receiver's microseconds.
bool decode_sbus(std::span<const std::uint8_t> frame, RcSample& sample) noexcept;
// Pulse widths in microseconds. A gap of at least 3000 us ends the frame.
bool decode_ppm(std::span<const int> pulses_us, RcSample& sample) noexcept;

} // namespace motion

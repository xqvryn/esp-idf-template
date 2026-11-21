#pragma once

#include "runtime/mailbox.hpp"

#include <cstdint>

namespace channels {

// Latest telemetry sample. A new publish replaces the previous one.
struct Sample {
    std::uint16_t id{0};
    float value{0};
    std::int64_t time_us{0};
};

using SampleMailbox = runtime::LatestMailbox<Sample>;

} // namespace channels

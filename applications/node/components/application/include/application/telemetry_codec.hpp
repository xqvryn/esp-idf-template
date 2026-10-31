#pragma once

#include "domain/types.hpp"

#include <cstddef>
#include <span>

namespace application {

// MQTT JSON capacity shared with the node reporter. Excludes the terminating NUL.
inline constexpr std::size_t telemetry_payload_capacity = 512;

// Returns payload length (excluding NUL), or zero for invalid/insufficient storage.
std::size_t encode_telemetry(const domain::Telemetry& status, std::span<char> buffer) noexcept;

} // namespace application

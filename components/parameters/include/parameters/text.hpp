#pragma once

#include "parameters/registry.hpp"

#include <string_view>

namespace parameters {

enum class ParameterWrite : std::uint8_t {
    applied,
    unknown,
    rejected
};

// Assignment a command handler can call. Does not parse a command envelope.
// Booleans accept true/false/1/0. Integers and reals reject trailing junk.
ParameterWrite write_text(Registry& registry, std::string_view name,
                          std::string_view text) noexcept;

} // namespace parameters

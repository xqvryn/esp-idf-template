#pragma once

#include "parameters/store.hpp"

namespace parameters {

// NVS namespace espkit_param, key blob. Does not touch network settings.
class NvsParameterStore final : public BlobStore {
    public:
    bool load(std::span<std::uint8_t> buffer, std::size_t& size) noexcept override;
    bool store(std::span<const std::uint8_t> bytes) noexcept override;
};

} // namespace parameters

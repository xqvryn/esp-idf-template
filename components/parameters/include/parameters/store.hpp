#pragma once

#include "parameters/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace parameters {

// Persistence port. A missing blob is an empty load (size 0), not a failure.
class BlobStore {
    public:
    virtual ~BlobStore() = default;
    virtual bool load(std::span<std::uint8_t> buffer, std::size_t& size) noexcept = 0;
    virtual bool store(std::span<const std::uint8_t> bytes) noexcept = 0;
};

// Scripted store for host tests. Fixed buffer, no NVS.
class MemoryParameterStore final : public BlobStore {
    public:
    bool load(std::span<std::uint8_t> buffer, std::size_t& size) noexcept override;
    bool store(std::span<const std::uint8_t> bytes) noexcept override;

    private:
    std::uint8_t bytes_[max_blob_bytes]{};
    std::size_t size_{0};
};

bool save_parameters(const Registry& registry, BlobStore& store) noexcept;
bool load_parameters(Registry& registry, BlobStore& store) noexcept;

} // namespace parameters

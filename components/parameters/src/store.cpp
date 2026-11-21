#include "parameters/store.hpp"

#include <cstring>

namespace parameters {

bool MemoryParameterStore::load(std::span<std::uint8_t> buffer, std::size_t& size) noexcept {
    if (buffer.size() < size_) {
        return false;
    }
    if (size_ > 0) {
        std::memcpy(buffer.data(), bytes_, size_);
    }
    size = size_;
    return true;
}

bool MemoryParameterStore::store(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() > max_blob_bytes) {
        return false;
    }
    if (!bytes.empty()) {
        std::memcpy(bytes_, bytes.data(), bytes.size());
    }
    size_ = bytes.size();
    return true;
}

bool save_parameters(const Registry& registry, BlobStore& store) noexcept {
    std::uint8_t buffer[max_blob_bytes];
    const auto written = registry.encode(buffer);
    if (written == 0) {
        return false;
    }
    return store.store(std::span<const std::uint8_t>(buffer, written));
}

bool load_parameters(Registry& registry, BlobStore& store) noexcept {
    std::uint8_t buffer[max_blob_bytes];
    std::size_t size = 0;
    if (!store.load(buffer, size)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    if (size > max_blob_bytes) {
        return false;
    }
    return registry.decode(std::span<const std::uint8_t>(buffer, size));
}

} // namespace parameters

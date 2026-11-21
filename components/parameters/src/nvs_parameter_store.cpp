#include "nvs.h"
#include "nvs_flash.h"
#include "parameters/nvs_store.hpp"

namespace parameters {

namespace {

constexpr char nvs_namespace[] = "espkit_param";
constexpr char nvs_key[] = "blob";

} // namespace

bool NvsParameterStore::load(std::span<std::uint8_t> buffer, std::size_t& size) noexcept {
    size = 0;
    if (nvs_flash_init() != ESP_OK) {
        return false;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_namespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    std::size_t stored = 0;
    const auto probed = nvs_get_blob(handle, nvs_key, nullptr, &stored);
    if (probed == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return true;
    }
    if (probed != ESP_OK || stored > buffer.size() || stored > max_blob_bytes) {
        nvs_close(handle);
        return false;
    }
    const auto read = nvs_get_blob(handle, nvs_key, buffer.data(), &stored);
    nvs_close(handle);
    if (read != ESP_OK) {
        return false;
    }
    size = stored;
    return true;
}

bool NvsParameterStore::store(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() > max_blob_bytes || nvs_flash_init() != ESP_OK) {
        return false;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_namespace, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    const auto wrote = nvs_set_blob(handle, nvs_key, bytes.data(), bytes.size());
    const auto committed = wrote == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return committed;
}

} // namespace parameters

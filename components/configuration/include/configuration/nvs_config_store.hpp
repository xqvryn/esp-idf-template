#pragma once

#include "configuration/network_config.hpp"

namespace configuration {

class NvsConfigStore {
    public:
    // Reads the espkit namespace. Never erases or rewrites NVS.
    // Applies migrate_settings in memory. A false return leaves `config` unchanged.
    bool load(NetworkConfig& config, bool allow_plaintext = false) const noexcept;

    // Reads only wifi_ssid / wifi_pass. Does not require MQTT or identity keys.
    // A false return leaves `config` unchanged.
    bool load_wifi(NetworkConfig& config) const noexcept;

    // Writes only wifi_ssid / wifi_pass. Leaves schema and every other key as-is.
    bool save_wifi(const NetworkConfig& config) const noexcept;
};

} // namespace configuration

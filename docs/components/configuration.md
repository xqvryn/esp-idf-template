# Configuration

`components/configuration` checks network settings and can load them from NVS. It does not start Wi-Fi and it does not include the sensor-node application.

`configuration::NetworkConfig` holds the Wi-Fi SSID and password, the MQTT URI and optional credentials, and the device and site identifiers. `valid_network_config` rejects empty required fields, topic characters in identifiers, and plaintext `mqtt://` unless the caller explicitly allows a development broker.

Stored records use the NVS namespace `espkit`. Schema 1 is the current layout. `migrate_settings` upgrades older records **in memory**. Load does not rewrite NVS. A corrupt or out-of-range record is rejected and left stored.

`NvsConfigStore` is the ESP-IDF adapter. Host tests call the portable validation and migration code and do not open NVS.

Networking depends on this component. You can use storage without networking.

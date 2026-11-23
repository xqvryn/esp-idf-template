# Wi-Fi check

Associates the ESP32-S3 station with a WPA2 network, logs the assigned IP,
RSSI, and channel, and optionally syncs time over SNTP. It reuses ESPKit's
`networking::WifiStation`, `networking::SntpTime`, and
`configuration::NvsConfigStore`. It is a hardware example, not a QEMU test.

The committed defaults leave the SSID and password empty so CI can compile the
project without secrets. Flashing those defaults logs `wifi not configured`
and returns.

## Credentials

Do not commit an SSID or password. Put them in an untracked local defaults
file next to this README:

```text
# examples/wifi-check/sdkconfig.defaults.local
CONFIG_ESPKIT_WIFI_CHECK_SSID="your-ssid"
CONFIG_ESPKIT_WIFI_CHECK_PASSWORD="your-password"
```

`sdkconfig.defaults.local` and `sdkconfig.local` are gitignored. The example
CMakeLists appends either file when it exists. You can also pass the list
explicitly:

```sh
SDKCONFIG_DEFAULTS="examples/wifi-check/sdkconfig.defaults;examples/wifi-check/sdkconfig.defaults.local" \
  idf.py -C examples/wifi-check set-target esp32s3
```

Kconfig names:

- `CONFIG_ESPKIT_WIFI_CHECK_SSID`
- `CONFIG_ESPKIT_WIFI_CHECK_PASSWORD`
- `CONFIG_ESPKIT_WIFI_CHECK_SAVE_TO_NVS` (default `n`)
- `CONFIG_ESPKIT_WIFI_CHECK_SNTP` (default `y`)

If the Kconfig SSID is empty, the example reads `wifi_ssid` / `wifi_pass`
from the existing `espkit` NVS namespace (for example after
`tools/provision_device.py`). When `CONFIG_ESPKIT_WIFI_CHECK_SAVE_TO_NVS` is
enabled, a successful connect writes only those two keys and leaves schema,
MQTT, and identity keys unchanged. The password is never logged.

The onboard station uses WPA2-PSK (`WifiStation`'s usual threshold). The
password must be at least eight characters.

## Build and run

From the repository root, with the board plugged in and a local defaults file
in place:

```sh
idf.py -C examples/wifi-check set-target esp32s3
idf.py -C examples/wifi-check build
idf.py -C examples/wifi-check -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS look
under `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`. This board has two USB-C
ports; try the other port if the monitor stays silent.

## Expected output

With no credentials (including a CI-style build flashed as-is):

```text
wifi not configured
```

With a valid SSID and password:

```text
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
wifi time synced unix=1763890000
```

If association fails, each retry logs a reason and the example keeps trying
for about 90 seconds:

```text
wifi connecting ssid=your-ssid
wifi failed reason=201/NO_AP_FOUND
wifi failed reason=15/4WAY_HANDSHAKE_TIMEOUT
```

The log tag is `wifi-check`. IP, RSSI, channel, Unix time, and disconnect
reasons depend on the access point. The password never appears in the log.

## Validation

CI compiles this project for `esp32s3` with empty credentials. It is not
booted under QEMU, because there is no Wi-Fi radio there.

# Wi-Fi check

`examples/wifi-check` associates the ESP32-S3 station with a WPA2 network using ESPKit's `WifiStation`, logs the assigned address, and can sync time with `SntpTime`. Credentials come from Kconfig or from the existing `espkit` NVS namespace. The password is never logged.

The committed defaults leave `CONFIG_ESPKIT_WIFI_CHECK_SSID` and `CONFIG_ESPKIT_WIFI_CHECK_PASSWORD` empty so CI can compile the example. Put real values in an untracked `examples/wifi-check/sdkconfig.defaults.local` (or `sdkconfig.local`). Those filenames are gitignored, and the example's CMakeLists appends them when present:

```text
CONFIG_ESPKIT_WIFI_CHECK_SSID="your-ssid"
CONFIG_ESPKIT_WIFI_CHECK_PASSWORD="your-password"
```

```sh
idf.py -C examples/wifi-check set-target esp32s3
idf.py -C examples/wifi-check build
idf.py -C examples/wifi-check -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. This board has two USB-C ports; try the other one if the monitor stays silent.

A successful run looks like:

```text
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
wifi time synced unix=1763890000
```

With empty credentials the firmware logs `wifi not configured` and stops. Association failures are logged as `wifi failed reason=<code/name>` and retried for about 90 seconds. `CONFIG_ESPKIT_WIFI_CHECK_SAVE_TO_NVS` (default off) writes only `wifi_ssid` and `wifi_pass` after a successful connect. CI compiles the example. QEMU does not boot it, because there is no Wi-Fi radio there.

# OTA update

Updates this board over Wi-Fi and rolls back if the new image does not come
up healthy. It is written for an ESP32-S3-WROOM CAM / Freenove-style board
(16 MB flash, WS2812 on GPIO 48). It reuses ESPKit's `networking::WifiStation`,
`configuration::NvsConfigStore`, and the portable `ota` helper. It is a
hardware example, not a QEMU test.

The repository already reserves two application slots in the root
`partitions.csv` (`ota_0` and `ota_1`, 0x1E0000 bytes each, plus `otadata`)
and enables `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. This example points at
that table instead of inventing another layout. The table fits a 4 MiB chip
and also works on this 16 MB board; flash above the second slot stays unused.

A new image is only marked valid after it joins Wi-Fi and passes a self-check.
If that fails, the bootloader boots the previous slot again. The onboard LED
shows what is happening: yellow while booting or waiting to confirm, blue
while an image is being written, green once the image is confirmed (one blink
for `ota_0`, two blinks for `ota_1`), and red if this image failed its
self-check and is about to roll back.

Opening the native USB-Serial/JTAG port on the Mac can reboot the chip even
when DTR and RTS are left low. Prefer reading the printed URL, then using the
web page or `tools/ota_push.py`, instead of leaving a serial monitor connected.

The committed defaults leave the SSID and password empty so CI can compile
the project without secrets. Flashing those defaults logs `wifi not configured`
and returns.

## Credentials

Do not commit an SSID or password. Put them in an untracked local defaults
file next to this README:

```text
# examples/ota-update/sdkconfig.defaults.local
CONFIG_ESPKIT_OTA_UPDATE_SSID="your-ssid"
CONFIG_ESPKIT_OTA_UPDATE_PASSWORD="your-password"
```

`sdkconfig.defaults.local` and `sdkconfig.local` are gitignored. The example
CMakeLists appends either file when it exists. The password is never logged.
The station uses WPA2-PSK. The password must be at least eight characters.

After the link is up, the firmware disables Wi-Fi modem sleep
(`WIFI_PS_NONE`). Incoming HTTP requests otherwise fail while the station is
asleep.

Anyone on the same network can push firmware. This is a LAN bring-up example,
not a fleet security model.

## Build, flash, and update

From the repository root, with the board plugged in and a local defaults file
in place:

```sh
idf.py -C examples/ota-update set-target esp32s3
idf.py -C examples/ota-update -DPROJECT_VER=1.0.0-dev.1 build
idf.py -C examples/ota-update -p /dev/cu.usbmodem1101 flash
```

On Linux the serial port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS
look under `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`. This board has two
USB-C ports; try the other port if the monitor stays silent. If you use
esptool directly, prefer `--after watchdog_reset` and `write_flash @flash_args`
from the build directory.

`PROJECT_VER` must be a supported SemVer string of at most 31 bytes (for a
local A/B test, `1.0.0-dev.1` and `1.0.0-dev.2` are valid). Once the station
has an address, open the printed URL or `GET /status`.

To install a second image over Wi-Fi only (no USB flash):

```sh
idf.py -C examples/ota-update -DPROJECT_VER=1.0.0-dev.2 build
python3 tools/ota_push.py --url http://192.0.2.10 --firmware examples/ota-update/build/ota-update.bin
```

Or with curl:

```sh
curl -f -X POST --data-binary @examples/ota-update/build/ota-update.bin \
  http://192.0.2.10/update
```

The firmware listens on port 80:

- `/` — a page with the running version, partition, and an upload form
- `/status` — JSON: `version`, `partition`, `state`
- `POST /update` — stream an ESP-IDF application `.bin` into the next OTA slot
- `POST /pull` — download an image from a URL (`url=...`). HTTPS is the default.
  Plain `http://` pulls require `CONFIG_ESPKIT_OTA_UPDATE_ALLOW_HTTP` (off in
  the committed defaults; LAN testing only)

## Rollback test

Build a third image with **Fail the post-boot self-check** enabled under
**ESPKit OTA update** in `menuconfig` (or set
`CONFIG_ESPKIT_OTA_UPDATE_FAIL_SELF_CHECK=y` in an untracked local defaults
file). Push that image the same way. It will join Wi-Fi, serve `/status` for a
few seconds so you can see the new version, then reboot back into the previous
slot.

## Expected output

```text
running version=1.0.0-dev.1 partition=ota_0 state=valid
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
httpd listening on port 80
ota url=http://192.168.1.42/
status url=http://192.168.1.42/status
```

The log tag is `ota-update`. After a successful upload the board reboots into
the other slot, confirms itself, and logs `image confirmed version=...`.

## Validation

CI compiles this project for `esp32s3` with empty credentials and the
repository partition table. Host tests cover boot confirmation and the
application-image prefix check. QEMU does not boot this example, because it
needs the real Wi-Fi radio and LED.

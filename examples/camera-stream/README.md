# Camera stream

Serves a live JPEG preview from the OV-series camera over HTTP. It is written
for an ESP32-S3-WROOM CAM / Freenove-style board (ESP32-S3-EYE pin map,
16 MB flash, 8 MB octal PSRAM). It reuses ESPKit's `networking::WifiStation`
and `configuration::NvsConfigStore`. It is a hardware example, not a QEMU
test.

On this board, VGA (the default) streams at about 5-6 frames per second.
Switch to QVGA in `menuconfig` if you want a smoother preview. Opening the
native USB-Serial/JTAG port on the Mac can reboot the chip even when DTR and
RTS are left low; prefer reading the printed URL, then browsing it, instead
of leaving a serial monitor connected.

The committed defaults leave the SSID and password empty so CI can compile
the project without secrets. Flashing those defaults logs `wifi not configured`
and returns.

## Credentials

Do not commit an SSID or password. Put them in an untracked local defaults
file next to this README:

```text
# examples/camera-stream/sdkconfig.defaults.local
CONFIG_ESPKIT_CAMERA_STREAM_SSID="your-ssid"
CONFIG_ESPKIT_CAMERA_STREAM_PASSWORD="your-password"
```

`sdkconfig.defaults.local` and `sdkconfig.local` are gitignored. The example
CMakeLists appends either file when it exists. You can also pass the list
explicitly:

```sh
SDKCONFIG_DEFAULTS="examples/camera-stream/sdkconfig.defaults;examples/camera-stream/sdkconfig.defaults.local" \
  idf.py -C examples/camera-stream set-target esp32s3
```

Kconfig names:

- `CONFIG_ESPKIT_CAMERA_STREAM_SSID`
- `CONFIG_ESPKIT_CAMERA_STREAM_PASSWORD`
- `CONFIG_ESPKIT_CAMERA_STREAM_FRAMESIZE_VGA` / `_QVGA` (default VGA)
- `CONFIG_ESPKIT_CAMERA_STREAM_JPEG_QUALITY` (default 12)

If the Kconfig SSID is empty, the example reads `wifi_ssid` / `wifi_pass`
from the existing `espkit` NVS namespace (for example after
`tools/provision_device.py` or a Wi-Fi check save). The password is never
logged. The station uses WPA2-PSK (`WifiStation`'s usual threshold). The
password must be at least eight characters.

After the link is up, the firmware disables Wi-Fi modem sleep
(`WIFI_PS_NONE`). Incoming HTTP requests otherwise fail while the station is
asleep.

## Build and run

From the repository root, with the board plugged in and a local defaults file
in place:

```sh
idf.py -C examples/camera-stream set-target esp32s3
idf.py -C examples/camera-stream build
idf.py -C examples/camera-stream -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the serial port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS
look under `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`. This board has two
USB-C ports; try the other port if the monitor stays silent.

Once the station has an address, open the printed index URL in a browser on
the same network. The firmware listens on port 80:

- `/` — a small preview page that embeds `/stream`
- `/stream` — multipart MJPEG
- `/capture` — a single JPEG

The GPIO pin map, JPEG size, and quality are under **ESPKit camera stream**
in `idf.py -C examples/camera-stream menuconfig`. The default pin map is:

| Function | GPIO |
| --- | --- |
| PWDN | -1 (unused) |
| RESET | -1 (unused) |
| XCLK | 15 |
| SIOD (SDA) | 4 |
| SIOC (SCL) | 5 |
| D0..D7 (Y2..Y9) | 11, 9, 8, 10, 12, 18, 17, 16 |
| VSYNC | 6 |
| HREF | 7 |
| PCLK | 13 |

## Expected output

```text
camera sensor pid=0x26 model=OV2640
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
httpd listening on port 80
stream url=http://192.168.1.42/stream
capture url=http://192.168.1.42/capture
index url=http://192.168.1.42/
```

The log tag is `camera-stream`. The PID, model, IP, RSSI, and channel depend
on the board and access point. If init fails, the firmware logs
`camera init failed err=...` or `wifi not configured` and stops.

## Validation

CI compiles this project for `esp32s3` with empty credentials and the
octal-PSRAM defaults. It is not booted under QEMU, because there is no
camera or Wi-Fi radio there.

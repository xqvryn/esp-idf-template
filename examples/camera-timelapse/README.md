# Camera timelapse

Captures JPEG stills on a schedule, or when a portable motion scorer sees
enough change between downscaled grayscale frames. It is written for an
ESP32-S3-WROOM CAM / Freenove-style board (ESP32-S3-EYE pin map, 16 MB flash,
8 MB octal PSRAM, WS2812 on GPIO 48). It reuses ESPKit's
`networking::WifiStation`, `configuration::NvsConfigStore`, the `ota` helper,
and `image_motion::Detector`. It is a hardware example, not a QEMU test.

Stills live in a PSRAM ring (default 24 slots). There is no SD card path in
this example; frames are lost on reboot or when the ring wraps. The default
pin map is the same ESP32-S3-EYE / common WROOM CAM wiring used by the
camera-stream example.

The firmware also accepts a wireless update at `POST /update`, using the
repository partition table and bootloader rollback. A new image is marked
valid only after Wi-Fi associates. `tools/ota_push.py` works against this
example too.

Opening the native USB-Serial/JTAG port on the Mac can reboot the chip even
when DTR and RTS are left low. Prefer the printed URL, then the web page or
`tools/timelapse_pull.py`, instead of leaving a serial monitor connected.

The committed defaults leave the SSID and password empty so CI can compile
the project without secrets. Flashing those defaults logs `wifi not configured`
and returns.

## Credentials

Do not commit an SSID or password. Put them in an untracked local defaults
file next to this README:

```text
# examples/camera-timelapse/sdkconfig.defaults.local
CONFIG_ESPKIT_CAMERA_TIMELAPSE_SSID="your-ssid"
CONFIG_ESPKIT_CAMERA_TIMELAPSE_PASSWORD="your-password"
```

`sdkconfig.defaults.local` and `sdkconfig.local` are gitignored. The example
CMakeLists appends either file when it exists. The password is never logged.
The station uses WPA2-PSK. The password must be at least eight characters.

After the link is up, the firmware disables Wi-Fi modem sleep
(`WIFI_PS_NONE`). Incoming HTTP requests otherwise fail while the station is
asleep.

## Build and run

From the repository root, with the board plugged in and a local defaults file
in place:

```sh
idf.py -C examples/camera-timelapse set-target esp32s3
idf.py -C examples/camera-timelapse build
idf.py -C examples/camera-timelapse -p /dev/cu.usbmodem1101 flash
```

On Linux the serial port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS
look under `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`. This board has two
USB-C ports. If you use esptool directly, prefer `--after watchdog_reset` and
`write_flash @flash_args` from the build directory.

Once the station has an address, open the printed URL. The firmware listens
on port 80:

- `/` — status, settings, a live preview, and the stored stills
- `/status` — JSON: mode, interval, counts, last motion score, and OTA version
- `/list` — JSON list of stored stills
- `/photo/<id>` — one stored JPEG
- `/preview` — a current JPEG that is not stored
- `/capture` — take a still now and store it
- `POST /settings` — `mode=timelapse|motion`, `interval_s`, `pixel_threshold`,
  `changed_permille`, `cooldown_ms`, `motion_period_ms`
- `POST /update` — stream an ESP-IDF application `.bin` into the next OTA slot

The LED is dim green when idle, white on a timelapse or manual capture, red
on a motion capture, yellow while a new OTA image is waiting to confirm, and
blue while an image is being written.

To pull every still and assemble a movie (ffmpeg required):

```sh
python3 tools/timelapse_pull.py --url http://192.0.2.10 --out /tmp/timelapse --mp4 /tmp/timelapse.mp4
```

Motion scoring compares 1/8-scale grayscale frames (80x60 at VGA) and can
subtract a global brightness shift so auto-exposure is less likely to look
like movement. A moved subject still trips it. Wave a hand in front of the
lens, then refresh `/` or `/status` (`motion_events` and `last_motion_score`).

## Expected output

```text
camera sensor pid=0x26 model=OV2640
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
httpd listening on port 80
timelapse url=http://192.168.1.42/
status url=http://192.168.1.42/status
stored id=1 reason=timelapse bytes=16091 score=0
```

The log tag is `camera-timelapse`.

## Validation

CI compiles this project for `esp32s3` with empty credentials, octal PSRAM,
and the repository partition table. Host tests cover `image_motion::Detector`
(identical frames, noise, a moved block, brightness compensation, cooldown).
QEMU does not boot this example, because it needs the real camera, Wi-Fi
radio, and LED.

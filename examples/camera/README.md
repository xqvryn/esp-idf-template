# Camera

Initializes the OV-series camera, logs the detected sensor, and captures a
short burst of JPEG frames. It is written for an ESP32-S3-WROOM CAM /
Freenove-style board whose pinout matches Espressif's ESP32-S3-EYE map. It
is a hardware example, not a QEMU test.

The sensor model is read from the chip at runtime (often OV2640, OV3660, or
OV5640). This example has not been confirmed on a specific module yet.

## Requirements

ESP-IDF 5.4.2 and the board. This example enables 16 MB flash and 8 MB
**octal** PSRAM (`CONFIG_SPIRAM_MODE_OCT`), which is the usual configuration
for an ESP32-S3R8 / N16R8 module with embedded 8 MB PSRAM. If the camera
initializes but PSRAM fails to map, that assumption would need to be
revisited.

No Wi-Fi credentials are required. This example does not start an MJPEG
stream.

## Build and run

From the repository root, with the board plugged in:

```sh
idf.py -C examples/camera set-target esp32s3
idf.py -C examples/camera build
idf.py -C examples/camera -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the serial port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS
look under `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`. This board has two
USB-C ports; try the other port if the monitor stays silent.

The default pin map is:

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

Change the preset, pins, JPEG size (QVGA or VGA), or frame count under
"ESPKit board camera" in `idf.py -C examples/camera menuconfig`.

## Expected output

On a successful boot you should see lines like:

```text
camera sensor pid=0x26 model=OV2640
camera frame n=1 bytes=8192 w=320 h=240 fmt=jpeg
camera frame n=2 bytes=7900 w=320 h=240 fmt=jpeg
...
camera capture done frames=10 avg_fps=12.34
```

The log tag is `camera`. The PID, model name, JPEG sizes, and frames-per-second
value depend on the sensor and lighting. The default is ten QVGA frames.

If the camera does not start, the firmware logs a clear error and returns:

```text
camera init failed err=0x... (...)
```

## Validation

CI compiles this project for `esp32s3` with the octal-PSRAM defaults. It is
not booted under QEMU because the camera is not present there.

# Camera

`examples/camera` initializes the OV-series camera on an ESP32-S3-WROOM CAM / Freenove-style board, logs the detected sensor, and captures a short burst of JPEG frames. The default pin map is the ESP32-S3-EYE / common WROOM CAM wiring (XCLK 15, SIOD 4, SIOC 5, D0..D7 = 11, 9, 8, 10, 12, 18, 17, 16, VSYNC 6, HREF 7, PCLK 13, PWDN and RESET unused).

The example enables 16 MB flash and 8 MB **octal** PSRAM, which is the usual layout for an ESP32-S3R8 / N16R8 module. The sensor model is read from the chip at runtime.

```sh
idf.py -C examples/camera set-target esp32s3
idf.py -C examples/camera build
idf.py -C examples/camera -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. Pins, JPEG size (QVGA or VGA), and frame count are under **ESPKit board camera** in `menuconfig`.

A successful run looks like:

```text
camera sensor pid=0x26 model=OV2640
camera frame n=1 bytes=8192 w=320 h=240 fmt=jpeg
camera capture done frames=10 avg_fps=12.34
```

The PID, model, JPEG sizes, and frames-per-second value depend on the sensor and lighting. If init fails, the firmware logs `camera init failed err=...` and stops. CI compiles the example with the octal-PSRAM defaults. QEMU does not boot it, because there is no camera there. This example does not start an MJPEG stream.

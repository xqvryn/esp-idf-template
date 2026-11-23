# Board LED

Cycles the onboard WS2812 RGB LED. It is written for an ESP32-S3-WROOM CAM /
Freenove-style board whose factory LED sits on GPIO 48. It is a hardware
example, not a QEMU test.

## Requirements

ESP-IDF 5.4.2 and the board. The default flash size in this example is 16 MB,
matching an N16R8 module. No camera, Wi-Fi credentials, or ESPKit application
components are required.

## Build and run

From the repository root, with the board plugged in:

```sh
idf.py -C examples/board-led set-target esp32s3
idf.py -C examples/board-led build
idf.py -C examples/board-led -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the serial port is often `/dev/ttyACM0` (native USB-Serial/JTAG) or
`/dev/ttyUSB0` (the UART USB-C port). On macOS look under `/dev/cu.usbmodem*`
or `/dev/cu.usbserial*`. This board has two USB-C ports; try the other port if
the monitor stays silent.

The GPIO and LED count are Kconfig options (`ESPKIT_BOARD_LED_GPIO`, default
48, and `ESPKIT_BOARD_LED_COUNT`, default 1). Change them with
`idf.py -C examples/board-led menuconfig` under "ESPKit board LED".

## Expected output

```text
board-led gpio=48 count=1
led color=red
led color=green
led color=blue
led color=white
led color=off
```

The log tag is `board-led`. The sequence repeats. Timestamps and the ESP-IDF
prefix vary. The LED should visibly follow those colors.

## Validation

CI compiles this project for `esp32s3`. It is not booted under QEMU because
the RMT WS2812 path needs the real LED.

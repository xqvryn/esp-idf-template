# Board LED

`examples/board-led` cycles the onboard WS2812 RGB LED. The default data pin is GPIO 48 and the default count is one pixel, which matches the usual LED on an ESP32-S3-WROOM CAM / Freenove-style board and on Espressif's ESP32-S3 DevKits.

```sh
idf.py -C examples/board-led set-target esp32s3
idf.py -C examples/board-led build
idf.py -C examples/board-led -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. This board has two USB-C ports; try the other one if the monitor stays silent. The GPIO and LED count are under **ESPKit board LED** in `menuconfig`.

A successful run logs:

```text
board-led gpio=48 count=1
led color=red
led color=green
led color=blue
led color=white
led color=off
```

The sequence repeats. The log tag is `board-led`. CI compiles the example. QEMU does not boot it, because the RMT path needs the real LED.

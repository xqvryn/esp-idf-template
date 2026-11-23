# Camera stream

`examples/camera-stream` brings up the OV-series camera on an ESP32-S3-WROOM CAM / Freenove-style board, joins a WPA2 network with ESPKit's `WifiStation`, and serves a live JPEG preview over HTTP. The default pin map is the ESP32-S3-EYE / common WROOM CAM wiring (XCLK 15, SIOD 4, SIOC 5, D0..D7 = 11, 9, 8, 10, 12, 18, 17, 16, VSYNC 6, HREF 7, PCLK 13, PWDN and RESET unused).

The example enables 16 MB flash and 8 MB **octal** PSRAM. VGA is the default size and typically streams at about 5-6 frames per second on this board; QVGA is smoother. Credentials come from `CONFIG_ESPKIT_CAMERA_STREAM_SSID` / `CONFIG_ESPKIT_CAMERA_STREAM_PASSWORD` (empty committed defaults) or from the `espkit` NVS namespace. Put secrets in an untracked `examples/camera-stream/sdkconfig.defaults.local` (or `sdkconfig.local`). Those filenames are gitignored, and the example's CMakeLists appends them when present. The password is never logged.

```text
CONFIG_ESPKIT_CAMERA_STREAM_SSID="your-ssid"
CONFIG_ESPKIT_CAMERA_STREAM_PASSWORD="your-password"
```

```sh
idf.py -C examples/camera-stream set-target esp32s3
idf.py -C examples/camera-stream build
idf.py -C examples/camera-stream -p /dev/cu.usbmodem1101 flash monitor
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. This board has two USB-C ports. Opening the native USB-Serial/JTAG port can reboot the chip even when DTR and RTS are left low, so once the firmware has printed the URL you can close the monitor and open the page in a browser.

A successful run looks like:

```text
camera sensor pid=0x26 model=OV2640
wifi connecting ssid=your-ssid
wifi connected ip=192.168.1.42 rssi=-51 channel=6
stream url=http://192.168.1.42/stream
capture url=http://192.168.1.42/capture
index url=http://192.168.1.42/
```

The firmware listens on port 80: `/` is a small preview page, `/stream` is multipart MJPEG, and `/capture` returns a single JPEG. After the link is up it disables Wi-Fi modem sleep (`WIFI_PS_NONE`); incoming HTTP otherwise fails while the station is asleep. Pins, JPEG size, and quality are under **ESPKit camera stream** in `menuconfig`. CI compiles the example with empty credentials. QEMU does not boot it, because there is no camera or Wi-Fi radio there.

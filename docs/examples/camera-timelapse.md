# Camera timelapse

`examples/camera-timelapse` captures JPEG stills on an ESP32-S3-WROOM CAM / Freenove-style board (ESP32-S3-EYE pin map, 16 MB flash, 8 MB octal PSRAM, WS2812 on GPIO 48). In timelapse mode it stores a still every N seconds. In motion mode it decodes each JPEG at 1/8 scale, converts it to grayscale, and asks `image_motion::Detector` whether enough pixels changed. The detector is portable and host-tested; it can subtract a global brightness shift so auto-exposure is less likely to look like movement.

Stills sit in a PSRAM ring (default 24 slots, oldest evicted first). There is no SD card path; frames are lost on reboot. Credentials, `WifiStation`, empty committed SSID/password, and a gitignored `sdkconfig.defaults.local` follow the other hardware examples. The password is never logged. After associate, the firmware disables Wi-Fi modem sleep (`WIFI_PS_NONE`).

The example reuses the repository `partitions.csv` and bootloader rollback flag, and it accepts `POST /update` so later images can be installed with `tools/ota_push.py`. A pending image is marked valid only after Wi-Fi is up.

```sh
idf.py -C examples/camera-timelapse set-target esp32s3
idf.py -C examples/camera-timelapse build
idf.py -C examples/camera-timelapse -p /dev/cu.usbmodem1101 flash
```

Opening the native USB-Serial/JTAG port can reboot the chip even when DTR and RTS are left low. Once the URL is printed, close the monitor and use the web page or `tools/timelapse_pull.py`.

```sh
python3 tools/timelapse_pull.py --url http://192.0.2.10 --out /tmp/timelapse --mp4 /tmp/timelapse.mp4
```

`GET /` is a gallery and settings page. `GET /status` returns JSON (mode, interval, photo counts, `motion_events`, `last_motion_score`, firmware version). `GET /list` and `GET /photo/<id>` expose the ring. `GET /preview` returns a still without storing it. `GET /capture` stores a manual still. `POST /settings` switches mode and thresholds at runtime. The LED flashes white on a scheduled or manual capture and red on a motion capture. CI compiles the example with empty credentials. QEMU does not boot it.

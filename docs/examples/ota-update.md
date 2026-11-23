# OTA update

`examples/ota-update` joins a WPA2 network with ESPKit's `WifiStation` and updates the running application over HTTP. It uses the repository's existing `partitions.csv` (`nvs`, `otadata`, `ota_0`, `ota_1`) and `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` rather than a second layout. Each application slot is 0x1E0000 bytes. That 4 MiB-friendly table also works on the 16 MB ESP32-S3-WROOM CAM / Freenove-style board this example targets (WS2812 on GPIO 48).

A newly written slot boots as pending. The image calls `esp_ota_mark_app_valid_cancel_rollback` only after Wi-Fi is up and a self-check passes. Otherwise it waits a short grace period (so `/status` can still be read) and then `esp_ota_mark_app_invalid_rollback_and_reboot`. The portable `ota::decide_boot_confirm` helper encodes that rule and is covered by host tests. It is not the same function as `decide_boot_health`, which remains the node application's output-based confirmation rule and still does not call the OTA API.

Credentials come from `CONFIG_ESPKIT_OTA_UPDATE_SSID` / `CONFIG_ESPKIT_OTA_UPDATE_PASSWORD` (empty committed defaults) or from the `espkit` NVS namespace. Put secrets in an untracked `examples/ota-update/sdkconfig.defaults.local` (or `sdkconfig.local`). The password is never logged. After the link is up the firmware disables Wi-Fi modem sleep (`WIFI_PS_NONE`).

```sh
idf.py -C examples/ota-update set-target esp32s3
idf.py -C examples/ota-update -DPROJECT_VER=1.0.0-dev.1 build
idf.py -C examples/ota-update -p /dev/cu.usbmodem1101 flash
```

On Linux the port is often `/dev/ttyACM0` or `/dev/ttyUSB0`. This board has two USB-C ports. Opening the native USB-Serial/JTAG port can reboot the chip even when DTR and RTS are left low, so once the firmware has printed the URL you can close the monitor and use the web page or `tools/ota_push.py`.

```sh
idf.py -C examples/ota-update -DPROJECT_VER=1.0.0-dev.2 build
python3 tools/ota_push.py --url http://192.0.2.10 --firmware examples/ota-update/build/ota-update.bin
```

`GET /` shows the running version and partition. `GET /status` returns JSON. `POST /update` streams an application `.bin` into the next slot. `POST /pull` uses `esp_https_ota`; plaintext `http://` URLs are rejected unless `CONFIG_ESPKIT_OTA_UPDATE_ALLOW_HTTP` is enabled (LAN testing only, default off). Anyone on the same network can push firmware; this is a bring-up example, not a fleet security model.

The LED is yellow while booting or waiting to confirm, blue while writing, green when confirmed (one blink is `ota_0`, two blinks are `ota_1`), and red when the self-check failed and rollback is about to run. A rollback test image can be built with `CONFIG_ESPKIT_OTA_UPDATE_FAIL_SELF_CHECK`. CI compiles the example with empty credentials. QEMU does not boot it.

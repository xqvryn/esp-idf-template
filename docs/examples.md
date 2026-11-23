# Examples

Each directory under `examples/` is a standalone ESP-IDF project. Build it with ESP-IDF 5.4.2:

```sh
idf.py -C examples/<name> set-target esp32s3
idf.py -C examples/<name> build
```

CI compiles every example for `esp32s3`. Compilation does not flash a board.

| Example | What it shows |
| --- | --- |
| [Minimal](examples/minimal.md) | One log line. No sensors, networking, or node application |
| [Networking](examples/networking.md) | Wi-Fi/MQTT/SNTP linked without the node application |
| [Sensor node](examples/sensor-node.md) | A synthetic 23.5 reading, sampled on its own task |
| [Actuator node](examples/actuator-node.md) | ON until the one-second run limit latches a fault |
| [Simulation](examples/simulation.md) | Sensor, control, and reporter tasks. Output requested off |
| [Drone](guides/building-a-drone.md) | Quadcopter skeleton. Simulation-verified only, disarmed at boot |
| [Board LED](examples/board-led.md) | Cycles the onboard WS2812 on GPIO 48. Hardware only |
| [Camera](examples/camera.md) | Detects the OV-series sensor and captures JPEG frames. Hardware only |
| [Wi-Fi check](examples/wifi-check.md) | Associates the station, logs IP/RSSI/channel, optional SNTP. Hardware only |
| [Camera stream](examples/camera-stream.md) | HTTP MJPEG preview and still capture on the same board. Hardware only |
| [OTA update](examples/ota-update.md) | HTTP firmware upload and rollback on the same board. Hardware only |

The board LED, camera, Wi-Fi check, camera stream, and OTA update examples target an ESP32-S3-WROOM CAM / Freenove-style board (16 MB flash, 8 MB octal PSRAM, WS2812 on GPIO 48). CI compiles them but does not boot them under QEMU.

## Sensor node

Details and the expected log line are on the [sensor node](examples/sensor-node.md) page.

## Actuator node

Details and the expected log lines are on the [actuator node](examples/actuator-node.md) page.

Reusable components are pulled in with `idf_component.yml` path dependencies. The node application, when an example needs it, is added with `EXTRA_COMPONENT_DIRS`. The minimal and networking examples do not add that directory.

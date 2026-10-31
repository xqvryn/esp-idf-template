# minimal

A standalone ESP-IDF application. It logs one line and does not sample a sensor,
drive an actuator, or open a network connection. It does not compile the ESPKit
`domain` or `application` components.

## Requirements

ESP-IDF 5.4.2 and an ESP32-S3 development board for execution. No external sensor,
actuator, Wi-Fi credentials, or broker is required.

## Build and run

From the repository root:

```sh
idf.py -C examples/minimal set-target esp32s3
idf.py -C examples/minimal build
idf.py -C examples/minimal -p /dev/ttyACM0 flash monitor
```

## Expected output

```text
Hello ESP32!
```

The log tag is `app`. Timestamps and the ESP-IDF prefix vary.

## Validation

CI compiles this project for `esp32s3`. A successful build does not execute it
on hardware.

## Grow it

This project does not put `components/` on the build path. Add a component only
when the application calls it.

1. Point `EXTRA_COMPONENT_DIRS` at the components you want. From
   `examples/minimal/CMakeLists.txt`, before `project()`:

```cmake
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/configuration"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/platform"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/networking")
```

`EXTRA_COMPONENT_DIRS` entries are component directories (each one contains
`CMakeLists.txt`), not the parent `components/` folder. Listing a directory you
do not `REQUIRES` does not by itself make it mandatory, but omitting it keeps
the dependency out of the search path.

2. Require only those components from `main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "app_main.cpp" PRIV_REQUIRES log configuration)
```

| Component | Opt in when you need |
| --- | --- |
| `configuration` | NVS storage for validated settings |
| `platform` | ESP-IDF clock, logging helpers, and board adapters |
| `networking` | Wi-Fi station and MQTT |

`configuration` does not require the sensor/actuator application. `platform` and
`networking` currently implement ports owned by `application`, so requiring
either of them also builds that component. Leave `domain`, `application`, and
`drivers` off the path unless the sensor → control → report execution model is
the one you want.

Chip target (`idf.py set-target`) is a separate choice from which of these
components you require. Board pin maps stay in the board profile, not in this
example.

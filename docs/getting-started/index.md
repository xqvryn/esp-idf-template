# Getting started

Create a new repository from the ESPKit template, then build the minimal example for ESP32-S3.

## 1. Create the repository

```sh
gh repo create my-app --template xqvryn/esp-idf-template --private --clone
cd my-app
```

That copy includes the build, CI, host tests, and docs. It does **not** receive later ESPKit commits. If you already have a project and only want networking or storage, use [components](../guides/components.md) instead of copying the template.

You need [ESP-IDF 5.4.2](installation.md) on your machine, or Docker. The commands below assume the `idf.py` from that release is on your `PATH`.

## 2. Build the minimal example

The minimal example logs `Hello ESP32!`. It does not compile sensors, actuators, networking, or the sensor-node application.

=== "On your machine"

    ```sh
    idf.py -C examples/minimal set-target esp32s3
    idf.py -C examples/minimal build
    ```

=== "In Docker"

    ```sh
    docker run --rm -v "$PWD:/project" -w /project espressif/idf:v5.4.2 \
      bash -c 'idf.py -C examples/minimal set-target esp32s3 && idf.py -C examples/minimal build'
    ```

`set-target esp32s3` selects the chip. It does not select a custom board and it does not select an application profile.

Flash and monitor only after you have a board and a serial port:

```sh
idf.py -C examples/minimal -p /dev/ttyACM0 flash monitor
```

The port name differs by machine. Monitor exits with `Ctrl+]`.

## 3. Pick a next step

| Next step | Where |
| --- | --- |
| Build the root firmware, which defaults to the sensor profile | [Build and run](build-and-run.md) |
| Switch the root firmware to minimal, sensor, or actuator | [Build profiles](../guides/profiles.md) |
| Add Wi-Fi without the sensor workers | [Networking example](../examples/networking.md) |
| Read the dependency rules before editing components | [Boundaries](../concepts/boundaries.md) |

!!! note
    A green build does not mean the binary was run on hardware. CI compiles ESP32-S3 firmware. It does not flash a board.

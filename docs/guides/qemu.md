# Testing with QEMU

ESP-IDF 5.4.2 can boot this firmware in Espressif's QEMU fork. Boot smoke checks that an image starts. Scenario tests drive the sensor firmware over the console and check commands, NVS, faults, and panics. Neither replaces a board test.

## Install

From a machine that already has ESP-IDF 5.4.2:

```sh
sudo apt-get update
sudo apt-get install --yes libslirp0
. "$IDF_PATH/export.sh"
python "$IDF_PATH/tools/idf_tools.py" install qemu-xtensa
. "$IDF_PATH/export.sh"
python -m pip install -r requirements-qemu.txt
```

`idf_tools.py` installs the `qemu-xtensa` package pinned by that ESP-IDF release. The binary needs `libslirp.so.0`. ESP-IDF 5.4.2 lists that package as an ESP32 tool, so an environment installed only for ESP32-S3 does not put it on `PATH`. The runners add `~/.espressif/tools/qemu-xtensa/*/qemu/bin` themselves.

## Boot smoke

```sh
python3 tools/qemu_smoke.py
```

| Image | What a passing boot prints |
| --- | --- |
| `examples/minimal` | `Hello ESP32!` |
| Root `minimal` profile | `Hello ESP32!` |
| Root `actuator` profile | ON, then a latched fault after the one-second run limit |
| Root `sensor` profile | `output_mode=simulated` and `requested=0` |
| `examples/simulation` | The same simulated sensor log |

`python3 tools/qemu_smoke.py --only example-minimal` runs one image.

## Write a scenario

Scenario tests live in `tests/qemu/`. They boot the root **sensor** profile with `CONFIG_ESPKIT_QEMU_HARNESS` enabled (`sdkconfig.defaults.qemu`). The harness is off in production builds. A task reads `espkit ...` lines from the console and answers with `espkit: ...` log lines.

```sh
python3 tests/qemu/run.py
python3 tests/qemu/run.py tests/qemu/test_command.py
```

A new test is a few lines. `run.py` builds the harness image, then pytest-embedded-qemu supplies the `dut` fixture (serial expect, write, and a log under `build/qemu-harness/logs`).

```python
def test_ping(espkit):
    espkit.send("espkit ping", "espkit: pong")
```

`espkit.ready()` already ran. `send` writes a line and waits for a pattern. `wait_status(output, fault)` polls `espkit status` and returns the guest `uptime` in milliseconds. Compare those uptimes. Do not compare them with the host clock: QEMU does not run at wall time.

Useful lines:

| Command | Response |
| --- | --- |
| `espkit on 400` | `command status=accepted now=<ms>` |
| `espkit off` | `command status=accepted` |
| `espkit status` | `output=` `fault=` `state=` `uptime=` |
| `espkit sensor 23.5` or `espkit sensor fail` | changes the synthetic reading |
| `espkit envelope <json>` | `reason=malformed`, `oversize`, or `admission=` |
| `espkit nvs-unversioned` then `espkit restart` | settings survive the reboot and migrate in memory |
| `espkit panic` | abort, backtrace, reboot |

Copy `tests/qemu/` into a downstream project and add a `test_*.py` file. Keep the `espkit ` replies stable so old scenarios still match.

## What is emulated

| Checked here | Not checked here |
| --- | --- |
| Boot, command ingress, `TimedOutput` lease, synthetic sensor failure, NVS bytes across a software reboot, panic backtrace decoded with `addr2line` | Wi-Fi, MQTT, GPIO, I2C, and real actuator timing |

`idf.py qemu` attaches an `open_eth` NIC. This firmware's network stack is a Wi-Fi station plus MQTT, not Ethernet. A broker on the host would not exercise `WifiStation`, the link state machine, or SNTP. Those transitions stay in the host tests. There is no QEMU MQTT job.

The QEMU launch disables the machine watchdog timer (`wdt_disable`). A healthy image was resetting without that flag. Task-watchdog behavior still belongs on a board. The panic scenario is the crash check: `espkit panic` must print a full backtrace, and `xtensa-esp32s3-elf-addr2line` must resolve a frame to `esp_system_abort` or the harness. The first frame must not be an unresolved address.

## How this fits the other tests

Host tests run the same C++ without ESP-IDF, including envelopes, migration, and the lease overflow. QEMU runs the sensor firmware, the console, NVS, and FreeRTOS. Unity tests under `tests/target` still need a board. A green QEMU run is not permission to connect a pump or a radio.

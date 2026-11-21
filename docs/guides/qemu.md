# Run in QEMU

ESP-IDF 5.4.2 can boot this firmware in Espressif's QEMU fork. That checks the boot path without a board. It does not replace a hardware test.

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

`idf_tools.py` installs the `qemu-xtensa` package pinned by that ESP-IDF release. The binary needs `libslirp.so.0`. ESP-IDF 5.4.2 lists that package as an ESP32 tool, so an environment installed only for ESP32-S3 does not put it on `PATH`. `tools/qemu_smoke.py` adds `~/.espressif/tools/qemu-xtensa/*/qemu/bin` itself. CI runs these commands inside the ESP-IDF 5.4.2 image.

## Boot the images

```sh
python3 tools/qemu_smoke.py
```

The script builds and boots five images for `esp32s3`:

| Image | What a passing boot prints |
| --- | --- |
| `examples/minimal` | `Hello ESP32!` |
| Root `minimal` profile | `Hello ESP32!` |
| Root `actuator` profile | ON, then a latched fault after the one-second run limit |
| Root `sensor` profile | `output_mode=simulated` and `requested=0` |
| `examples/simulation` | The same simulated sensor log |

Each run must also print the ROM banner (`ESP-ROM:`) and `cpu_start:`. The script keeps reading for two seconds after those lines so a panic that follows the banner is still caught. It fails on `Guru Meditation`, `abort()`, a backtrace, `Rebooting...`, a stack canary or stack overflow, `task_wdt`, or a load/instruction fault.

`python3 tools/qemu_smoke.py --only example-minimal` runs one image. The sensor and simulation builds are refused if `CONFIG_ESPKIT_NETWORK_ENABLED=y`.

## Limits

QEMU is not a board.

- Wi-Fi is not simulated. `idf.py qemu` attaches an `open_eth` NIC, and this firmware does not use Ethernet. Leave networking off for these runs.
- GPIO, I2C, flash timing, and the task watchdog need real hardware. QEMU is started with the machine watchdog disabled, so a watchdog failure will not show up here.
- There is no serial command prompt. Local commands stay inside the firmware. The actuator image is the boot that shows that path: it keeps requesting ON until `TimedOutput` latches a fault. A UART session cannot submit a command.
- A green QEMU run does not mean a pump, sensor, or radio was exercised.

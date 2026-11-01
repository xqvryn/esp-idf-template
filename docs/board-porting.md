# Board porting

The initial build target is `esp32s3`, with a 4 MiB flash layout and no physical
peripherals. No board pin map has been selected. A build is not hardware validation.

To add a board, document chip, module, flash size, console transport, sensor bus,
output polarity, boot-sensitive pins, and safe output behavior. Use `main/boards/esp32s3/board_config.hpp` and `main/Kconfig.projbuild` and select wiring there rather than scattering pin numbers across
application code. Add the board profile to CI only after its build works.

Chip target, board profile, and application profile are different axes.
`idf.py set-target` selects the chip; it does not select a custom PCB or which
application sources are compiled. ESP32 family members have different radio
and peripheral capabilities. Do not promise every chip supports the same features.

The application profile is `-DESPKIT_PROFILE=minimal|sensor|actuator`. It chooses
sources under `profiles/` and whether the sensor-node components are on the
build path. It does not change the chip and it does not load a board pin map.
See `profiles/README.md`.

Use distinct build and sdkconfig paths for concurrent target builds. Generated
sdkconfig can retain old choices; start a clean configuration when testing defaults.
A `boards/.../sdkconfig.defaults` file is not loaded automatically: explicitly
supply it through SDKCONFIG_DEFAULTS when board profiles are introduced.

# Motion port

`components/motion_port` is the ESP-IDF side of [`motion`](motion.md). `install_*` returns true immediately when the matching GPIOs are still `-1`. It does not program a peripheral in that case.

When a board profile sets a GPIO:

| Function | Peripheral |
| --- | --- |
| `install_i2c` | I2C master, 7-bit, internal pull-ups |
| `install_spi` | SPI2, mode 0, 1 MHz, the chip-select in `spi_cs` |
| `install_ledc` / `write_ledc_pulse` | 50 Hz, 14-bit, pulse clamped to 1000–2000 µs |
| `install_mcpwm` / `write_mcpwm_pulse` | One MCPWM generator, 1 MHz timer, period 20 ms |
| `install_dshot` / `write_dshot` | RMT at 10 MHz. Bit timing comes from `dshot_symbols` |
| `install_sbus` | UART 100000 8E2, RX inverted, not the console UART |

Nothing in the default sensor firmware calls these functions. QEMU does not model an IMU, an ESC, or an RC receiver. Host tests cover the decoders and the simulated backends. A selected pin still needs a board test before a motor is connected.

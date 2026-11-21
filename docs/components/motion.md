# Motion

`components/motion` decodes sensors and actuator commands. It does not open I2C, SPI, LEDC, MCPWM, RMT, or UART. Those calls are in [`motion_port`](motion-port.md), and only after a board profile sets a GPIO to something other than `-1`.

| Piece | Contract |
| --- | --- |
| `Imu` | `read` fills acceleration in m/s² and angular rate in rad/s |
| `Mpu6050` / `Icm42688` | Burst decode. The scale is the full-scale the caller programmed (`mpu6050_scale_2g_250dps`, `icm42688_scale_16g_2000dps`, or another datasheet pair) |
| `SimulatedImu` | Test double. `set` then `read` |
| `PwmOut` | Pulse width in microseconds. `SimulatedPwm` records it |
| `encode_dshot` | 11-bit throttle, telemetry bit, nibble XOR CRC |
| `Rc` | `decode_sbus` returns raw 11-bit channels and the failsafe flag. `decode_ppm` returns pulse widths and stops at a gap of 3000 µs |
| `MotionPins` | Every GPIO defaults to `-1`. `hardware_pins_selected` is false for that value |

No PCB is selected. A `-1` pin is not a claim that a pin works. Do not copy a development board's silkscreen into this struct without measuring it.

`ByteBus` is the transfer this IMU uses. It is not a board HAL. A host test passes a scripted bus. An IDF port can implement the same two calls.

# Components

These components are optional. A project compiles one only when it `REQUIRES` it. None of them includes the sensor-node workers in `applications/node`. The minimal and actuator profiles exclude `runtime`, `parameters`, `channels`, and `runtime_task`.

| Component | Responsibility |
| --- | --- |
| [platform](platform.md) | ESP-IDF clock, I2C, and FreeRTOS queue and task helpers |
| [configuration](configuration.md) | Validated network settings, schema migration, and NVS |
| [networking](networking.md) | Wi-Fi station, SNTP, MQTT client, and the link state machine |
| [drivers](drivers.md) | SHT3x temperature and humidity samples |
| [timed_output](timed-output.md) | Off / running / fault policy with a maximum run and sample age |
| [runtime](runtime.md) | Task spec, service lifecycle, loop budget, SPSC queue, latest mailbox |
| [parameters](parameters.md) | Typed parameters, text assignment, and a separate NVS blob |
| [channels](channels.md) | Structured records and a latest telemetry sample |
| [runtime task](runtime-task.md) | Pinned FreeRTOS periodic task that feeds the loop budget |
| [motion](motion.md) | IMU, PWM, DShot, and RC decoders, plus simulated backends |
| [motion port](motion-port.md) | ESP-IDF peripherals, idle while every GPIO is `-1` |
| [flight](flight.md) | Quadcopter skeleton. Simulation-verified only |

How to depend on them from another repository, including git and registry forms, is in [Use the components](../guides/components.md). Manifest version is `3.2.0`. Publishing is optional and is described in [Publish components to the registry](../guides/component-registry.md).

The sensor-node code under `applications/node` (`domain`, `application`, `node_drivers`, `wiring`) is an example application, not one of these reusable components.

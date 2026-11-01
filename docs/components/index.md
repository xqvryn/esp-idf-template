# Components

These five components are optional. A project compiles one only when it `REQUIRES` it. None of them includes the sensor-node workers in `applications/node`.

| Component | Responsibility |
| --- | --- |
| [platform](platform.md) | ESP-IDF clock, I2C, and FreeRTOS queue and task helpers |
| [configuration](configuration.md) | Validated network settings, schema migration, and NVS |
| [networking](networking.md) | Wi-Fi station, SNTP, MQTT client, and the link state machine |
| [drivers](drivers.md) | SHT3x temperature and humidity samples |
| [timed_output](timed-output.md) | Off / running / fault policy with a maximum run and sample age |

How to depend on them from another repository is in [Use the components](../guides/components.md). Manifest version is `3.0.0`. They are not in the ESP Component Registry.

The sensor-node code under `applications/node` (`domain`, `application`, `node_drivers`, `wiring`) is an example application, not one of these reusable components.

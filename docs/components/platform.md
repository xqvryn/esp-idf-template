# Platform

`components/platform` wraps a few ESP-IDF primitives. It does not include the sensor-node application.

| API | Role |
| --- | --- |
| `platform::EspClock` | Monotonic milliseconds from `esp_timer` |
| `platform::EspI2cDevice` | I2C master. `begin` is explicit. Invalid pins do not touch GPIO |
| `platform::FreeRtosDelay` | Millisecond delay on a FreeRTOS task |
| `platform::FreeRtosSnapshot` | One-slot latest-value queue |
| `platform::FreeRtosFifo` | Bounded FIFO that rejects when full |
| `platform::PeriodicTask` | Calls `step()` on the object you pass. It is not an event bus |

Sensor-node code that needs these objects as application ports uses `applications/node` `wiring`. A project that only needs the clock or a queue can use `platform` directly.

Public headers are under `components/platform/include/platform/`.

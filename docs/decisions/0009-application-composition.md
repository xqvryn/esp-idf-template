# ADR 0009: Template, reusable components, and sensor-node application

Status: accepted. Refines ADR 0003 and ADR 0004. Does not add an event bus or a
generic HAL.

## Context

ESPKit is used in two ways that were packed into one component graph:

1. **Project template.** `gh repo create <name> --template xqvryn/esp-idf-template`
   copies CMake, CI, host tests, formatting, and docs. A copy does not receive
   later upstream changes.
2. **Reusable components.** Another firmware (a camera pipeline, a BLE node, a
   walking robot, an ESP-NOW swarm) should be able to depend on networking,
   storage, or a driver without compiling the sensor/actuator workers.

The sensor path (sample, timed output, report, MQTT telemetry JSON, command
envelope) was reachable from `platform` and `networking` because those adapters
implemented application ports and encoded `domain::Telemetry`. Requiring Wi-Fi
therefore built `SensorWorker`. A second product could not start from the same
build and CI without rewriting that graph.

## Decision

Keep three layers. Select a layer by what you delete or what you depend on.
Do not introduce a service locator, event bus, or board-wide HAL.

| Layer | What it is | Examples |
| --- | --- | --- |
| Project template | Build, CI, formatting, host tests, docs | Root `CMakeLists.txt`, `.github/workflows/`, `tools/` |
| Reusable components | Optional, independently consumable, no dependency on the sensor application | Platform adapter, networking, storage, SHT3x |
| Examples | Applications with their own execution model | `examples/minimal`, sensor node, actuator node |

Classification of the code that exists today:

| Piece | Where it belongs |
| --- | --- |
| Platform adapter (clock, I2C, FreeRTOS queue/task primitives) | Optional component `components/platform` |
| Networking (Wi-Fi, SNTP, MQTT client, link lifecycle) | Optional component `components/networking` |
| Storage (validated settings and NVS) | Optional component `components/configuration` |
| Sensor driver, for example SHT3x | Optional component `components/drivers`. It returns its own sample type |
| Output policy `TimedOutput` | Reusable within the sensor-node `domain` component. Not required by infrastructure |
| Irrigation policy | Smart-farm application only. Not in this template |
| Application workers, command ingress, quiesce, telemetry JSON, command envelope | Sensor/actuator application under `applications/node/` |
| Synthetic sensor and simulated output | That same application (`node_drivers`), not the reusable driver |
| Board configuration | Per board, in the composition root (`main/boards/`), not in a reusable component |

Chip target and board profile stay separate axes. See
[Board porting](../board-porting.md). This decision does not select a PCB and
does not add a board sdkconfig.

`platform` exposes concrete ESP-IDF primitives. `wiring` (device-only, under
`applications/node/`) adapts those primitives to the sensor-node ports.
`PeriodicTask` calls `step()` on the object it is given. It is not a bus.
`NetworkService` moves bytes and owns the link state machine. It does not read
sensors. The node encodes telemetry before `forward()`.

The root firmware and the existing examples still build the same sensor,
control, and report behavior. `examples/minimal` does not add
`applications/node/` to the component path.

## Consequences

Reusable components can be omitted. A project that only needs storage or the
SHT3x driver does not compile workers. `decode_command_envelope` stays in
networking, so that component still links the node application.
Host tests still cover policy, workers, the SHT3x frame parser, settings
migration, the command envelope, and the network state machine.

Breaking source adjustments inside this repository:

- `networking::encode_telemetry` is now `application::encode_telemetry`.
  `NetworkService::forward` accepts the encoded bytes.
- `Sht3xSensor::measure()` returns `drivers::EnvironmentSample`. The composition
  root stamps `domain::Reading`.
- FreeRTOS snapshots and FIFOs no longer inherit application ports. `wiring`
  adapts them for the node.

## Follow-ups

- Component manifests live next to `platform`, `configuration`, `networking`,
  and `drivers`. Examples consume them with path dependencies. They are not
  uploaded to the component registry. See [Use ESPKit](../using-espkit.md).
- `TimedOutput::update` still takes `Reading`. Split that policy from
  measurement types only when a second application needs the state machine
  without the sensor node.
- `LogTelemetry` stays in node wiring. It prints sensor status. General logging
  remains `ESP_LOG`.
- Application profiles are selected with `-DESPKIT_PROFILE=minimal|sensor|actuator`.
  They stay orthogonal to chip target and board profile. See `profiles/README.md`.

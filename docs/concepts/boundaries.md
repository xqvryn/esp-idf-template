# Boundaries

This is the accepted split from [ADR 0009](../decisions/0009-application-composition.md). A component may depend downward in this table. Reusable components do not include the sensor-node application.

| Piece | Where it belongs |
| --- | --- |
| Platform adapter (clock, I2C, FreeRTOS queue and task primitives) | Optional `components/platform` |
| Networking (Wi-Fi, SNTP, MQTT client, link lifecycle) | Optional `components/networking` |
| Storage (validated settings and NVS) | Optional `components/configuration` |
| Sensor driver, for example SHT3x | Optional `components/drivers`. It returns its own sample type |
| Output policy `TimedOutput` | Reusable `components/timed_output`. It sees a sample time, not a sensor `Reading` |
| Irrigation policy | A smart-farm application only. Not in this template |
| Application workers, command ingress, quiesce, telemetry JSON, command envelope | `applications/node/` |
| Synthetic sensor and simulated output | That same application (`node_drivers`), not the reusable driver |
| Board configuration | Per board, in `main/boards/`, not in a reusable component |

`networking` depends on `configuration`. Inbound MQTT data is a `MqttPayloadHandler` of raw topic and payload bytes. `application::decode_command_envelope` calls `CommandIngress` and stays with the node.

`timed_output` does not include `domain`. The control worker checks that a measurement is finite, then passes a sample time into the policy.

!!! warning
    Do not add a generic event bus or a hardware abstraction layer to make the next product fit. Add the component you need, or delete the ones you do not.

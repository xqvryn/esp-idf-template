# Networking

`components/networking` is the Wi-Fi station, SNTP, and MQTT client, plus the state machine that starts them in order and stops them in reverse. It depends on `configuration`. It does not include sensor workers, `domain`, or `timed_output`.

`NetworkService` moves the link through disabled, starting, waiting, running, stopped, and failed. A failed or partial start can be released and retried. `stop` does not read a sensor and does not change an output. Local control keeps running.

`NetworkService::forward` sends opaque bytes when the station link is up. The sensor node encodes `domain::Telemetry` before it calls `forward`.

## Inbound bytes

MQTT does not subscribe to command topics. When a DATA event arrives, `MqttTelemetry` calls `MqttPayloadHandler::on_payload` with the topic, the fragment, its offset, and the total size. The handler must not call `start`, `stop`, or `publish`.

The node application owns decoding. `application::decode_command_envelope` checks one reassembled payload and calls `CommandIngress::submit`. Networking does not.

`examples/networking` is the build that links this component without `applications/node`.

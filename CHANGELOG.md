# Changelog

Firmware tags and GitHub release notes are created by semantic-release when a
`main` or `develop` push is published. This file records the pending stable
release. It is not a second version source: `release.config.mjs` still chooses
the tag from commit history.

## 3.0.0

The last stable tag is `v2.0.0` on `main`. `develop` has since added
`feat!: add a restartable lifecycle for the network stack`, so the configured
analyzer selects **3.0.0** (`v3.0.0`) for the next stable promotion. A develop
pre-release of the same history is `v3.0.0-dev.1` after `main` is merged back
into `develop`.

### Breaking

- Network startup can be stopped and retried without rebooting local control.
- Sensor and actuator workers, telemetry JSON, and `decode_command_envelope`
  live under `applications/node`. Networking no longer links that application.
  Inbound MQTT data is a raw `MqttPayloadHandler`.
- `TimedOutput` is `components/timed_output` and takes a sample time, not a
  sensor `Reading`.
- Component manifest versions move from `0.1.0` to `3.0.0`.

### Added

- `examples/minimal` and application profiles `minimal`, `sensor`, and
  `actuator` (`-DESPKIT_PROFILE`).
- `examples/networking`, which links Wi-Fi/MQTT/SNTP without the node
  application. CI rejects that build if the node libraries appear.
- `idf_component.yml` manifests for `platform`, `configuration`, `networking`,
  `drivers`, and `timed_output`.
- Command ingress, output quiesce, boot-health decision, and versioned network
  settings.

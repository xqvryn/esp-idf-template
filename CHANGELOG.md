# Changelog

Firmware tags and GitHub release notes are created by semantic-release when a
`main` or `develop` push is published. This file is a short record of stable
releases for the documentation site. It is not a second version source:
`release.config.mjs` still chooses the tag from commit history. The GitHub
release for a tag is the full note.

## 3.3.0

Released as `v3.3.0` on `main` (2026-11-22). The simulation-only quadcopter
skeleton gained a 6-DOF host plant, mixer desaturation, tilt and IMU-age
arming checks, a QEMU failsafe log, and a props-off pre-flight checklist. It
is not flight-tested.

## 3.2.0

Released as `v3.2.0` on `main` (2026-11-22). Reusable components gained complete
Component Registry metadata, and CI packs those archives on pull requests. A
tag workflow can publish when `IDF_COMPONENT_API_TOKEN` is already present;
that token is not required to use the repository.

## 3.1.0

Released as `v3.1.0` on `main` (2026-11-21).

### Added

- QEMU boot and scenario CI for the minimal, profile, and simulation images.
- An optional application framework in `runtime`.
- IMU, PWM/DShot, and RC drivers with simulated backends.
- A simulation-only quadcopter skeleton and `examples/drone`.

### Fixed

- Command lease math stays inside `int64`.
- QEMU expects wait for the end of an accepted or status line.

## 3.0.0

Released as `v3.0.0` on `main` (2026-11-01), following `v2.0.0`.

### Breaking

- Network startup can be stopped and retried without rebooting local control.
- Sensor and actuator workers, telemetry JSON, and `decode_command_envelope`
  live under `applications/node`. Networking no longer links that application.
  Inbound MQTT data is a raw `MqttPayloadHandler`.
- `TimedOutput` is `components/timed_output` and takes a sample time, not a
  sensor `Reading`.
- Component manifest versions move from `0.1.0` to `3.0.0`. Later stable
  releases keep those manifests on the same version as the tag.

### Added

- `examples/minimal` and application profiles `minimal`, `sensor`, and
  `actuator` (`-DESPKIT_PROFILE`).
- `examples/networking`, which links Wi-Fi/MQTT/SNTP without the node
  application. CI rejects that build if the node libraries appear.
- `idf_component.yml` manifests for the reusable components under
  `components/`.
- Command ingress, output quiesce, boot-health decision, and versioned network
  settings.

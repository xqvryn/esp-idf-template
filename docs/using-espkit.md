# Use ESPKit

ESPKit has two roles. Pick the one that matches how the firmware should track
upstream.

## Start from the project template

The template copies the build, CI, formatting, host tests, and docs.

```sh
gh repo create smart-farm --template xqvryn/esp-idf-template --private --clone
```

Delete the examples and profiles you do not need. A copy does not receive later
upstream commits. Chip target, board profile, and application profile stay
separate. See [Board porting](board-porting.md) and `profiles/README.md`.

`examples/minimal` is the smallest starting application. It does not compile
sensors, actuators, or networking.

## Depend on the reusable components

An existing project can consume the same components without copying the sensor
workers. Each component has its own `idf_component.yml` with a version,
description, license, and `idf: ">=5.4"`.

| Component | Manifest | Depends on |
| --- | --- | --- |
| `platform` | `components/platform/idf_component.yml` | ESP-IDF |
| `configuration` | `components/configuration/idf_component.yml` | ESP-IDF |
| `networking` | `components/networking/idf_component.yml` | `configuration` |
| `drivers` | `components/drivers/idf_component.yml` | ESP-IDF |

`networking` depends on `configuration` through a path dependency so a build of
this repository resolves the sibling directory. That is the same manifest
mechanism an external project uses.

From another ESP-IDF project, add the components you call to `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.4"
  configuration:
    git: https://github.com/xqvryn/esp-idf-template.git
    path: components/configuration
    version: main
  networking:
    git: https://github.com/xqvryn/esp-idf-template.git
    path: components/networking
    version: main
```

Replace `version: main` with a tag once a release contains the manifest you
need. `git`, `path`, and `version` together select one component inside the
repository. Require the component from CMake with `REQUIRES` or `PRIV_REQUIRES`.
`platform`, `configuration`, `networking`, and `drivers` do not compile the
sensor workers. `examples/networking` is that build. Leave
`applications/node` off the path unless you want the sensor/actuator workers.
A node that accepts MQTT commands installs `networking::MqttPayloadHandler`
and calls `application::decode_command_envelope` itself.

The examples in this repository use a local path instead of `git`, which is the
development form of that dependency:

```yaml
dependencies:
  platform:
    path: "../../../components/platform"
```

`override_path` is the other local form. It replaces a versioned dependency
with a directory. Use it while editing a component that an example would
otherwise download. `path` is enough when the component is not in the registry.

These manifests are not uploaded to the ESP Component Registry. Publication
needs a maintainer account. Do not add a registry dependency until that upload
exists.

## Versions

Firmware versions are calculated by semantic-release. See
[Releases](releases.md) and the root `CHANGELOG.md`. The `version` field in
each component manifest matches the pending stable release, currently `3.0.0`.
Nothing has been uploaded to the component registry or tagged by this change.
Pin an external git dependency to `v3.0.0` only after that tag exists. A git
dependency on `main` tracks the integration branch and is not a release.

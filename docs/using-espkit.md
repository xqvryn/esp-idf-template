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
| `networking` | `components/networking/idf_component.yml` | `configuration`, and the node application because `decode_command_envelope` calls `CommandIngress` |
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
`platform`, `configuration`, and `drivers` do not compile the sensor workers.
`networking` still does: `decode_command_envelope` calls `CommandIngress`, so
also put `applications/node/components` on the component path (or add `domain`
and `application` the same way) when you depend on `networking`. Leave that
directory out when you do not use networking or the sensor/actuator workers.

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
[Releases](releases.md). The `version` field in each component manifest is the
component version a registry upload would use. It is `0.1.0` until a maintainer
publishes. Bump that field when the component's public API changes, and tag the
repository before asking other projects to pin it. A git dependency on `main`
tracks the integration branch and is not a release.

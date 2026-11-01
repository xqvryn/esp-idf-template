# Use the components

Copy the GitHub template when you want a new firmware repository. Depend on individual components when a project already exists.

## From a new repository

```sh
gh repo create my-app --template xqvryn/esp-idf-template --private --clone
```

Delete the examples you do not need. Upstream fixes do not flow into that copy.

## From an existing ESP-IDF project

Each reusable component has an `idf_component.yml` with version `3.0.0`, a description, the MIT license, and `idf: ">=5.4"`.

| Component | Manifest | Needs |
| --- | --- | --- |
| `platform` | `components/platform/idf_component.yml` | ESP-IDF |
| `configuration` | `components/configuration/idf_component.yml` | ESP-IDF |
| `networking` | `components/networking/idf_component.yml` | `configuration` |
| `drivers` | `components/drivers/idf_component.yml` | ESP-IDF |
| `timed_output` | `components/timed_output/idf_component.yml` | ESP-IDF |

Add the components you call to `main/idf_component.yml`:

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

Replace `version: main` with a tag after `v3.0.0` exists. `git`, `path`, and `version` select one directory inside the repository. Then `REQUIRES` or `PRIV_REQUIRES` that component from CMake.

These manifests are **not** uploaded to the ESP Component Registry. Do not add a registry dependency until a maintainer publishes them.

Inside this repository, examples use a local path instead of `git`:

```yaml
dependencies:
  platform:
    path: "../../../components/platform"
```

`override_path` is the other local form. It replaces a versioned dependency with a directory while you edit. `path` is enough while the component is not in the registry.

`platform`, `configuration`, `networking`, `drivers`, and `timed_output` do not compile the sensor workers. `examples/networking` is the build that checks that. Leave `applications/node` off the path unless you want those workers.

A node that accepts MQTT commands installs `networking::MqttPayloadHandler` and calls `application::decode_command_envelope` itself. Networking does not decode commands.

# Use the components

Copy the GitHub template when you want a new firmware repository. Depend on individual components when a project already exists.

## From a new repository

```sh
gh repo create my-app --template xqvryn/esp-idf-template --private --include-all-branches --clone
```

That form also copies `main`. See [Create a project](../getting-started/create-project.md) if you only need `develop`.

Delete the examples you do not need. Upstream fixes do not flow into that copy.

## From an existing ESP-IDF project

Each reusable component has an `idf_component.yml` with version `3.2.0`, a description, homepage, documentation, issues, Git repository, `repository_info.path`, the MIT license, a `files` exclude list, and `idf: ">=5.4"`.

| Component | Manifest | Needs |
| --- | --- | --- |
| `platform` | `components/platform/idf_component.yml` | ESP-IDF |
| `configuration` | `components/configuration/idf_component.yml` | ESP-IDF |
| `networking` | `components/networking/idf_component.yml` | `configuration` |
| `drivers` | `components/drivers/idf_component.yml` | ESP-IDF |
| `timed_output` | `components/timed_output/idf_component.yml` | ESP-IDF |
| `runtime` | `components/runtime/idf_component.yml` | ESP-IDF |
| `parameters` | `components/parameters/idf_component.yml` | ESP-IDF |
| `channels` | `components/channels/idf_component.yml` | `runtime` |
| `runtime_task` | `components/runtime_task/idf_component.yml` | `runtime` |
| `motion` | `components/motion/idf_component.yml` | ESP-IDF |
| `motion_port` | `components/motion_port/idf_component.yml` | `motion` |
| `flight` | `components/flight/idf_component.yml` | `motion`, `parameters` |

Add the components you call to `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.4"
  configuration:
    git: https://github.com/xqvryn/esp-idf-template.git
    path: components/configuration
    version: v3.2.0
  networking:
    git: https://github.com/xqvryn/esp-idf-template.git
    path: components/networking
    version: v3.2.0
```

`version: v3.2.0` selects that release tag (use the latest stable tag). The repository is public, so this `git` dependency works without a token. `git`, `path`, and `version` select one directory inside the repository. Then `REQUIRES` or `PRIV_REQUIRES` that component from CMake.

Once a maintainer has published to the ESP Component Registry, the same components can be pulled by name. Until that upload happens, prefer the `git` form above.

```yaml
dependencies:
  idf: ">=5.4"
  xqvryn/configuration:
    version: "^3.2.0"
  xqvryn/networking:
    version: "^3.2.0"
```

How packaging and the optional publish job work is in [Publish components to the registry](component-registry.md).

Inside this repository, examples use a local path instead of `git`:

```yaml
dependencies:
  platform:
    path: "../../../components/platform"
```

`override_path` is the other local form. It replaces a versioned git or registry dependency with a directory while you edit. `path` is enough for an in-tree example.

`platform`, `configuration`, `networking`, `drivers`, `timed_output`, `runtime`, `parameters`, `channels`, and `runtime_task` do not compile the sensor workers. `examples/networking` is the build that checks the network side of that split. Leave `applications/node` off the path unless you want those workers. The minimal profile does not compile the runtime, parameter, or channel components.

A node that accepts MQTT commands installs `networking::MqttPayloadHandler` and calls `application::decode_command_envelope` itself. Networking does not decode commands.

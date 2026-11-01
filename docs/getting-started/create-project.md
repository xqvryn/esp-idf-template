# Create a project

```sh
gh repo create my-app --template xqvryn/esp-idf-template --private --clone
cd my-app
```

The full first build is [Getting started](index.md). In short:

1. Install [ESP-IDF 5.4.2](installation.md).
2. Build `examples/minimal` for `esp32s3`.
3. Rename the root CMake project if you do not want the name `device`.
4. Choose a [profile](../guides/profiles.md) only when you are building the root firmware.

A template copy starts a new Git history. It does not receive upstream ESPKit commits. Use `develop` for contributions to ESPKit itself. A new product can start from `main` after you review that branch. Today `main` is the `v2.0.0` tag. `develop` has later work that is not tagged yet.

## Make the sensor firmware yours

The root `sensor` profile is optional. If you keep it:

- Choose implementations in `main/device.cpp`.
- Set periods in `main/device_config.hpp`.
- Set `output_policy` in that same file.
- Keep credentials out of Git and build artifacts.

CI in a new repository runs without secrets. GitHub Pages and GitHub Releases stay off until you set `ENABLE_PAGES` or `ENABLE_RELEASES`. See [Repository setup](../repository-setup.md).

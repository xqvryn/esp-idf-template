# Create a project

```sh
gh repo create my-app --template xqvryn/esp-idf-template --private --include-all-branches --clone
cd my-app
git checkout main
```

`gh repo create --template` and **Use this template** copy only the template default branch unless every branch is included. This repository's default branch is `develop`. `--include-all-branches` is how a product copy also gets `main`. To contribute a change back to ESPKit, work from `develop`.

The full first build is [Getting started](index.md). In short:

1. Install [ESP-IDF 5.4.2](installation.md).
2. Build `examples/minimal` for `esp32s3`.
3. Rename the root CMake project if you do not want the name `device`.
4. Choose a [profile](../guides/profiles.md) only when you are building the root firmware.

A template copy starts a new Git history. It does not receive later ESPKit commits. Today `main` is the `v3.2.0` tag.

## Make the sensor firmware yours

The root `sensor` profile is optional. If you keep it:

- Choose implementations in `main/device.cpp`.
- Set periods in `main/device_config.hpp`.
- Set `output_policy` in that same file.
- Keep credentials out of Git and build artifacts.

CI in a new repository runs without secrets. GitHub Pages and GitHub Releases stay off until you set `ENABLE_PAGES` or `ENABLE_RELEASES`. See [Repository setup](../repository-setup.md).

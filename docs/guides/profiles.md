# Build profiles

An application profile chooses which sources the **root** firmware compiles. The standalone projects under `examples/` stay separate. `examples/actuator-node` compiles the same `profiles/actuator/app_main.cpp` as the actuator profile so the two cannot drift apart.

## Build one

From the repository root, with ESP-IDF 5.4.2:

```sh
idf.py -DESPKIT_PROFILE=minimal set-target esp32s3
idf.py -DESPKIT_PROFILE=minimal build
```

Use `sensor` or `actuator` the same way. The chip target stays `esp32s3` in these commands. Pass a board defaults file separately when you have one. See [Three axes](../concepts/axes.md).

## What the flag changes

`-DESPKIT_PROFILE` is a CMake cache variable. The root `CMakeLists.txt` uses it to:

1. Add `applications/node/components` only for `sensor` and `actuator`.
2. Exclude components the profile does not call. `minimal` excludes `platform`, `configuration`, `networking`, and `drivers`. `actuator` excludes `configuration`, `networking`, and `drivers`.
3. Append `profiles/<name>/sdkconfig.defaults`, which selects the matching Kconfig choice.
4. Compile that profile's sources from `main/CMakeLists.txt`.

| Profile | Compiled application | Node components | Networking |
| --- | --- | --- | --- |
| `minimal` | `profiles/minimal/app_main.cpp` | No | No |
| `sensor` | `main/device.cpp` | Yes | Yes, still off unless you enable it |
| `actuator` | `profiles/actuator/app_main.cpp` | Yes | No |

The sensor profile is the historical root firmware: a synthetic reading, local control, and a status log. Outputs stay simulated.

!!! note
    Generated `sdkconfig` keeps old choices. Delete `sdkconfig` and the build directory when you change `-DESPKIT_PROFILE` or the chip target.

# Application profiles

An application profile chooses which sources the root firmware compiles.
It is not the chip target and it is not a board pin map.

| Profile | What it builds |
| --- | --- |
| `minimal` | `profiles/minimal/app_main.cpp`. One log line. No sensor, actuator, domain, or networking components |
| `sensor` | The default composition in `main/device.cpp`: sample, local control, and reporting. This is the historical root firmware |
| `actuator` | `profiles/actuator/app_main.cpp`, the same loop as `examples/actuator-node` |

`examples/minimal`, `examples/sensor-node`, and `examples/actuator-node` stay
standalone projects. The actuator example compiles `profiles/actuator/app_main.cpp`
so the profile and the example cannot drift.

## Select a profile

From the repository root, with ESP-IDF 5.4.2:

```sh
idf.py -DESPKIT_PROFILE=minimal set-target esp32s3
idf.py -DESPKIT_PROFILE=minimal build
```

Replace `minimal` with `sensor` or `actuator`. Omit `-DESPKIT_PROFILE` to build
`sensor`.

`idf.py set-target esp32s3` still selects the chip. A board profile, when one
exists, is an extra `sdkconfig` defaults file supplied through
`SDKCONFIG_DEFAULTS`. See [Board porting](../docs/board-porting.md). Pass the
application profile with `-DESPKIT_PROFILE` either way. Do not encode the chip
or the board inside the profile name.

Generated `sdkconfig` keeps the previous choice. Delete `sdkconfig` and the
build directory before switching profiles. The build stops if
`ESPKIT_PROFILE` and the saved Kconfig choice disagree.

## What the flag changes

`-DESPKIT_PROFILE` is a CMake cache variable. The root `CMakeLists.txt` uses
it to:

- add `applications/node/components` only for `sensor` and `actuator`
- exclude reusable components the profile does not call, so they are not compiled
- append `profiles/<name>/sdkconfig.defaults`, which selects the matching
  Kconfig choice
- compile that profile's sources from `main/CMakeLists.txt`

`SDKCONFIG_DEFAULTS` alone does not change the source list. The CMake variable
does. Kconfig records the same choice so `sdkconfig` shows which application
was built.

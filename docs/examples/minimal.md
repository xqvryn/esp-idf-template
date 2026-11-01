# Minimal

`examples/minimal` logs `Hello ESP32!` and returns to the scheduler. It does not compile sensors, actuators, networking, or the sensor-node application.

```sh
idf.py -C examples/minimal set-target esp32s3
idf.py -C examples/minimal build
```

The root [minimal profile](../guides/profiles.md) is a similar program built from the repository root with `-DESPKIT_PROFILE=minimal`. The example is a standalone project, so you can copy it without the sensor firmware.

To add a component later, depend on it from `idf_component.yml` and `REQUIRES`. See [Use the components](../guides/components.md).

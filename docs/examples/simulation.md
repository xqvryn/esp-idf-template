# Simulation

`examples/simulation` runs the shared sensor, control, and reporter composition from `main/device.cpp`. The requested output stays off. The command ingress is constructed and left idle.

```sh
idf.py -C examples/simulation set-target esp32s3
idf.py -C examples/simulation build
```

Illustrative log fields:

```text
requested=0 applied=1 fault=0
```

`applied=1` is the enum value for acknowledged off (`unknown=0`, `off=1`, `on=2`). That is a software acknowledgement, not proof that hardware moved.

This example uses the SDK default flash layout. The root firmware reserves two OTA application slots and does not implement updates.

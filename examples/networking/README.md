# networking

Links the reusable Wi-Fi/MQTT/SNTP component without the sensor-node application.
It does not add `applications/node` to the build. CI fails the job if that
application library appears in the build graph.

## Build

```sh
idf.py -C examples/networking set-target esp32s3
idf.py -C examples/networking build
```

Expected log if flashed: `networking linked delay=500`. The numeric width follows
`unsigned long`. No sensor, actuator, or
command decoder is compiled. Inbound MQTT bytes use `MqttPayloadHandler`. A node
that understands commands calls `application::decode_command_envelope` from its
own component.

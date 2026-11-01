# Networking

`examples/networking` links the Wi-Fi, SNTP, and MQTT component without `applications/node`. It logs a reconnect delay and does not open a socket.

```sh
idf.py -C examples/networking set-target esp32s3
idf.py -C examples/networking build
```

CI reads the generated build graph and fails the job if `libapplication.a`, `libdomain.a`, or `applications/node` appears.

The example depends on `configuration` and `networking` through path entries in `main/idf_component.yml`. Inbound MQTT bytes, when a subscription exists, arrive at `MqttPayloadHandler`. This example does not install a handler and does not decode commands.

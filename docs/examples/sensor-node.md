# Sensor node

`examples/sensor-node` feeds a fixed value, 23.5, into a sensor worker. A task samples every 500 ms. The main task logs the latest sample once a second.

```sh
idf.py -C examples/sensor-node set-target esp32s3
idf.py -C examples/sensor-node build
```

Illustrative log fields:

```text
value=23.5 sampled_at_ms=<uptime> valid=1
```

Timestamps and prefixes vary. The example does not use GPIO, Wi-Fi, or MQTT. It does compile the node application, `platform`, and `timed_output` because the worker types live there. It is smaller than the root sensor profile, which also runs control and reporting tasks.

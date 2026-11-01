# Actuator node

`examples/actuator-node` requests ON in memory and steps the sensor and control workers on the main task every 100 ms. After about one second the run-time limit latches a fault. Later ON requests stay off until restart.

```sh
idf.py -C examples/actuator-node set-target esp32s3
idf.py -C examples/actuator-node build
```

Illustrative log:

```text
output=1 fault=0
output=0 fault=1
```

`output=1` means the simulated output is enabled. `fault=1` means `TimedOutput` latched. Nothing is written to GPIO.

The source file is `profiles/actuator/app_main.cpp`, shared with the root actuator profile. The root [simulation](simulation.md) example is the one that schedules workers on separate tasks.

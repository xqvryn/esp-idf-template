# Drone skeleton

Simulation-only quadcopter loop. It is not flight-tested. See the site page
[Building a drone](../../docs/guides/building-a-drone.md).

```sh
idf.py -C examples/drone set-target esp32s3
idf.py -C examples/drone build
```

The same `profiles/drone/app_main.cpp` is the `drone` application profile.
GPIOs stay at `-1`. Motors are commanded only while the arming state is armed,
and this boot leaves the vehicle disarmed.

A props-off bench list is in [Pre-flight checklist](../../docs/guides/pre-flight-checklist.md).

# Pre-flight checklist / bench test with props off

This is a bench checklist for the simulation-only quadcopter skeleton. It is **not flight-tested**. A green host test or QEMU log is not permission to spin a motor or to fly.

## Before you connect anything

- Remove every propeller. Leave them off for every bench test.
- Confirm `MotionPins` GPIOs are still `-1` unless you are deliberately bringing up a board. `-1` selects the simulated backends and does not program LEDC, MCPWM, RMT, I2C, SPI, or UART.
- Confirm the firmware boots **disarmed**. Motor commands must stay exactly zero until the arming state is `armed`.
- Do not connect a battery to an ESC until the logs show zero commands while disarmed on that board.

## What the software already checks

Arming requires all of:

- the arm switch, after it has been released
- throttle at or below `throttle_arm_max` (default 0.05)
- accelerometer tilt at or below `max_arm_tilt_rad` (default 0.2 rad)
- battery voltage at or above `battery_min_v` (default 10 V)
- no RC-loss flag and no kill switch

A lost RC link, a low battery, an invalid IMU, or a frozen or over-age IMU timestamp enters **failsafe** and forces the motors to zero. Clearing the fault returns to **disarmed**. The arm switch must be released and pressed again. The kill switch forces **disarmed**.

The mixer shifts or scales the four motor commands together so a roll or pitch request is not clipped on one motor only.

## Bench steps, props off

1. Build and run the host tests, including the flight and plant cases: `bash tools/test-host.sh`.
2. Build `examples/drone` for `esp32s3` and, where QEMU is available, boot it. The boot log must contain `simulation only: arm=0 motor_sum=0.00` and `pins=simulated`, then the scripted denials and failsafes with `motor_sum=0.00`.
3. On a restrained airframe with the props still off, confirm the live log stays disarmed at boot and that motor outputs stay at rest until you complete a level, throttle-low arm.
4. Trigger RC loss, an IMU fault, and a low-battery reading separately. The motors must go to zero and must not arm again until the switch is released and pressed.
5. Only after those board tests pass should anyone consider a later, documented, props-off motor spin. That work is outside this skeleton.

Read [Building a drone](building-a-drone.md) and [Safety](../safety.md) before you change a pin or a gain.

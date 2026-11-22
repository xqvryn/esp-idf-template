# Building a drone

This is a skeleton for a quadcopter flight controller. It is **simulation-verified only**. It has not been flight-tested. It has not been bench-tested on a motor.

## Safety

- Do not install propellers.
- Do not connect a battery to an ESC until a later board test says the outputs stay at zero while disarmed.
- The firmware boots **disarmed**. Motor commands are exactly zero unless the arming state is `armed`.
- Arming requires the arm switch, throttle at or below `throttle_arm_max` (default 0.05), accelerometer tilt at or below `max_arm_tilt_rad` (default 0.2 rad), a battery voltage at or above `battery_min_v` (default 10 V), and no RC-loss flag.
- RC loss, a low battery, an invalid IMU, or a frozen or over-age IMU timestamp enters **failsafe** and forces the motors to zero. Clearing the fault returns to **disarmed**. The arm switch must be released and pressed again.
- A kill switch forces **disarmed** and zero motors.
- Every GPIO in `MotionPins` defaults to `-1`. That selects the simulated IMU and does not program LEDC, MCPWM, RMT, I2C, SPI, or UART. Putting a real GPIO number in that struct is a board decision, not something this template has validated.
- QEMU does not model an IMU, a radio, or an ESC. A green QEMU log is not permission to spin a motor.

## What the skeleton contains

`components/flight` is portable:

- a complementary filter on roll and pitch, with yaw integrated from the gyro
- cascaded angle and rate PIDs with anti-windup
- a quad-X mixer (front-right, rear-right, rear-left, front-left) that shifts or scales saturating commands together
- the arming state machine above

Gains and the battery floor are `parameters` entries. `write_text` can change them inside the declared range. The sensor-node command envelope is not modified.

`examples/drone` and the `drone` application profile compile the same `profiles/drone/app_main.cpp`. The boot steps the controller five times on a level simulated IMU, leaves the vehicle disarmed, and logs `motor_sum=0.00`. It then runs a scripted throttle denial, tilt denial, RC-loss failsafe, and frozen-IMU failsafe. GPIOs stay at `-1`.

## What the host simulation checks

`tests/host/test_flight.cpp` still has a simple rotational plant for the original angle tests. `flight::step_plant` is a 6-DOF rigid body with first-order motor lag, IMU noise, and a wind force. It does not model propeller airflow, battery sag, or a flexible frame, and it is not flight-tested.

- From a 0.35 rad roll, with the sticks centered, the angle returns near zero on the simple plant.
- A half stick settles near 0.2 rad on that plant.
- The 6-DOF plant returns toward level through motor lag, sensor noise, and a short wind gust.
- High throttle or a large tilt does not arm. RC loss, low battery, an invalid or frozen IMU, and the kill switch force zero motor commands.
- At full throttle, a roll request still produces a left/right motor differential instead of clipping one side only.

Those passes are not a tune for a real aircraft. Change the gains only with the host tests still passing, then follow the [pre-flight checklist](pre-flight-checklist.md) on a restrained airframe with the props off.

`examples/drone` logs the disarmed boot, then a scripted throttle denial, tilt denial, RC-loss failsafe, and frozen-IMU failsafe. QEMU looks for those lines. That log is not a board test.

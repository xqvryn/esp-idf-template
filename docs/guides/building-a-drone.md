# Building a drone

This is a skeleton for a quadcopter flight controller. It is **simulation-verified only**. It has not been flight-tested. It has not been bench-tested on a motor.

## Safety

- Do not install propellers.
- Do not connect a battery to an ESC until a later board test says the outputs stay at zero while disarmed.
- The firmware boots **disarmed**. Motor commands are exactly zero unless the arming state is `armed`.
- Arming requires the arm switch, throttle at or below `throttle_arm_max` (default 0.05), a battery voltage at or above `battery_min_v` (default 10 V), and no RC-loss flag.
- RC loss or a low battery enters **failsafe** and forces the motors to zero. Clearing the fault returns to **disarmed**. The arm switch must be released and pressed again.
- A kill switch forces **disarmed** and zero motors.
- Every GPIO in `MotionPins` defaults to `-1`. That selects the simulated IMU and does not program LEDC, MCPWM, RMT, I2C, SPI, or UART. Putting a real GPIO number in that struct is a board decision, not something this template has validated.
- QEMU does not model an IMU, a radio, or an ESC. A green QEMU log is not permission to spin a motor.

## What the skeleton contains

`components/flight` is portable:

- a complementary filter on roll and pitch, with yaw integrated from the gyro
- cascaded angle and rate PIDs with anti-windup
- a quad-X mixer (front-right, rear-right, rear-left, front-left)
- the arming state machine above

Gains and the battery floor are `parameters` entries. `write_text` can change them inside the declared range. The sensor-node command envelope is not modified.

`examples/drone` and the `drone` application profile compile the same `profiles/drone/app_main.cpp`. The boot steps the controller five times on a level simulated IMU, leaves the vehicle disarmed, and logs `motor_sum=0.00`.

## What the host simulation checks

`tests/host/test_flight.cpp` runs a rotational rigid body. Thrust is linear in the motor command. It does not model propeller airflow, battery sag, or a flexible frame.

- From a 0.35 rad roll, with the sticks centered, the angle returns near zero.
- A half stick settles near 0.2 rad.
- High throttle does not arm. RC loss, low battery, and the kill switch force zero motor commands.

Those passes are not a tune for a real aircraft. Change the gains only with the host test still passing, then repeat on a restrained airframe with the props off.

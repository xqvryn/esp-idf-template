# Flight

`components/flight` is a quadcopter skeleton. It is simulation-verified only. It is not a flight-tested controller and it does not talk to hardware.

`step_vehicle` updates a complementary roll/pitch estimate, integrates yaw from the gyro, runs cascaded angle and rate PIDs, and mixes a quad X. Motor commands are exactly zero unless the state is `armed`.

Arming requires the arm switch, throttle at or below the limit, tilt at or below `max_arm_tilt_rad`, battery voltage at or above the limit, and no RC-loss or kill input. RC loss, a low battery, an invalid IMU, or a frozen or over-age IMU timestamp enters `failsafe`. The kill switch enters `disarmed`. Both force zero commands. Leaving failsafe does not arm again until the arm switch is released and pressed with the throttle low and the vehicle level.

`define_flight_parameters` / `load_flight_parameters` store the gains and the battery floor in the parameter registry. `write_text` is how a command handler changes them.

Read [Building a drone](../guides/building-a-drone.md) and the [pre-flight checklist](../guides/pre-flight-checklist.md) before connecting a motor.

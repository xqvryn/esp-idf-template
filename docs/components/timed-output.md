# Timed output

`components/timed_output` is a small state machine: **off**, **running**, and **fault**. It does not include a sensor `Reading`, GPIO, Wi-Fi, or FreeRTOS.

`TimedOutput::update` takes three things:

- whether the output is requested on
- a `SampleTime` (`sampled_at` and `valid`)
- the current monotonic time

It refuses to run when the policy limits are not positive, the clock jumps backward, the sample is invalid, the sample is in the future, or the sample is older than `max_sample_age_ms`. A running interval does not grow if the caller keeps requesting ON. When `max_run_ms` elapses, the state latches **fault** and `update` returns false until `reset_fault()`. `reset_fault()` does nothing while the state is still running.

The control worker in `applications/node` performs the sensor-specific check. A missing sample or a non-finite measurement is passed as `valid=false`. The policy then fails closed.

Default limits are 5000 ms maximum run and 2000 ms maximum sample age. Pass an `OutputPolicy` to override them.

This is a software policy. It does not confirm that a physical actuator moved. Read [Safety](../safety.md) before you connect real hardware.

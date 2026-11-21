# Runtime task

`components/runtime_task` is the ESP-IDF binding for [`runtime`](runtime.md). `PinnedPeriodicTask` creates one FreeRTOS task with `xTaskCreatePinnedToCore`, calls `step()` once per period, and records the wake time in a `LoopBudget`.

The period is converted to FreeRTOS ticks. A period shorter than one tick is rejected. With the default 100 Hz tick, the shortest period is 10 ms. Jitter measured by the budget includes that quantization. `esp_timer_get_time` supplies the timestamps.

`stop()` deletes the task. Do not call it while `step()` holds a lock the deleter needs. The component does not restart a failed task for you.

The minimal and actuator profiles exclude this component. A project that does not `REQUIRES` it does not link it.

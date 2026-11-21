# Runtime

`components/runtime` is portable C++. It does not start a FreeRTOS task, open NVS, or talk to a peripheral.

| Type | What it does |
| --- | --- |
| `TaskSpec` | Name, stack, priority, and core. `core` `-1` means unpinned. `task_spec_valid` checks the request |
| `Service` | `start`, `stop`, and `health`. One object you own. Not a registry of services |
| `ScriptedService` | Test double that can fail the next start |
| `ScriptedClock` | Test double you advance in microseconds |
| `LoopBudget` | Deadline misses and jitter from timestamps you pass in |
| `SpscQueue` | Bounded single-producer single-consumer queue. `try_push` fails when full |
| `LatestMailbox` | One writer replaces the last value. `load` is false until the first publish |

`LoopBudget` counts every deadline a late wake skipped. An early wake is negative jitter and is not a miss. A backward timestamp sets `clock_fault` and stops further accounting. The numbers are only as good as the clock you pass. This component does not measure a board.

`SpscQueue` keeps one slot empty so it can tell full from empty. Two threads may use it only as one producer and one consumer.

These types are optional. `examples/minimal` does not include them. The pinned task that applies a `TaskSpec` on ESP-IDF is [`runtime_task`](runtime-task.md).

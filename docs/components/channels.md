# Channels

`components/channels` moves structured values between one writer and one reader. It does not call `ESP_LOG` and it does not publish MQTT.

`RecordChannel` holds `Record` values (time, severity, code, and two floats) in a bounded queue. `emit` returns false and increments `dropped` when the queue is full. The new record is discarded. Older records stay.

`SampleMailbox` is a `LatestMailbox`. A new sample replaces the previous one. `load` returns false until the first publish.

The channel is not a bus. The composition root passes the object into the two tasks that use it.

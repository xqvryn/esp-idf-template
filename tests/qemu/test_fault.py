"""Bad inputs stay inside the command and sensor paths. They must not panic."""


def test_malformed_and_oversize_envelopes_do_not_panic(espkit):
    espkit.send("espkit envelope {", "envelope reason=malformed")
    espkit.send("espkit envelope " + ("{" * 513), "envelope reason=oversize")
    # expires_at is wall time with offset 0, so it is also the monotonic deadline.
    # It must be after boot and within the 15000 ms command TTL.
    payload = (
        '{"schema_version":1,"command_id":"8f36a7b1","type":"set_output","enabled":true,'
        '"issued_at":"1970-01-01T00:00:00Z","expires_at":"1970-01-01T00:00:05Z",'
        '"max_duration_ms":400}'
    )
    espkit.send("espkit envelope " + payload, "admission=accepted")
    try:
        espkit.dut.expect("Backtrace:", timeout=0.5)
    except Exception:
        return
    raise AssertionError("envelope handling panicked")


def test_sensor_failure_latches_fault_without_panic(espkit):
    espkit.send("espkit on 2000", "command status=accepted")
    espkit.wait_status(1, 0)
    espkit.send("espkit sensor fail", "sensor valid=0")
    espkit.wait_status(0, 1, timeout=1)
    try:
        espkit.dut.expect("Backtrace:", timeout=0.4)
    except Exception:
        return
    raise AssertionError("sensor failure panicked")

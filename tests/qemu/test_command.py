"""Local command ingress and TimedOutput expiry, driven over the QEMU console."""


def test_on_expires_without_a_fault(espkit):
    matched = espkit.send(r"espkit on 400", r"command status=accepted now=(\d+)")
    accepted_at = int(matched.group(1))
    running_at = espkit.wait_status(1, 0)
    stopped_at = espkit.wait_status(0, 0)
    assert accepted_at <= running_at < stopped_at
    # Guest milliseconds. One control period is 100 ms, so the 400 ms lease
    # can end a little early or one sample late.
    assert 250 <= stopped_at - accepted_at <= 700


def test_off_clears_a_running_output(espkit):
    espkit.send("espkit on 2000", "command status=accepted")
    espkit.wait_status(1, 0)
    espkit.send("espkit off", "command status=accepted")
    espkit.wait_status(0, 0, timeout=1)

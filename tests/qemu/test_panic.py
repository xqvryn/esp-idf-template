"""A deliberate abort prints a backtrace that addr2line can decode."""


def test_panic_backtrace_decodes_to_the_harness(espkit):
    espkit.dut.write("espkit panic")
    matched = espkit.dut.expect(
        r"Backtrace:(?: 0x[0-9a-fA-F]+:0x[0-9a-fA-F]+)+\r?\n", timeout=15
    )
    decoded = espkit.decode_backtrace(matched.group(0))
    assert "esp_system_abort" in decoded or "qemu_harness" in decoded
    assert "??" not in decoded.splitlines()[0]

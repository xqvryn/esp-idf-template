"""Fixtures for scripted ESPKit scenarios under pytest-embedded-qemu."""

import subprocess
import time

import pytest


class Espkit:
    """Small wrapper a downstream test can use without touching QEMU directly."""

    def __init__(self, dut, elf: str):
        self.dut = dut
        self.elf = elf

    def ready(self, timeout: int = 60) -> None:
        self.dut.expect("espkit: ready", timeout=timeout)

    def send(self, line: str, pattern: str, timeout: int = 10):
        self.dut.write(line)
        return self.dut.expect(pattern, timeout=timeout)

    def status(self, timeout: int = 5) -> tuple[int, int, str, int]:
        self.dut.write("espkit status")
        matched = self.dut.expect(
            r"espkit: output=(\d) fault=(\d) state=(\w+) value=[0-9.eE+-]+ valid=\d uptime=(\d+)",
            timeout=timeout,
        )
        return int(matched.group(1)), int(matched.group(2)), matched.group(3), int(matched.group(4))

    def wait_status(self, output: int, fault: int, timeout: float = 8) -> int:
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.status(timeout=2)
            if last[0] == output and last[1] == fault:
                return last[3]
            time.sleep(0.05)
        raise AssertionError(f"wanted output={output} fault={fault}, last={last}")

    def decode_backtrace(self, line) -> str:
        import re

        if isinstance(line, bytes):
            line = line.decode()
        # Backtrace lines are PC:SP pairs. The stack pointer is not an instruction.
        addresses = re.findall(r"(0x[0-9a-fA-F]+):0x[0-9a-fA-F]+", line)
        if not addresses:
            addresses = re.findall(r"0x[0-9a-fA-F]+", line)
        if not addresses:
            raise AssertionError(f"no addresses in {line!r}")
        return subprocess.check_output(
            ["xtensa-esp32s3-elf-addr2line", "-pf", "-e", self.elf, *addresses],
            text=True,
        )


@pytest.fixture
def espkit(dut, app):
    board = Espkit(dut, app.elf_file)
    board.ready()
    return board

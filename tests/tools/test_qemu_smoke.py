"""Log checks for the QEMU smoke runner. These do not launch QEMU."""
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("qemu_smoke", ROOT / "tools/qemu_smoke.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def boot_log(*lines: str) -> str:
    return "ESP-ROM:esp32s3-20210327\nI (20) cpu_start: Pro cpu start\n" + "\n".join(lines) + "\n"


class QemuLogTests(unittest.TestCase):
    def test_accepts_a_clean_minimal_boot(self):
        self.assertEqual(module.assess(boot_log("I (100) app: Hello ESP32!"), ("Hello ESP32!",)), [])

    def test_rejects_panic_even_after_the_expected_line(self):
        log = boot_log("I (100) app: Hello ESP32!", "Backtrace: 0x40375a61", "Rebooting...")
        problems = module.assess(log, ("Hello ESP32!",))
        self.assertTrue(any("Backtrace:" in problem for problem in problems))

    def test_rejects_abort_stack_overflow_and_watchdog(self):
        for marker in ("abort() was called", "stack overflow in task", "task_wdt: Task watchdog"):
            problems = module.assess(boot_log("I (100) app: Hello ESP32!", marker), ("Hello ESP32!",))
            self.assertTrue(problems, marker)

    def test_rejects_a_boot_that_never_prints_the_application_line(self):
        problems = module.assess(boot_log("I (100) app: still starting"), ("Hello ESP32!",))
        self.assertTrue(any("Hello ESP32!" in problem for problem in problems))

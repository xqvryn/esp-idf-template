#!/usr/bin/env python3
"""Build ESPKit images and boot them under Espressif's ESP32-S3 QEMU."""

import argparse
import os
import re
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
PANIC_MARKERS = (
    "Guru Meditation",
    "abort() was called",
    "Backtrace:",
    "Rebooting...",
    "Stack canary watchpoint",
    "stack overflow",
    "task_wdt",
    "panic'ed",
    "LoadProhibited",
    "InstrFetchProhibited",
)
BOOT_MARKERS = ("ESP-ROM:", "cpu_start:")


@dataclass(frozen=True)
class Case:
    name: str
    project: str
    expect: tuple[str, ...]
    profile: str | None = None
    require_network_disabled: bool = False
    timeout_s: int = 90


CASES = (
    Case("example-minimal", "examples/minimal", ("Hello ESP32!",)),
    Case("profile-minimal", ".", ("Hello ESP32!",), profile="minimal"),
    Case(
        "profile-actuator",
        ".",
        (
            "Simulation only: no GPIO",
            "output=1 fault=0",
            "output=0 fault=1",
        ),
        profile="actuator",
        timeout_s=120,
    ),
    Case(
        "profile-sensor",
        ".",
        ("output_mode=simulated", "requested=0"),
        profile="sensor",
        require_network_disabled=True,
    ),
    Case(
        "example-simulation",
        "examples/simulation",
        ("output_mode=simulated", "requested=0"),
        require_network_disabled=True,
    ),
)


def assess(log: str, expect: tuple[str, ...]) -> list[str]:
    """Return human-readable failures. An empty list means the boot is acceptable."""
    text = ANSI.sub("", log)
    problems = [marker for marker in PANIC_MARKERS if marker in text]
    missing = [item for item in (*BOOT_MARKERS, *expect) if item not in text]
    if missing:
        problems.append("missing log text: " + ", ".join(missing))
    return problems


def _idf(case: Case, build_dir: Path, *tasks: str) -> list[str]:
    command = [
        "idf.py",
        "-C",
        case.project,
        "-B",
        str(build_dir),
        f"-DSDKCONFIG={_sdkconfig(build_dir)}",
    ]
    if case.profile:
        command.append(f"-DESPKIT_PROFILE={case.profile}")
    command.extend(tasks)
    return command


def _sdkconfig(build_dir: Path) -> Path:
    return build_dir / "sdkconfig"


def _env(build_dir: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["SDKCONFIG"] = str(_sdkconfig(build_dir))
    return env


def _run(command: list[str], cwd: Path, build_dir: Path) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True, env=_env(build_dir))


def build(case: Case, build_dir: Path) -> None:
    _run(_idf(case, build_dir, "set-target", "esp32s3"), ROOT, build_dir)
    _run(_idf(case, build_dir, "build"), ROOT, build_dir)
    if not case.require_network_disabled:
        return
    sdkconfig = _sdkconfig(build_dir).read_text(encoding="utf-8")
    if "CONFIG_ESPKIT_NETWORK_ENABLED=y" in sdkconfig:
        raise SystemExit(f"{case.name}: networking is enabled; QEMU smoke keeps it off")


def boot(case: Case, build_dir: Path) -> str:
    import pexpect

    command = _idf(case, build_dir, "qemu")
    child = pexpect.spawn(
        command[0],
        command[1:],
        cwd=str(ROOT),
        encoding="utf-8",
        codec_errors="replace",
        timeout=1,
        dimensions=(50, 200),
        env=_env(build_dir),
    )
    collected = []
    deadline = time.monotonic() + case.timeout_s
    satisfied_at = None
    try:
        while time.monotonic() < deadline:
            try:
                chunk = child.read_nonblocking(size=8192, timeout=1)
            except pexpect.TIMEOUT:
                chunk = ""
            except pexpect.EOF:
                break
            if chunk:
                collected.append(chunk)
                sys.stdout.write(chunk)
                sys.stdout.flush()
            text = ANSI.sub("", "".join(collected))
            if any(marker in text for marker in PANIC_MARKERS):
                break
            if not assess(text, case.expect):
                if satisfied_at is None:
                    satisfied_at = time.monotonic()
                elif time.monotonic() - satisfied_at >= 2:
                    break
            else:
                satisfied_at = None
        return "".join(collected)
    finally:
        _stop(child)


def _descendants(pid: int) -> list[int]:
    children = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            fields = (entry / "stat").read_text(encoding="utf-8").split()
            parent = int(fields[3])
        except (OSError, IndexError, ValueError):
            continue
        if parent == pid:
            children.append(int(entry.name))
    nested = []
    for child in children:
        nested.extend(_descendants(child))
    return children + nested


def _stop(child) -> None:
    if child.pid is None:
        return
    pids = [child.pid, *_descendants(child.pid)]
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    time.sleep(0.4)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    if child.isalive():
        child.wait()


def run_case(case: Case, build_root: Path) -> None:
    build_dir = build_root / case.name
    started = time.monotonic()
    build(case, build_dir)
    log = boot(case, build_dir)
    problems = assess(log, case.expect)
    elapsed = time.monotonic() - started
    if problems:
        tail = ANSI.sub("", log)[-2000:]
        raise SystemExit(
            f"{case.name} failed after {elapsed:.1f}s: {'; '.join(problems)}\n--- log tail ---\n{tail}"
        )
    print(f"\nPASS {case.name} in {elapsed:.1f}s", flush=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", action="append", default=[], help="Case name. Repeatable.")
    parser.add_argument("--build-root", type=Path, default=ROOT / "build" / "qemu")
    args = parser.parse_args(argv)
    selected = CASES
    if args.only:
        known = {case.name: case for case in CASES}
        missing = [name for name in args.only if name not in known]
        if missing:
            raise SystemExit("unknown case: " + ", ".join(missing))
        selected = tuple(known[name] for name in args.only)
    if shutil_which("qemu-system-xtensa") is None:
        raise SystemExit(
            "qemu-system-xtensa is not installed. From an exported ESP-IDF 5.4.2 shell run:\n"
            '  python "$IDF_PATH/tools/idf_tools.py" install qemu-xtensa\n'
            "  . \"$IDF_PATH/export.sh\""
        )
    started = time.monotonic()
    for case in selected:
        run_case(case, args.build_root)
    print(f"QEMU smoke passed {len(selected)} image(s) in {time.monotonic() - started:.1f}s")
    return 0


def ensure_qemu_on_path() -> None:
    """ESP-IDF 5.4.2 marks qemu-xtensa as an esp32 tool, so export.sh skips it for an esp32s3-only install."""
    from shutil import which

    if which("qemu-system-xtensa"):
        return
    roots = []
    tools = os.environ.get("IDF_TOOLS_PATH")
    if tools:
        roots.append(Path(tools) / "tools" / "qemu-xtensa")
    roots.append(Path.home() / ".espressif" / "tools" / "qemu-xtensa")
    for root in roots:
        bins = sorted(path for path in root.glob("*/qemu/bin") if (path / "qemu-system-xtensa").is_file())
        if not bins:
            continue
        os.environ["PATH"] = str(bins[-1]) + os.pathsep + os.environ.get("PATH", "")
        return


def shutil_which(name: str) -> str | None:
    from shutil import which

    ensure_qemu_on_path()
    return which(name)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from error

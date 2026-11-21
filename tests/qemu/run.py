#!/usr/bin/env python3
"""Build the sensor firmware with the QEMU harness, then run the scenario tests."""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "qemu-harness"
SDKCONFIG = BUILD / "sdkconfig"
EFUSE = BUILD / "qemu_efuse.bin"


def efuse_bytes() -> bytes:
    source = Path(os.environ["IDF_PATH"]) / "tools" / "idf_py_actions" / "qemu_ext.py"
    text = source.read_text(encoding="utf-8")
    start = text.find("'esp32s3': QemuTarget(")
    chunk = text[start : text.find("boot_mode_arg", start)]
    blob = re.search(r"unhexlify\(\s*((?:'[^']*'\s*)+)\)", chunk)
    if blob is None:
        raise SystemExit("could not read the ESP32-S3 QEMU eFuse default from ESP-IDF")
    digits = re.sub(r"[^0-9a-f]", "", blob.group(1))
    if len(digits) < 64:
        raise SystemExit("could not read the ESP32-S3 QEMU eFuse default from ESP-IDF")
    return bytes.fromhex(digits)


def build() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["SDKCONFIG"] = str(SDKCONFIG)
    env["SDKCONFIG_DEFAULTS"] = (
        "sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.defaults.qemu"
    )
    env["ESPKIT_PROFILE"] = "sensor"
    base = [
        "idf.py",
        "-C",
        str(ROOT),
        "-B",
        str(BUILD),
        f"-DSDKCONFIG={SDKCONFIG}",
        "-DESPKIT_PROFILE=sensor",
    ]
    subprocess.run([*base, "set-target", "esp32s3"], cwd=ROOT, env=env, check=True)
    subprocess.run([*base, "build"], cwd=ROOT, env=env, check=True)
    text = SDKCONFIG.read_text(encoding="utf-8")
    if "CONFIG_ESPKIT_QEMU_HARNESS=y" not in text:
        raise SystemExit("QEMU harness was not enabled in the scenario build")
    if "CONFIG_ESPKIT_NETWORK_ENABLED=y" in text:
        raise SystemExit("scenario build enabled networking; leave Wi-Fi off")
    EFUSE.write_bytes(efuse_bytes())


def main() -> int:
    sys.path.insert(0, str(ROOT))
    from tools.qemu_smoke import ensure_qemu_on_path

    ensure_qemu_on_path()
    build()
    extra = (
        f"-drive file={EFUSE},if=none,format=raw,id=efuse "
        "-global driver=nvram.esp32c3.efuse,property=drive,value=efuse "
        "-global driver=timer.esp32s3.timg,property=wdt_disable,value=true "
        "-nic user,model=open_eth"
    )
    logs = BUILD / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        "-m",
        "pytest",
        str(ROOT / "tests" / "qemu"),
        "--embedded-services=idf,qemu",
        "--target=esp32s3",
        f"--app-path={ROOT}",
        f"--build-dir={BUILD}",
        "--qemu-cli-args=-nographic -machine esp32s3",
        f"--qemu-extra-args={extra}",
        f"--root-logdir={logs}",
        f"--junitxml={BUILD / 'junit.xml'}",
        "-q",
        *sys.argv[1:],
    ]
    return subprocess.run(command, cwd=ROOT).returncode


if __name__ == "__main__":
    raise SystemExit(main())

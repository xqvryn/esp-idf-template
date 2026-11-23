#!/usr/bin/env python3
"""Push an ESP-IDF application image to examples/ota-update over HTTP."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


def image_application_version(image: bytes) -> str:
    # ESP-IDF 5.4 app descriptor: image header (24), segment header (8),
    # descriptor prefix (16), then the NUL-terminated version field (32).
    if len(image) < 80 or image[0] != 0xE9 or int.from_bytes(image[32:36], "little") != 0xABCD5432:
        raise ValueError("not an ESP-IDF application image")
    field = image[48:80]
    if b"\0" not in field or field[0] == 0:
        raise ValueError("invalid application image version field")
    return field.split(b"\0", 1)[0].decode("ascii")


def request_json(url: str, timeout: float) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def status(base: str, timeout: float) -> dict:
    return request_json(base.rstrip("/") + "/status", timeout=timeout)


def wait_for_version(base: str, expected: str, timeout: float) -> dict:
    deadline = time.monotonic() + timeout
    last_error = "no response"
    while time.monotonic() < deadline:
        try:
            payload = status(base, timeout=min(5.0, max(1.0, deadline - time.monotonic())))
            if payload.get("version") == expected:
                return payload
            last_error = (
                f"running version={payload.get('version')} partition={payload.get('partition')}"
            )
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError) as exc:
            last_error = str(exc)
        time.sleep(2)
    raise TimeoutError(f"timed out waiting for version {expected}: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True, help="Board base URL, for example http://192.0.2.10")
    parser.add_argument(
        "--firmware",
        required=True,
        type=Path,
        help="Path to the application .bin (not bootloader or flash_args)",
    )
    parser.add_argument("--timeout", type=float, default=120.0, help="Seconds to wait after reboot")
    args = parser.parse_args()

    image = args.firmware.read_bytes()
    expected = image_application_version(image)
    base = args.url.rstrip("/")
    print(f"image version={expected} bytes={len(image)}")

    try:
        before = status(base, timeout=5)
        print(
            "before "
            f"version={before.get('version')} "
            f"partition={before.get('partition')} "
            f"state={before.get('state')}"
        )
        if before.get("version") == expected:
            print("board already runs this version; posting anyway")
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError) as exc:
        print(f"status before update unavailable: {exc}")

    request = urllib.request.Request(
        base + "/update",
        data=image,
        method="POST",
        headers={"Content-Type": "application/octet-stream"},
    )
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            print(response.read().decode("utf-8", errors="replace").strip())
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace").strip()
        print(f"upload failed: HTTP {exc.code} {detail}", file=sys.stderr)
        return 1

    after = wait_for_version(base, expected, timeout=args.timeout)
    print(
        "after "
        f"version={after.get('version')} "
        f"partition={after.get('partition')} "
        f"state={after.get('state')}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

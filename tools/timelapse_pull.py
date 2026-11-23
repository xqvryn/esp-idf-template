#!/usr/bin/env python3
"""Download stills from examples/camera-timelapse and optionally build an MP4."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path


def request_bytes(url: str, timeout: float) -> bytes:
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return response.read()


def request_json(url: str, timeout: float) -> dict:
    return json.loads(request_bytes(url, timeout).decode("utf-8"))


def is_jpeg(data: bytes) -> bool:
    return len(data) >= 4 and data[:2] == b"\xff\xd8" and data[-2:] == b"\xff\xd9"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True, help="Board base URL, for example http://192.0.2.10")
    parser.add_argument("--out", required=True, type=Path, help="Directory for downloaded JPEGs")
    parser.add_argument("--mp4", type=Path, help="Optional output movie (needs ffmpeg)")
    parser.add_argument("--fps", type=float, default=8.0, help="Frame rate for the optional movie")
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--reason", help="Only download this reason (timelapse, motion, or manual)")
    args = parser.parse_args()

    base = args.url.rstrip("/")
    args.out.mkdir(parents=True, exist_ok=True)
    listing = request_json(base + "/list", timeout=args.timeout)
    photos = listing.get("photos") or []
    saved = []
    for photo in photos:
        reason = str(photo.get("reason") or "")
        if args.reason and reason != args.reason:
            continue
        identifier = int(photo["id"])
        data = request_bytes(base + f"/photo/{identifier}", timeout=args.timeout)
        if not is_jpeg(data):
            print(f"skip id={identifier}: not a complete JPEG", file=sys.stderr)
            continue
        path = args.out / f"{identifier:04d}-{reason or 'still'}.jpg"
        path.write_bytes(data)
        saved.append(path)
        print(f"saved {path.name} bytes={len(data)} reason={reason}")

    print(f"downloaded {len(saved)} of {len(photos)} listed stills")
    if not args.mp4:
        return 0
    if not saved:
        print("no stills to encode", file=sys.stderr)
        return 1
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        print("ffmpeg is not on PATH", file=sys.stderr)
        return 1
    list_file = args.out / "frames.txt"
    list_file.write_text("".join(f"file '{path.name}'\n" for path in saved))
    command = [
        ffmpeg,
        "-y",
        "-f",
        "concat",
        "-safe",
        "0",
        "-r",
        str(args.fps),
        "-i",
        str(list_file),
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        str(args.mp4),
    ]
    subprocess.run(command, check=True, cwd=args.out)
    print(f"wrote {args.mp4} frames={len(saved)} fps={args.fps}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except urllib.error.URLError as exc:
        print(f"request failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
    except subprocess.CalledProcessError as exc:
        print(f"ffmpeg failed with exit {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode or 1)

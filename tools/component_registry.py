#!/usr/bin/env python3
"""Pack, dry-run, or publish reusable ESP-IDF components."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = ROOT / "components"
REPO = "https://github.com/xqvryn/esp-idf-template.git"


def component_dirs() -> list[Path]:
    dirs = sorted(
        path.parent
        for path in COMPONENTS.glob("*/idf_component.yml")
    )
    if not dirs:
        raise SystemExit(f"no component manifests under {COMPONENTS}")
    return dirs


def run(command: list[str], cwd: Path) -> None:
    print("+", " ".join(command), f"(in {cwd})", flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def compote_args(path: Path, dest: Path) -> list[str]:
    return [
        "--project-dir",
        str(path),
        "--name",
        path.name,
        "--dest-dir",
        str(dest),
        "--repository",
        REPO,
        "--repository-path",
        f"components/{path.name}",
    ]


def pack_one(path: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    run(["compote", "component", "pack", *compote_args(path, dest)], cwd=ROOT)


def dry_run_one(path: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    run(
        ["compote", "component", "upload", "--dry-run", *compote_args(path, dest)],
        cwd=ROOT,
    )


def upload_one(path: Path, dest: Path, version: str, namespace: str) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    run(
        [
            "compote",
            "component",
            "upload",
            "--namespace",
            namespace,
            "--version",
            version,
            "--allow-existing",
            *compote_args(path, dest),
        ],
        cwd=ROOT,
    )


def pack_all(dest: Path) -> None:
    for path in component_dirs():
        pack_one(path, dest)
    print(f"packed {len(component_dirs())} components into {dest}")


def stable_tag_version(ref: str) -> str | None:
    """Return X.Y.Z for a stable tag like v1.2.3; otherwise None."""
    name = ref
    if name.startswith("refs/tags/"):
        name = name[len("refs/tags/") :]
    if name.startswith("v"):
        name = name[1:]
    parts = name.split(".")
    if len(parts) != 3 or any(not part.isdigit() for part in parts):
        return None
    return name


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=("pack", "dry-run", "publish"),
        help="pack archives, validate with upload --dry-run, or publish on a stable tag",
    )
    parser.add_argument(
        "--dest-dir",
        default=str(ROOT / "local" / "ci" / "component-archives"),
        help="directory for component archives",
    )
    parser.add_argument(
        "--namespace",
        default=os.environ.get("IDF_COMPONENT_NAMESPACE", "xqvryn"),
        help="ESP Component Registry namespace used when publishing",
    )
    args = parser.parse_args()
    dest = Path(args.dest_dir)

    if args.action == "pack":
        pack_all(dest)
        return 0

    if args.action == "dry-run":
        pack_all(dest)
        print("upload --dry-run still talks to the registry; skip it when no token is set")
        if not os.environ.get("IDF_COMPONENT_API_TOKEN"):
            print("IDF_COMPONENT_API_TOKEN is unset; packed archives only")
            return 0
        for path in component_dirs():
            dry_run_one(path, dest)
        return 0

    token = os.environ.get("IDF_COMPONENT_API_TOKEN", "")
    if not token:
        print("IDF_COMPONENT_API_TOKEN is not set; skipping registry publish")
        return 0

    ref = os.environ.get("GITHUB_REF", "")
    version = stable_tag_version(ref)
    if version is None:
        print(f"{ref or 'no tag'} is not a stable vX.Y.Z tag; skipping registry publish")
        return 0

    pack_all(dest)
    for path in component_dirs():
        upload_one(path, dest, version, args.namespace)
    print(f"published {len(component_dirs())} components as {version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

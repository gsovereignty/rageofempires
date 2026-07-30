#!/usr/bin/env python3
"""Verify copied runtime asset families against an extracted installation."""

from __future__ import annotations

import hashlib
import pathlib
import sys


FAMILIES = (
    "Bin",
    "Campaign",
    "Data",
    "Sound",
    "Taunt",
    "Terrain",
    "launcher_res",
)
ROOT_FILES = ("scenariobkg.bmp",)


def inventory(root: pathlib.Path) -> dict[str, tuple[int, str]]:
    result = {}
    for family in FAMILIES:
        directory = root / family
        for path in directory.rglob("*"):
            if not path.is_file() or path.name == ".DS_Store":
                continue
            relative = path.relative_to(root).as_posix()
            data = path.read_bytes()
            result[relative] = (
                len(data),
                hashlib.sha256(data).hexdigest(),
            )
    for name in ROOT_FILES:
        path = root / name
        if path.is_file():
            data = path.read_bytes()
            result[name] = (len(data), hashlib.sha256(data).hexdigest())
    return result


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: verify_copied_game_assets.py "
            "EXTRACTED_APP_ROOT GAME_DATA_ROOT"
        )
        return 2
    source = inventory(pathlib.Path(sys.argv[1]))
    copied = inventory(pathlib.Path(sys.argv[2]))
    failed = False
    for path in sorted(source.keys() - copied.keys()):
        print(f"missing copied asset: {path}")
        failed = True
    for path in sorted(copied.keys() - source.keys()):
        print(f"unexpected copied asset: {path}")
        failed = True
    for path in sorted(source.keys() & copied.keys()):
        if source[path] != copied[path]:
            print(f"changed copied asset: {path}")
            failed = True
    if failed:
        return 1
    print(
        f"copied game asset verification passed: "
        f"{len(source)} files across {len(FAMILIES)} families "
        f"and {len(ROOT_FILES)} root assets"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

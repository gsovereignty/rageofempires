#!/usr/bin/env python3
"""Verify every packaged runtime resource against its checked-in manifest."""

from __future__ import annotations

import hashlib
import json
import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_resource_manifest.py MANIFEST RESOURCE_ROOT")
        return 2

    manifest_path = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = {
        record["path"]: (record["size"], record["sha256"])
        for record in document["files"]
    }
    actual = {
        path.relative_to(root).as_posix()
        for directory in ("resources", "game_data")
        for path in (root / directory).rglob("*")
        if path.is_file() and path.name != ".DS_Store"
    }

    failed = False
    for relative in sorted(expected.keys() - actual):
        print(f"missing packaged resource: {relative}")
        failed = True
    for relative in sorted(actual - expected.keys()):
        print(f"unmanifested packaged resource: {relative}")
        failed = True
    for relative in sorted(expected.keys() & actual):
        data = (root / relative).read_bytes()
        size, digest = expected[relative]
        if len(data) != size:
            print(f"wrong packaged resource size: {relative}")
            failed = True
        elif hashlib.sha256(data).hexdigest() != digest:
            print(f"wrong packaged resource hash: {relative}")
            failed = True

    if failed:
        return 1
    print(f"packaged resource manifest passed: {len(expected)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

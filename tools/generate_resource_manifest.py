#!/usr/bin/env python3
"""Generate deterministic manifest for reconstruction-owned runtime resources."""

from __future__ import annotations

import hashlib
import json
import pathlib
import sys


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    check_only = "--check" in sys.argv[2:]
    records = []
    for directory in ("resources", "game_data"):
        resource_root = root / directory
        for path in sorted(resource_root.rglob("*")):
            if not path.is_file() or path.name == ".DS_Store":
                continue
            data = path.read_bytes()
            records.append(
                {
                    "path": path.relative_to(root).as_posix(),
                    "size": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )
    document = {
        "schema": "aoe-runtime-resource-manifest-v1",
        "root": ".",
        "files": records,
    }
    generated = json.dumps(
        document, indent=2, sort_keys=False
    ) + "\n"
    output = root / "generated/resource_manifest.json"
    if check_only:
        if not output.exists() or output.read_text(
            encoding="utf-8"
        ) != generated:
            print("runtime resource manifest is stale")
            return 1
    else:
        output.write_text(generated, encoding="utf-8")
    print(f"runtime resource manifest passed: {len(records)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

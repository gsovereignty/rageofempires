#!/usr/bin/env python3
"""Compare packaged game data with supplied HD and 1999 research trees."""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inventory(root: Path) -> dict[tuple[int, str], list[str]]:
    result: dict[tuple[int, str], list[str]] = defaultdict(list)
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        result[(path.stat().st_size, sha256(path))].append(path.relative_to(root).as_posix())
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-data", type=Path, required=True)
    parser.add_argument("--hd-root", type=Path, required=True)
    parser.add_argument("--classic-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    hd = inventory(args.hd_root)
    classic = inventory(args.classic_root)
    files = []
    counts: Counter[str] = Counter()
    sizes: Counter[str] = Counter()
    directories: dict[str, Counter[str]] = defaultdict(Counter)

    for path in sorted(item for item in args.game_data.rglob("*") if item.is_file()):
        relative = path.relative_to(args.game_data).as_posix()
        size = path.stat().st_size
        digest = sha256(path)
        key = (size, digest)
        hd_matches = hd.get(key, [])
        classic_matches = classic.get(key, [])
        if hd_matches and classic_matches:
            classification = "hd_and_1999_identical"
        elif hd_matches:
            classification = "hd_only"
        elif classic_matches:
            classification = "1999_only"
        else:
            classification = "no_exact_source_match"
        counts[classification] += 1
        sizes[classification] += size
        directories[relative.split("/", 1)[0]][classification] += 1
        files.append({
            "path": relative,
            "size_bytes": size,
            "sha256": digest,
            "classification": classification,
            "hd_matches": hd_matches,
            "classic_1999_matches": classic_matches,
        })

    document = {
        "manifest_version": 1,
        "method": "SHA-256 and byte-size equality; source paths are research-only",
        "source_roots": {
            "packaged": "game_data",
            "hd": "original-assets-hd/app",
            "classic_1999": "original-assets-1999/Binary",
        },
        "summary": {
            "file_count": len(files),
            "size_bytes": sum(item["size_bytes"] for item in files),
            "counts_by_classification": dict(sorted(counts.items())),
            "bytes_by_classification": dict(sorted(sizes.items())),
            "counts_by_top_level_directory": {
                directory: dict(sorted(values.items()))
                for directory, values in sorted(directories.items())
            },
        },
        "files": files,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

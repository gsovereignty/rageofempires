#!/usr/bin/env python3
"""Inventory user-supplied campaign, scenario, localization, and audio assets."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


EXTENSIONS = {
    "campaign": {".aoe2campaign", ".campaign", ".cpn", ".cpx"},
    "scenario": {".aoe2scenario", ".scenario", ".scn", ".scx"},
    "localization": {".dll", ".ini", ".json", ".po", ".txt", ".xml"},
    "audio": {".mid", ".midi", ".mp3", ".ogg", ".wav", ".wma"},
}
LOCALIZATION_PARTS = {
    "lang",
    "language",
    "languages",
    "locale",
    "locales",
    "localization",
    "resources",
}
AUDIO_ARCHIVES = {"sounds.drs", "sounds_x1.drs", "sounds_x2.drs"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _category(relative: Path) -> str | None:
    suffix = relative.suffix.lower()
    parts = {part.lower() for part in relative.parts[:-1]}
    name = relative.name.lower()
    if suffix in EXTENSIONS["campaign"]:
        return "campaign"
    if suffix in EXTENSIONS["scenario"]:
        return "scenario"
    if name in AUDIO_ARCHIVES:
        return "audio_archive"
    if suffix in EXTENSIONS["audio"]:
        return "audio"
    if suffix in EXTENSIONS["localization"] and (
        parts & LOCALIZATION_PARTS or name.startswith("language")
    ):
        return "localization"
    return None


def inventory(root: Path) -> dict[str, object]:
    root = root.resolve()
    if not root.is_dir():
        raise ValueError(f"asset root is not a directory: {root}")
    categories: dict[str, list[dict[str, object]]] = {
        name: []
        for name in (
            "campaign",
            "scenario",
            "localization",
            "audio",
            "audio_archive",
        )
    }
    for path in sorted(root.rglob("*"), key=lambda item: item.as_posix().lower()):
        if path.is_symlink() or not path.is_file():
            continue
        relative = path.relative_to(root)
        category = _category(relative)
        if category is None:
            continue
        categories[category].append(
            {
                "path": relative.as_posix(),
                "bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
        )
    return {
        "schema": "aoe-content-assets-v1",
        "root": str(root),
        "categories": {
            name: {
                "count": len(files),
                "bytes": sum(int(item["bytes"]) for item in files),
                "files": files,
            }
            for name, files in categories.items()
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_root", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        help="write JSON here instead of standard output",
    )
    arguments = parser.parse_args()
    try:
        report = inventory(arguments.asset_root)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if arguments.output:
        arguments.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

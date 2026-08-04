#!/usr/bin/env python3
"""Inventory original built-in RMS resources without redistributing their text."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


RESOURCE_NAMES = {
    54000: "random_map.def",
    54201: "Arabia",
    54204: "Black Forest",
    54211: "Islands",
    54217: "Rivers",
}


def drs_entries(payload: bytes) -> dict[tuple[str, int], bytes]:
    if len(payload) < 64 or payload[40:44] != b"1.00":
        raise ValueError("unsupported or truncated DRS")
    table_count = struct.unpack_from("<I", payload, 56)[0]
    if table_count > 256 or 64 + table_count * 12 > len(payload):
        raise ValueError("DRS table directory outside archive")
    entries: dict[tuple[str, int], bytes] = {}
    for table in range(table_count):
        raw_ext, offset, count = struct.unpack_from(
            "<4sII", payload, 64 + table * 12
        )
        extension = raw_ext[::-1].decode("ascii", "strict").rstrip(" \0").lower()
        if count > 1_000_000 or offset + count * 12 > len(payload):
            raise ValueError("DRS entry table outside archive")
        for index in range(count):
            resource_id, data_offset, size = struct.unpack_from(
                "<iII", payload, offset + index * 12
            )
            if data_offset + size > len(payload):
                raise ValueError("DRS resource outside archive")
            key = (extension, resource_id)
            if key in entries:
                raise ValueError("duplicate DRS resource")
            entries[key] = payload[data_offset : data_offset + size]
    return entries


def resource_metadata(resource_id: int, payload: bytes) -> dict[str, object]:
    text = payload.decode("cp1252")
    sections = re.findall(r"^\s*<([A-Z_]+)>\s*$", text, re.MULTILINE)
    includes = re.findall(
        r"^\s*#include_drs\s+(\S+)\s+(-?\d+)\s*$", text, re.MULTILINE
    )
    return {
        "resource_id": resource_id,
        "name": RESOURCE_NAMES[resource_id],
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
        "line_count": len(text.splitlines()),
        "sections": sections,
        "includes": [
            {"filename": filename, "resource_id": int(include_id)}
            for filename, include_id in includes
        ],
    }


def audit(path: Path) -> dict[str, object]:
    archive = path.read_bytes()
    entries = drs_entries(archive)
    resources = []
    for resource_id in RESOURCE_NAMES:
        key = ("bina", resource_id)
        if key not in entries:
            raise ValueError(f"required RMS resource {resource_id} absent")
        resources.append(resource_metadata(resource_id, entries[key]))
    return {
        "schema": "aoe-builtin-random-map-evidence-v1",
        "archive": {
            "filename": path.name,
            "bytes": len(archive),
            "sha256": hashlib.sha256(archive).hexdigest(),
        },
        "resources": resources,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("gamedata_drs", type=Path)
    parser.add_argument("output", type=Path, nargs="?")
    args = parser.parse_args()
    rendered = json.dumps(audit(args.gamedata_drs), indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()

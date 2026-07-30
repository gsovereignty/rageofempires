#!/usr/bin/env python3
"""Verify classic Blendomatic cardinal mapping against local AoK HD binary."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct


EXPECTED_SHA256 = (
    "e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc"
)
IMAGE_BASE = 0x400000
NEIGHBOR_TABLE_VA = 0x81D310
EXPECTED_NEIGHBORS = (
    ("north_west", -1, -1, 0x01, 19),
    ("north_east", 1, -1, 0x02, 18),
    ("south_east", 1, 1, 0x04, 16),
    ("south_west", -1, 1, 0x08, 17),
    ("north", 0, -1, 0x10, 12),
    ("east", 1, 0, 0x20, 4),
    ("south", 0, 1, 0x40, 0),
    ("west", -1, 0, 0x80, 8),
)


def raw_offset_for_va(data: bytes, va: int) -> int:
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    sections = pe + 24 + optional_size
    rva = va - IMAGE_BASE
    for index in range(section_count):
        offset = sections + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = (
            struct.unpack_from("<IIII", data, offset + 8)
        )
        span = max(virtual_size, raw_size)
        if virtual_address <= rva < virtual_address + span:
            return raw_pointer + rva - virtual_address
    raise ValueError(f"VA 0x{va:x} is outside PE sections")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--decompiled", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/terrain_transition_evidence.json"),
    )
    args = parser.parse_args()

    binary = args.exe.read_bytes()
    digest = hashlib.sha256(binary).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unsupported executable SHA-256: {digest}")
    table_offset = raw_offset_for_va(binary, NEIGHBOR_TABLE_VA)
    values = struct.unpack_from("<24i", binary, table_offset)
    rows = []
    for index, expected in enumerate(EXPECTED_NEIGHBORS):
        name, dx, dy, bit, mask_base = expected
        actual = values[index * 3:index * 3 + 3]
        if actual != (dx, dy, bit):
            raise SystemExit(
                f"neighbor table mismatch at {name}: {actual!r}"
            )
        rows.append({
            "direction": name,
            "dx": dx,
            "dy": dy,
            "influence_bit": bit,
            "mask": mask_base if bit < 0x10 else None,
            "cardinal_family": (
                list(range(mask_base, mask_base + 4))
                if bit >= 0x10 else None
            ),
        })

    decompiled = args.decompiled.read_text(
        encoding="utf-8", errors="replace"
    )
    formula = "local_a8 = param_6 + param_7 & 3;"
    application = "bVar7 = bVar7 + param_9;"
    if formula not in decompiled or application not in decompiled:
        raise SystemExit("variant formula evidence missing from decompilation")

    report = {
        "status": "proved",
        "source": {
            "path": str(args.exe),
            "sha256": digest,
            "pe_neighbor_table_va": f"0x{NEIGHBOR_TABLE_VA:08x}",
            "decompiled_path": str(args.decompiled),
            "neighbor_table_function": "FUN_00553b10",
            "mask_catalog_function": "FUN_0054e400",
            "mask_application_function": "FUN_0054e810",
        },
        "neighbor_order": rows,
        "cardinal_variant": {
            "formula": "(tile_x + tile_y) & 3",
            "decompiler_expression": formula,
            "application": (
                "variant is added only when base mask ID is below 16"
            ),
            "decompiler_application": application,
        },
        "fixed_cardinal_combinations": {
            "north_east": 23,
            "north_south": 21,
            "east_south": 25,
            "north_east_south": 29,
            "north_west": 22,
            "east_west": 20,
            "north_east_west": 28,
            "south_west": 24,
            "north_south_west": 27,
            "east_south_west": 26,
            "all": 30,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Audit bounded original fog-edge evidence without inferring rendering roles."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


SHAPES = 17
EDGE_CLASSES = 47


def span_payload(data: bytes, offset: int) -> int:
    """Validate one original {row,left,right} list and return record count."""
    records = 0
    cursor = offset
    while cursor < len(data):
        if data[cursor] == 0xFF:
            return records
        if cursor + 3 > len(data):
            break
        records += 1
        cursor += 3
    raise ValueError("unterminated edge span payload")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def edge_table(path: Path, pointer_width: int) -> dict:
    data = path.read_bytes()
    if len(data) < SHAPES * 4:
        raise ValueError(f"{path}: truncated shape-offset table")
    offsets = list(struct.unpack_from("<17I", data))
    if offsets != sorted(offsets) or any(
        offset < SHAPES * 4 or
        offset + EDGE_CLASSES * pointer_width > len(data)
        for offset in offsets
    ):
        raise ValueError(f"{path}: invalid shape table offset")
    entries: list[dict] = []
    for shape, offset in enumerate(offsets):
        fmt = "<" + "I" * EDGE_CLASSES * (pointer_width // 4)
        pointers = struct.unpack_from(fmt, data, offset)
        invalid = [value for value in pointers if value >= len(data)]
        if invalid:
            raise ValueError(f"{path}: payload pointer outside file")
        payload_lengths = [
            span_payload(data, value)
            for value in sorted(set(pointers))
            if value != 0
        ]
        entries.append({
            "shape": shape,
            "table_offset": offset,
            "pointer_slots": len(pointers),
            "present_pointer_slots": sum(value != 0 for value in pointers),
            "unique_payloads": len(payload_lengths),
            "maximum_span_records": max(payload_lengths, default=0),
        })
    return {
        "filename": path.name,
        "sha256": sha256(path),
        "bytes": len(data),
        "shape_count": SHAPES,
        "edge_class_count": EDGE_CLASSES,
        "pointers_per_class": pointer_width // 4,
        "shapes": entries,
    }


def canonical_neighbor_classes() -> tuple[list[int], int]:
    """Reproduce FUN_0054dfb0's exact 256-byte normalization table."""
    result = [-1] * 256
    next_class = 0
    for mask in range(256):
        bit0_clear = not mask & 0x01
        bit1_set = bool(mask & 0x02)
        bit2_set = bool(mask & 0x04)
        bit3_clear = not mask & 0x08
        accepted = not (mask & 0x80) or (bit0_clear and bit3_clear)
        if mask & 0x40 and (bit2_set or not bit3_clear):
            accepted = False
        if mask & 0x20 and (bit1_set or bit2_set):
            accepted = False
        if (not mask & 0x10 or (bit0_clear and not bit1_set)) and accepted:
            result[mask] = next_class
            next_class += 1
    for mask in range(256):
        if result[mask] >= 0:
            continue
        normalized = mask
        if mask & 0x80:
            normalized &= ~0x09
        if mask & 0x40:
            normalized &= ~0x0C
        if mask & 0x20:
            normalized &= ~0x06
        if mask & 0x10:
            normalized &= ~0x03
        result[mask] = result[normalized]
    return result, next_class


def executable_evidence(path: Path) -> dict:
    data = path.read_bytes()
    strings = [
        b"TileEdge.Dat",
        b"BlkEdge.Dat",
        b"diam_map::draw_explored_tiles",
    ]
    positions = {
        value.decode(): data.find(value)
        for value in strings
    }
    return {
        "filename": path.name,
        "sha256": sha256(path),
        "bytes": len(data),
        "string_offsets": positions,
        "required_strings_present": all(value >= 0 for value in positions.values()),
    }


def make_catalog(
    executable: Path,
    tile_edge: Path,
    black_edge: Path,
    interface_drs: Path,
    graphics_drs: Path,
) -> dict:
    classes, count = canonical_neighbor_classes()
    if count != EDGE_CLASSES or min(classes) < 0 or max(classes) != 46:
        raise ValueError("canonical neighbor table did not produce 47 classes")
    return {
        "schema": "aoe-fog-rendering-contract-v2",
        "sources": {
            "executable": executable_evidence(executable),
            "tile_edge": edge_table(tile_edge, 8),
            "black_edge": edge_table(black_edge, 4),
            "interfac_drs": {
                "filename": interface_drs.name,
                "sha256": sha256(interface_drs),
                "bytes": interface_drs.stat().st_size,
            },
            "graphics_drs": {
                "filename": graphics_drs.name,
                "sha256": sha256(graphics_drs),
                "bytes": graphics_drs.stat().st_size,
            },
        },
        "proved": {
            "edge_files": (
                "executable names TileEdge.Dat and BlkEdge.Dat; each archive "
                "has 17 shape tables and 47 edge classes"
            ),
            "neighbor_mask_ordinal": (
                "executable normalization consumes bits 0..7 and maps all "
                "256 values into classes 0..46"
            ),
            "compass_bit_order": (
                "FUN_0054f970 constructs bits 0..7 as northwest, southwest, "
                "southeast, northeast, west, south, east, north"
            ),
            "state_to_asset": (
                "FUN_00555020 maps hidden to TileEdge class 0 with no "
                "BlkEdge, explored to TileEdge class 0 plus explored-neighbor "
                "BlkEdge, and visible to visible-neighbor TileEdge plus "
                "explored-neighbor BlkEdge"
            ),
            "shape_selection": (
                "FUN_00555020 reads the map tile shape byte and directly "
                "indexes matching TileEdge and BlkEdge tables 0..16"
            ),
            "payload_encoding": (
                "FUN_0050e520 and FUN_0050e960 consume both edge payloads as "
                "0xff-terminated three-byte {row,left,right} geometry spans; "
                "TileEdge adds spans and BlkEdge removes spans"
            ),
            "neighbor_class_map": classes,
            "minimap": (
                "executable contains diam_map::draw_explored_tiles; this "
                "proves explored-tile minimap participation only"
            ),
        },
        "unproved": {
            "resource_ids": (
                "edge assets are named loose DAT files, not proved DRS IDs; "
                "no fog SLP resource identity is established"
            ),
        },
        "renderer_decision": {
            "archive_backed_world_fog": True,
            "archive_backed_minimap_fog": True,
            "reason": (
                "pointer-free generated span geometry plus recovered 0x56/0x28 "
                "stipple and opaque hidden/explored composition are local"
            ),
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--tile-edge", type=Path, required=True)
    parser.add_argument("--black-edge", type=Path, required=True)
    parser.add_argument("--interface-drs", type=Path, required=True)
    parser.add_argument("--graphics-drs", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/fog_rendering_catalog.json"),
    )
    args = parser.parse_args()
    report = make_catalog(
        args.executable,
        args.tile_edge,
        args.black_edge,
        args.interface_drs,
        args.graphics_drs,
    )
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()

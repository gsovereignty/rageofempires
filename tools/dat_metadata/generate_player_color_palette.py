#!/usr/bin/env python3
"""Generate exact player-color evidence from local AoC/HD assets."""

import argparse
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def drs_entries(path):
    data = path.read_bytes()
    table_count = struct.unpack_from("<i", data, 56)[0]
    entries = {}
    for table in range(table_count):
        offset = 64 + table * 12
        extension = data[offset : offset + 4][::-1].decode("ascii").rstrip(" ").lower()
        info_offset, count = struct.unpack_from("<ii", data, offset + 4)
        for item in range(count):
            entry_offset = info_offset + item * 12
            resource_id, payload_offset, size = struct.unpack_from("<iii", data, entry_offset)
            entries[(extension, resource_id)] = data[payload_offset : payload_offset + size]
    return entries


def parse_palette(payload):
    lines = payload.decode("ascii").splitlines()
    if lines[:2] != ["JASC-PAL", "0100"]:
        raise ValueError("resource 50500 is not a JASC 0100 palette")
    count = int(lines[2])
    colors = [list(map(int, line.split())) for line in lines[3 : 3 + count]]
    if len(colors) != count:
        raise ValueError("palette is truncated")
    return colors


def scan_slp(payload, list_sources, fill_sources):
    if not payload.startswith(b"2.0"):
        return False
    frame_count = struct.unpack_from("<i", payload, 4)[0]
    for frame in range(frame_count):
        frame_offset = 32 + frame * 32
        commands, outline = struct.unpack_from("<II", payload, frame_offset)
        width, height = struct.unpack_from("<ii", payload, frame_offset + 16)
        if width <= 0 or height <= 0:
            raise ValueError("invalid SLP dimensions")
        for row in range(height):
            left, right = struct.unpack_from("<HH", payload, outline + row * 4)
            if left == 0x8000 or right == 0x8000:
                continue
            position = struct.unpack_from("<I", payload, commands + row * 4)[0]
            column = left
            row_end = width - right
            while True:
                command = payload[position]
                position += 1
                low, high, crumb = command & 0x0F, command & 0xF0, command & 0x03

                def count(shift):
                    nonlocal position
                    amount = command >> shift
                    if amount == 0:
                        amount = payload[position]
                        position += 1
                    return amount

                if low == 0x0F:
                    break
                if crumb == 0:
                    amount = command >> 2
                    position += amount
                    column += amount
                elif crumb == 1:
                    column += count(2)
                elif low in (0x02, 0x03):
                    amount = (high << 4) + payload[position]
                    position += 1
                    if low == 0x02:
                        position += amount
                    column += amount
                elif low == 0x06:
                    amount = count(4)
                    list_sources.update(payload[position : position + amount])
                    position += amount
                    column += amount
                elif low in (0x07, 0x0A):
                    amount = count(4)
                    source = payload[position]
                    position += 1
                    if low == 0x0A:
                        fill_sources[source] += amount
                    column += amount
                elif low == 0x0B:
                    column += count(4)
                elif low == 0x0E:
                    if high in (0x40, 0x60):
                        column += 1
                    elif high in (0x50, 0x70):
                        column += payload[position]
                        position += 1
                    elif high > 0x30:
                        raise ValueError("unknown extended SLP command")
                else:
                    raise ValueError("unknown SLP command")
            if column != row_end:
                raise ValueError("SLP row pixel count mismatch")
    return True


def extract(metadata, interface_drs, graphics_drs, executable):
    interface = drs_entries(interface_drs)
    palette_payload = interface[("bina", 50500)]
    colors = parse_palette(palette_payload)
    color_records = metadata["player_colors"]
    playable = color_records[:8]
    ramps = []
    for record in playable:
        base = record["base_palette_index"]
        ramp = colors[base : base + 16]
        ramps.append(
            {
                "color_id": record["id"],
                "base_palette_index": base,
                "palette_indices": list(range(base, base + 16)),
                "rgb": ramp,
                "rgb_sha256": hashlib.sha256(
                    bytes(channel for color in ramp for channel in color)
                ).hexdigest(),
                "unit_outline_palette_index": record["unit_outline_palette_index"],
                "unit_outline_rgb": colors[record["unit_outline_palette_index"]],
                "minimap_palette_indices": record["minimap_palette_indices"],
                "minimap_rgb": [colors[index] for index in record["minimap_palette_indices"]],
                "statistics_text_color": record["statistics_text_color"],
            }
        )

    list_sources, fill_sources = Counter(), Counter()
    classic = failed = 0
    for (extension, _resource_id), payload in drs_entries(graphics_drs).items():
        if extension != "slp":
            continue
        try:
            classic += int(scan_slp(payload, list_sources, fill_sources))
        except (IndexError, struct.error, ValueError):
            failed += 1

    return {
        "schema": 1,
        "profile": "2013 HD installer carrying VER 5.7 AoC-format DAT/DRS assets",
        "inputs": {
            "interface_drs_sha256": sha256(interface_drs),
            "graphics_drs_sha256": sha256(graphics_drs),
            "dat_sha256": metadata.get("source_sha256"),
            "executable_sha256": sha256(executable),
        },
        "palette": {
            "resource": {"archive": "interfac.drs", "extension": "bina", "id": 50500},
            "payload_sha256": hashlib.sha256(palette_payload).hexdigest(),
            "format": "JASC-PAL 0100",
            "color_count": len(colors),
            "full_rgb_sha256": hashlib.sha256(
                bytes(channel for color in colors for channel in color)
            ).hexdigest(),
        },
        "playable_color_records": playable,
        "ramps": ramps,
        "slp_player_pixels": {
            "formula": "palette_index = DAT player-color base_palette_index + SLP player source index",
            "player_color_list_command": "low nibble 0x06",
            "player_color_fill_command": "low nibble 0x0a",
            "classic_slp_resources_scanned": classic,
            "unsupported_or_invalid_resources": failed,
            "list_source_histogram": {
                str(index): count for index, count in sorted(list_sources.items())
            },
            "fill_source_histogram": {
                str(index): count for index, count in sorted(fill_sources.items())
            },
            "observed_source_indices": sorted(set(list_sources) | set(fill_sources)),
        },
        "selection": {
            "civilization_independent": True,
            "team_independent": True,
            "key": "player color-table ID 0..7 selected by match/scenario player slot",
            "not_slot_arithmetic": (
                "IDs 4..7 map to bases 96,112,128,80; base is not (id+1)*16"
            ),
        },
        "classification": {
            "asset_palette_and_dat_mapping": "exact",
            "slp_source_index_inventory": "exact archive evidence",
            "slp_command_formula": "exact pinned format-decoder contract",
            "classic_aoc_paletted_remap": "implementation-ready",
            "hd_runtime_equivalence_to_classic_remap": "unproved",
            "hd_brightness_or_shader_transform": "unproved",
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=Path)
    parser.add_argument("interface_drs", type=Path)
    parser.add_argument("graphics_drs", type=Path)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    metadata = json.loads(args.metadata.read_text())
    metadata["source_sha256"] = sha256(Path(metadata["source"]))
    result = extract(metadata, args.interface_drs, args.graphics_drs, args.executable)
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()

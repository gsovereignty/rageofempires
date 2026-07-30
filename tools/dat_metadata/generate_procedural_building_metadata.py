#!/usr/bin/env python3
"""Generate DAT/DRS evidence for buildings still rendered procedurally."""

import argparse
import json
import struct
from pathlib import Path


BUILDINGS = {
    "farm": 50,
    "monastery": 104,
    "palisade_wall": 72,
    "stone_wall": 117,
    "palisade_gate_x": 792,
    "palisade_gate_y": 796,
    "stone_gate_x": 789,
    "stone_gate_y": 793,
}


def drs_resources(path):
    data = path.read_bytes()
    table_count = struct.unpack_from("<I", data, 56)[0]
    result = {}
    for index in range(table_count):
        table = 64 + index * 12
        extension = data[table:table + 4]
        offset, count = struct.unpack_from("<II", data, table + 4)
        if extension != b" pls":
            continue
        for entry in range(count):
            record = offset + entry * 12
            resource_id, payload, size = struct.unpack_from("<III", data, record)
            result[resource_id] = data[payload:payload + size]
    return result


def slp_header(data):
    if data[:3] != b"2.0":
        raise ValueError("unsupported SLP")
    count = struct.unpack_from("<i", data, 4)[0]
    frames = []
    for index in range(count):
        offset = 32 + index * 32
        width, height, hotspot_x, hotspot_y = struct.unpack_from(
            "<iiii", data, offset + 16
        )
        frames.append({
            "width": width,
            "height": height,
            "hotspot_x": hotspot_x,
            "hotspot_y": hotspot_y,
        })
    return {"frame_count": count, "frames": frames}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("graphics_drs")
    parser.add_argument(
        "--output", default="generated/procedural_building_dat_metadata.json"
    )
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    graphics = {graphic["id"]: graphic for graphic in data["graphics"]}
    archive = drs_resources(Path(args.graphics_drs))

    mappings = {}
    root_graphics = set()
    for name, unit_id in BUILDINGS.items():
        families = {}
        for civilization in data["civilizations"][1:]:
            unit = next(unit for unit in civilization["units"] if unit["id"] == unit_id)
            record = {
                "standing_graphic": unit["standing_graphic"],
                "construction_graphic": unit["construction_graphic"],
                "dying_graphic": unit["dying_graphic"],
                "damage_sprites": unit["damage_sprites"],
            }
            key = json.dumps(record, sort_keys=True)
            families.setdefault(key, {"record": record, "civilizations": []})
            families[key]["civilizations"].append(civilization["name"])
            root_graphics.update(
                value for value in (
                    unit["standing_graphic"], unit["construction_graphic"],
                    unit["dying_graphic"],
                ) if value is not None
            )
            root_graphics.update(
                item["graphic_id"] for item in unit["damage_sprites"]
            )
        mappings[name] = list(families.values())

    closure = set()
    pending = list(root_graphics)
    while pending:
        graphic_id = pending.pop()
        if graphic_id in closure:
            continue
        closure.add(graphic_id)
        pending.extend(
            delta["graphic_id"] for delta in graphics[graphic_id]["deltas"]
            if delta["graphic_id"] is not None
        )

    graphic_records = {}
    for graphic_id in sorted(closure):
        record = dict(graphics[graphic_id])
        slp_id = record["slp_id"]
        record["slp_present"] = slp_id is not None and slp_id in archive
        record["slp_header"] = (
            slp_header(archive[slp_id]) if record["slp_present"] else None
        )
        graphic_records[str(graphic_id)] = record

    output = {
        "schema": "aoe-procedural-building-assets-v1",
        "source_format": data["format"],
        "mappings": mappings,
        "graphics": graphic_records,
        "gate_orientation": {
            "x": {
                "palisade_unit": 792,
                "stone_unit": 789,
                "root_names": ["SGAX1NN", "SGAA1NN"],
                "axis": "A",
            },
            "y": {
                "palisade_unit": 796,
                "stone_unit": 793,
                "root_names": ["SGBX1NN", "SGBA1NN"],
                "axis": "B",
            },
        },
        "validation_boundary": {
            "dat_proves": [
                "civilization-specific standing/construction/dying mappings",
                "damage graphic IDs, raw thresholds and flags",
                "graphic delta composition, offsets, angles and DAT frame counts",
            ],
            "drs_proves": [
                "SLP presence, frame count, dimensions and hotspots",
            ],
            "runtime_required": [
                "damage threshold comparison edge and layer replacement policy",
                "construction frame selection and death animation timing",
                "gate open/closed state transitions and draw-order policy",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

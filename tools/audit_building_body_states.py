#!/usr/bin/env python3
"""Audit all represented building body states against exact DAT/DRS chains."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


BUILDINGS = {
    "town_center": 109, "barracks": 12, "archery_range": 87,
    "house": 70, "mill": 68, "lumber_camp": 562,
    "mining_camp": 584, "farm": 50, "stable": 101,
    "blacksmith": 103, "castle": 82, "university": 209,
    "siege_workshop": 49, "palisade_wall": 72, "watch_tower": 79,
    "stone_wall": 117, "palisade_gate_x": 792,
    "palisade_gate_y": 796, "stone_gate_x": 789,
    "stone_gate_y": 793, "monastery": 104, "market": 84,
    "dock": 45, "bombard_tower": 236, "fish_trap": 199,
    "outpost": 598, "wonder": 276,
}

CONSTRUCTION_RENDERED = {
    "fish_trap",
    "stone_wall",
    "palisade_gate_x",
    "palisade_gate_y",
}
DAMAGE_RENDERED = set(BUILDINGS) - {"fish_trap"}
DEATH_RENDERED = set(BUILDINGS) - {"farm", "fish_trap"}


def drs_ids(path: Path) -> set[int]:
    data = path.read_bytes()
    result = set()
    tables = struct.unpack_from("<I", data, 56)[0]
    for index in range(tables):
        extension, offset, count = struct.unpack_from(
            "<4sII", data, 64 + index * 12
        )
        if extension != b" pls":
            continue
        if offset + count * 12 > len(data):
            raise ValueError("DRS table outside archive")
        for entry in range(count):
            resource, payload, size = struct.unpack_from(
                "<III", data, offset + entry * 12
            )
            if payload + size > len(data):
                raise ValueError("DRS payload outside archive")
            result.add(resource)
    return result


def chain(root: int | None, graphics: dict[int, dict], slps: set[int]) -> dict:
    if root is None or root < 0:
        return {"root": root, "complete": False, "reason": "absent_root"}
    pending = [(root, 0, 0)]
    seen = set()
    layers = []
    while pending:
        graphic_id, offset_x, offset_y = pending.pop()
        if graphic_id in seen:
            continue
        seen.add(graphic_id)
        graphic = graphics.get(graphic_id)
        if graphic is None:
            return {
                "root": root, "complete": False,
                "reason": f"missing_graphic_{graphic_id}",
            }
        slp = graphic["slp_id"]
        present = slp is not None and slp >= 0 and slp in slps
        if slp is not None and slp >= 0 and not present:
            return {
                "root": root, "complete": False,
                "reason": f"missing_slp_{slp}",
            }
        if present:
            layers.append({
                "graphic_id": graphic_id,
                "slp_id": slp,
                "layer": graphic["layer"],
                "offset_x": offset_x,
                "offset_y": offset_y,
                "frame_count": graphic["frame_count"],
                "angle_count": graphic["angle_count"],
                "palette_id": graphic["palette"],
                "frame_rate": graphic["frame_rate"],
                "replay_delay": graphic["replay_delay"],
            })
        for delta in graphic["deltas"]:
            child = delta["graphic_id"]
            if child is not None and child >= 0:
                pending.append((
                    child,
                    offset_x + delta["offset_x"],
                    offset_y + delta["offset_y"],
                ))
    if not layers:
        return {"root": root, "complete": False, "reason": "no_drawable_layer"}
    layers.sort(key=lambda item: item["layer"])
    frames = {item["frame_count"] for item in layers}
    return {
        "root": root,
        "complete": True,
        "reason": "complete_DAT_DRS_chain",
        "layer_order": layers,
        "cadence_compatible": len(frames) == 1 or frames <= {1, max(frames)},
    }


def make_catalog(metadata: dict, archive_ids: set[int]) -> dict:
    graphics = {item["id"]: item for item in metadata["graphics"]}
    entities = []
    for kind, unit_id in BUILDINGS.items():
        families = {}
        for civilization in metadata["civilizations"][1:]:
            unit = next(
                item for item in civilization["units"]
                if item["id"] == unit_id
            )
            record = {
                "standing": unit["standing_graphic"],
                "construction": unit["construction_graphic"],
                "death": unit["dying_graphic"],
                "damage": [
                    {
                        **item,
                        "chain": chain(
                            item["graphic_id"], graphics, archive_ids
                        ),
                    }
                    for item in unit["damage_sprites"]
                ],
            }
            key = json.dumps(record, sort_keys=True)
            family = families.setdefault(key, {
                "civilizations": [],
                "standing": chain(
                    record["standing"], graphics, archive_ids
                ),
                "construction": chain(
                    record["construction"], graphics, archive_ids
                ),
                "death": chain(record["death"], graphics, archive_ids),
                "damage": record["damage"],
            })
            family["civilizations"].append(civilization["name"])
        entities.append({
            "kind": kind,
            "unit_id": unit_id,
            "families": list(families.values()),
            "renderer": {
                "standing": "existing_archive_or_guarded_path",
                "construction": (
                    "exact_animated_DAT_composite"
                    if kind in CONSTRUCTION_RENDERED
                    else "classified_procedural_fallback"
                ),
                "damage": (
                    "exact_animated_DAT_overlay_or_replacement"
                    if kind in DAMAGE_RENDERED
                    else "classified_no_exact_overlay"
                ),
                "death": (
                    "exact_animated_DAT_composite"
                    if kind in DEATH_RENDERED
                    else "classified_procedural_fallback"
                ),
            },
        })
    return {
        "schema": "aoe-building-body-state-catalog-v1",
        "represented_buildings": len(entities),
        "coverage": {
            "construction": {
                "before": 0, "after": len(CONSTRUCTION_RENDERED), "total": 27
            },
            "damage": {
                "before": 0,
                "after": len(DAMAGE_RENDERED),
                "procedural_or_unrendered": len(
                    set(BUILDINGS) - DAMAGE_RENDERED
                ),
                "total": 27,
            },
            "death": {
                "before": 3, "after": len(DEATH_RENDERED), "total": 27
            },
        },
        "policy": {
            "promotion": (
                "exact DAT state root, recursively complete present DRS SLP "
                "chain containing a body layer >=20, stable layer order, "
                "accumulated offsets, per-player palette decode, compatible "
                "frame cadence"
            ),
            "construction": (
                "all audited construction roots resolve only layer-10 art; "
                "none is promoted as a body composition"
            ),
            "damage": (
                "runtime selector uses 100-floor(hp*100/maxhp), strict "
                "threshold comparison, flag 0/1 attachment overlays, and "
                "flag 2 standing-graphic replacement; flag 1 randomized "
                "offset records are excluded unless deterministic placement "
                "is independently proved"
            ),
        },
        "entities": entities,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=Path)
    parser.add_argument("graphics_drs", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/building_body_state_catalog.json"),
    )
    args = parser.parse_args()
    report = make_catalog(
        json.loads(args.metadata.read_text()),
        drs_ids(args.graphics_drs),
    )
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()

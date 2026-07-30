#!/usr/bin/env python3
"""Extract bounded garrison/combat evidence from pinned VER 5.7 JSON."""

import argparse
import json
from pathlib import Path


BUILDINGS = {
    "watch_tower": 79,
    "castle": 82,
    "town_center": 109,
    "guard_tower": 234,
    "keep": 235,
    "bombard_tower": 236,
}


def generate(data):
    units = {}
    wanted = set(BUILDINGS.values())
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            if unit["id"] in wanted and unit["id"] not in units:
                units[unit["id"]] = unit
    missing = sorted(wanted - units.keys())
    if missing:
        raise ValueError(f"missing garrison building records: {missing}")

    buildings = {}
    for name, unit_id in BUILDINGS.items():
        unit = units[unit_id]
        building = unit.get("building")
        combat = unit.get("combat")
        if "garrison_capacity" not in unit or building is None:
            raise ValueError(
                f"metadata extractor lacks garrison fields for unit {unit_id}"
            )
        buildings[name] = {
            "id": unit_id,
            "unit_class": unit["unit_class"],
            "garrison_capacity": unit["garrison_capacity"],
            "garrison_type_raw": building["garrison_type"],
            "garrison_heal_rate_raw": building["garrison_heal_rate"],
            "garrison_repair_rate_raw": building["garrison_repair_rate"],
            "combat": None if combat is None else {
                key: combat[key]
                for key in (
                    "weapons",
                    "range_min",
                    "range_max",
                    "area_effect_range",
                    "blast_level_offense",
                    "reload_time",
                    "missile_unit",
                    "accuracy",
                )
            },
        }
    return {
        "source_format": data["format"],
        "scope": "represented garrison buildings and raw combat fields",
        "buildings": buildings,
        "validation_boundary": {
            "dat_proves": [
                "raw garrison capacity",
                "raw garrison type, heal-rate and repair-rate fields",
                "weapon classes, range, reload, accuracy, projectile and blast fields",
            ],
            "original_runtime_required": [
                "garrison type-mask interpretation and accepted unit classes",
                "capacity modifiers and occupancy cost",
                "garrisoned projectile contribution and target selection",
                "ejection, healing, repair, conversion and destruction behavior",
                "area-effect and blast-level runtime filtering",
            ],
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument(
        "--output", default="generated/garrison_dat_metadata.json"
    )
    arguments = parser.parse_args()
    data = json.loads(Path(arguments.metadata).read_text())
    output = generate(data)
    destination = Path(arguments.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

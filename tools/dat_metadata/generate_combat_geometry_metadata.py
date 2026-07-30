#!/usr/bin/env python3
"""Extract bounded raw combat-geometry evidence from pinned VER 5.7 JSON."""

import argparse
import json
from pathlib import Path


WEAPONS = {
    "bombard_cannon": 36,
    "trebuchet_unpacked": 42,
    "bombard_tower": 236,
    "scorpion": 279,
    "mangonel": 280,
    "trebuchet_packed": 331,
    "cannon_galleon": 420,
    "petard": 440,
    "demolition_ship": 527,
    "heavy_demolition_ship": 528,
    "heavy_scorpion": 542,
    "onager": 550,
    "siege_onager": 588,
    "elite_cannon_galleon": 691,
}

COMBAT_FIELDS = (
    "weapons",
    "range_min",
    "range_max",
    "area_effect_range",
    "blast_level_offense",
    "missed_missile_spread",
    "missile_unit",
    "accuracy",
    "weapon_offset",
)

CREATION_FIELDS = (
    "volley_fire_amount",
    "max_attacks_in_volley",
    "volley_spread",
    "volley_start_spread_adjustment",
    "volley_missile_unit",
    "special_attack_flag",
)


def _first_units(data):
    units = {}
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            units.setdefault(unit["id"], unit)
    return units


def _select(record, fields):
    return None if record is None else {field: record[field] for field in fields}


def generate(data):
    units = _first_units(data)
    missing = sorted(set(WEAPONS.values()) - units.keys())
    if missing:
        raise ValueError(f"missing combat geometry records: {missing}")

    weapons = {}
    projectile_ids = set()
    for name, unit_id in WEAPONS.items():
        unit = units[unit_id]
        combat = unit.get("combat")
        creation = unit.get("creation")
        if "area_effect_level" not in unit or combat is None:
            raise ValueError(
                f"metadata extractor lacks combat geometry fields for unit {unit_id}"
            )
        combat_output = _select(combat, COMBAT_FIELDS)
        creation_output = _select(creation, CREATION_FIELDS)
        projectile_ids.update(
            projectile_id
            for projectile_id in (
                combat_output["missile_unit"],
                None if creation_output is None
                else creation_output["volley_missile_unit"],
            )
            if projectile_id is not None
        )
        weapons[name] = {
            "id": unit_id,
            "unit_class": unit["unit_class"],
            "area_effect_level_raw": unit["area_effect_level"],
            "combat": combat_output,
            "attack_dispersion": creation_output,
        }

    missing_projectiles = sorted(projectile_ids - units.keys())
    if missing_projectiles:
        raise ValueError(f"missing projectile records: {missing_projectiles}")
    projectiles = {
        str(projectile_id): {
            "id": projectile_id,
            "unit_class": units[projectile_id]["unit_class"],
            "area_effect_level_raw": units[projectile_id]["area_effect_level"],
            "combat": _select(units[projectile_id].get("combat"), COMBAT_FIELDS),
            "missile_raw": units[projectile_id].get("missile"),
        }
        for projectile_id in sorted(projectile_ids)
    }

    return {
        "source_format": data["format"],
        "scope": "represented radial and attack-dispersion weapon records",
        "weapons": weapons,
        "projectiles": projectiles,
        "validation_boundary": {
            "dat_proves": [
                "raw minimum and maximum range fields",
                "raw area-effect range and area/blast level fields",
                "raw missed-missile and volley spread fields",
                "raw weapon offset, projectile links and missile attributes",
            ],
            "original_runtime_required": [
                "distance metric, impact center and area shape",
                "damage falloff or uniform-damage behavior",
                "blast-level and area-effect-level target filtering",
                "friendly-fire, collision and footprint interaction",
                "missed-shot sampling and volley projectile placement",
                "projectile flight, impact, terrain and elevation behavior",
            ],
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument(
        "--output", default="generated/combat_geometry_dat_metadata.json"
    )
    arguments = parser.parse_args()
    data = json.loads(Path(arguments.metadata).read_text())
    output = generate(data)
    destination = Path(arguments.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate bounded economy-tech and resource evidence from VER 5.7 JSON."""

import argparse
import json
from pathlib import Path

from generate_religious_metadata import CIV_EFFECT, capture, parse_tasks, parse_tech


TECHNOLOGIES = {
    "horse_collar": 14, "heavy_plow": 13, "crop_rotation": 12,
    "bow_saw": 203, "two_man_saw": 221,
    "gold_mining": 55, "gold_shaft_mining": 182,
    "stone_mining": 278, "stone_shaft_mining": 279,
    "hand_cart": 249,
}

FARM_GRAPHIC_IDS = {253, 254, 255, 1599, 1600, 1952, 1953}
FARM_SOUND_IDS = {416}

RESOURCE_AND_WORKER_IDS = {
    48, 50, 59, 65, 66, 69, 102, 450, 451, 458, 594,
    120, 122, 123, 124, 214, 216, 218, 220, 259, 579, 581, 590, 592,
}

HUNT_GRAPHIC_IDS = {
    758, 761, 764, 768, 1226, 1317, 1601, 1602, 1701,
    1954, 1958, 2453, 2454, 2455,
    2457, 3172, 3175, 3178, 3179, 3183, 3184, 3189, 3238,
}
HUNT_SOUND_IDS = {56, 57, 413, 449, 450, 451, 452, 453, 456, 457, 458}
HUNT_CIVILIZATION_EFFECTS = {
    "british_shepherd": 381,
    "goth_hunter": 414,
    "mongol_hunter": 388,
    "mayan_resource_duration": 449,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--output", default="generated/economy_dat_metadata.json")
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    effects = {effect["id"]: effect for effect in data["effects"]}
    techs = {technology["id"]: technology for technology in data["techs"]}

    units = {}
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            if unit["id"] not in RESOURCE_AND_WORKER_IDS or str(unit["id"]) in units:
                continue
            record = dict(unit)
            if record["action"]:
                action = dict(record["action"])
                action["tasks_structured"] = parse_tasks(action.pop("tasks"))
                record["action"] = action
            units[str(unit["id"])] = record

    restriction_ids = sorted({
        unit["terrain_restriction_id"] for unit in units.values()
    })
    restrictions = {
        str(item["id"]): item
        for item in data["terrain_restrictions"]
        if item["id"] in restriction_ids
    }
    technology_records = {
        name: parse_tech(techs[technology_id], effects)
        for name, technology_id in TECHNOLOGIES.items()
    }
    farm = units["50"]
    farmers = {key: units[key] for key in ("214", "259")}
    farm_evidence = {
        "farm_unit_id": 50,
        "wood_cost": farm["creation"]["costs"][0],
        "create_time": farm["creation"]["create_time"],
        "create_at_unit": farm["creation"]["create_at_unit"],
        "create_button": farm["creation"]["create_button"],
        "hit_points": farm["hit_points"],
        "action_work_rate": farm["action"]["work_rate"],
        "attribute_max_amount": farm["attribute_max_amount"],
        "attributes": farm["attributes"],
        "standing_graphic": farm["standing_graphic"],
        "construction_graphic": farm["construction_graphic"],
        "dying_graphic": farm["dying_graphic"],
        "damage_sound": farm["damage_sound"],
        "death_sound": farm["death_sound"],
        "selected_sound": farm["selected_sound"],
        "farmer_units": {
            key: {
                "action_work_rate": worker["action"]["work_rate"],
                "carry_capacity": worker["attribute_max_amount"],
                "farm_task": worker["action"]["tasks_structured"][0],
            }
            for key, worker in farmers.items()
        },
        "graphics": {
            str(graphic["id"]): graphic for graphic in data["graphics"]
            if graphic["id"] in FARM_GRAPHIC_IDS
        },
        "sounds": {
            str(sound["id"]): sound for sound in data["sounds"]
            if sound["id"] in FARM_SOUND_IDS
        },
        "capacity_resource_effects": {
            name: [
                command for command in technology_records[name]["effect_commands"]
                if command["type"] == 1 and command["a"] == 36
            ]
            for name in ("horse_collar", "heavy_plow", "crop_rotation")
        },
    }
    hunt_evidence = {
        "animals": {
            key: {
                field: units[key][field]
                for field in (
                    "id", "unit_class", "hit_points", "attributes",
                    "attribute_max_amount", "attribute_rot", "standing_graphic",
                    "walking_graphic", "running_graphic", "attack_graphic",
                    "dying_graphic", "selected_sound", "damage_sound",
                    "death_sound",
                )
            }
            for key in ("48", "65", "594")
        },
        "workers": {
            key: {
                "action_work_rate": units[key]["action"]["work_rate"],
                "carry_capacity": units[key]["attribute_max_amount"],
                "tasks": units[key]["action"]["tasks_structured"],
            }
            for key in ("122", "216", "590", "592")
        },
        "graphics": {
            str(graphic["id"]): graphic for graphic in data["graphics"]
            if graphic["id"] in HUNT_GRAPHIC_IDS
        },
        "sprite_sounds": [
            item for item in data["sprite_sounds"]
            if item["graphic_id"] in HUNT_GRAPHIC_IDS
        ],
        "sounds": {
            str(sound["id"]): sound for sound in data["sounds"]
            if sound["id"] in HUNT_SOUND_IDS
        },
        "civilization_effects": {
            name: effects[effect_id]
            for name, effect_id in HUNT_CIVILIZATION_EFFECTS.items()
        },
    }
    availability = []
    for civilization in data["civilizations"][1:]:
        disabled = {
            int(command["d"])
            for command in effects[CIV_EFFECT[civilization["id"]]]["commands"]
            if command["type"] == 102 and command["d"] >= 0
        }
        availability.append({
            "id": civilization["id"],
            "name": civilization["name"],
            "technologies": {
                name: "available" if technology_id not in disabled else "unavailable"
                for name, technology_id in TECHNOLOGIES.items()
            },
        })

    output = {
        "source_format": data["format"],
        "scope": "ten economy technologies and bounded resource ecology",
        "technologies": technology_records,
        "farm_evidence": farm_evidence,
        "hunt_evidence": hunt_evidence,
        "availability": availability,
        "units": units,
        "terrain_restrictions": restrictions,
        "terrains": [
            {
                key: terrain[key] for key in (
                    "id", "enabled", "slp_id", "sound_id",
                    "passable_terrain_id", "impassable_terrain_id",
                    "terrain_objects",
                )
            }
            for terrain in data["terrains"]
        ],
        "validation_boundary": {
            "dat_proves": [
                "technology costs, times, prerequisites, locations, slots and icons",
                "raw effect commands and civilization disable boundaries",
                "resource amounts, worker work rates/tasks, animal decay fields",
                "animal graphics, graphic-event sounds, terrain restriction tables",
            ],
            "original_runtime_required": [
                "gather cadence and floating-point rounding",
                "carcass death eligibility, decay rounding and same-tick ordering",
                "drop-off, retarget, depletion and farm reseed ordering",
                "terrain movement/build semantics and map-generation placement",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

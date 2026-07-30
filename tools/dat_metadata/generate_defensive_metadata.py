#!/usr/bin/env python3
"""Generate bounded defensive-infrastructure evidence from VER 5.7 JSON."""

import argparse
import json
from pathlib import Path

from generate_religious_metadata import CIV_EFFECT, parse_tasks, parse_tech


TECHNOLOGIES = {
    "town_watch": 8,
    "town_patrol": 280,
    "masonry": 50,
    "architecture": 51,
    "ballistics": 93,
    "heated_shot": 380,
    "hoardings": 379,
    "sappers": 321,
}
GATE_ID = 332
UNIT_IDS = {
    "outpost": 598,
    "villager": 83,
    "watch_tower": 79,
    "guard_tower": 234,
    "keep": 235,
    "bombard_tower": 236,
    "castle": 82,
    "town_center": 109,
}


def sprite_id(value):
    if value in (None, "None"):
        return None
    return int(value.removeprefix("Some(SpriteID(").removesuffix("))"))


def enrich_multiplicative_packed_combat(technology):
    for command in technology["effect_commands"]:
        if command["type"] != 5 or command["c"] not in (8, 9):
            continue
        raw = int(command["d"]) & 0xFFFF
        packed_class = (raw >> 8) & 0xFF
        packed_amount = raw & 0xFF
        if packed_class >= 128:
            packed_class -= 256
        command["packed_class"] = packed_class
        command["packed_amount"] = packed_amount
        if command["c"] == 9:
            command["packed_attack_class"] = packed_class
            command["packed_attack_amount"] = packed_amount


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument(
        "--output", default="generated/defensive_dat_metadata.json"
    )
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    effects = {effect["id"]: effect for effect in data["effects"]}
    techs = {technology["id"]: technology for technology in data["techs"]}
    graphics = {graphic["id"]: graphic for graphic in data["graphics"]}
    sounds = {sound["id"]: sound for sound in data["sounds"]}
    sprite_sounds = {
        item["graphic_id"]: item for item in data["sprite_sounds"]
    }

    wanted = set(UNIT_IDS.values())
    units = {}
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            if unit["id"] not in wanted or unit["id"] in units:
                continue
            record = dict(unit)
            if record["action"]:
                action = dict(record["action"])
                action["tasks_structured"] = parse_tasks(action.pop("tasks"))
                record["action"] = action
            units[unit["id"]] = record

    linked_graphics = set()
    linked_sounds = set()
    for unit in units.values():
        for key in (
            "standing_graphic", "standing_graphic_2", "dying_graphic",
            "walking_graphic", "running_graphic", "attack_graphic",
            "construction_graphic",
        ):
            if unit[key] is not None:
                linked_graphics.add(unit[key])
        for key in (
            "train_sound", "damage_sound", "selected_sound", "death_sound",
        ):
            if unit[key] is not None:
                linked_sounds.add(unit[key])
        if unit["action"]:
            for key in ("command_sound", "move_sound"):
                if unit["action"][key] is not None:
                    linked_sounds.add(unit["action"][key])
            for task in unit["action"]["tasks_structured"]:
                for key in (
                    "move_sprite", "work_sprite", "work_sprite2", "carry_sprite",
                ):
                    if (graphic_id := sprite_id(task[key])) is not None:
                        linked_graphics.add(graphic_id)

    graphic_records = []
    for graphic_id in sorted(linked_graphics):
        record = dict(graphics[graphic_id])
        trigger = sprite_sounds.get(graphic_id)
        record["sound_triggers"] = trigger
        if trigger:
            if trigger["sound_id"] is not None:
                linked_sounds.add(trigger["sound_id"])
            for angle in trigger["attack_sounds"]:
                linked_sounds.update(item["sound_id"] for item in angle)
        graphic_records.append(record)

    technology_records = {
        name: parse_tech(techs[technology_id], effects)
        for name, technology_id in TECHNOLOGIES.items()
    }
    for technology in technology_records.values():
        enrich_multiplicative_packed_combat(technology)
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
            "outpost": "available" if GATE_ID not in disabled else "unavailable",
            "technologies": {
                name: "available" if technology_id not in disabled else "unavailable"
                for name, technology_id in TECHNOLOGIES.items()
            },
        })

    ballistics = technology_records["ballistics"]["effect_commands"]
    output = {
        "source_format": data["format"],
        "scope": (
            "Outpost, represented garrison buildings, and eight defensive "
            "infrastructure technologies"
        ),
        "entities": {
            name: units[unit_id] for name, unit_id in UNIT_IDS.items()
        },
        "gate": parse_tech(techs[GATE_ID], effects),
        "technologies": technology_records,
        "ballistics_projectile_ids": sorted({
            command["a"] for command in ballistics if command["type"] == 0
        }),
        "availability": availability,
        "graphics": graphic_records,
        "sounds": [sounds[sound_id] for sound_id in sorted(linked_sounds)],
        "validation_boundary": {
            "dat_proves": [
                "records, costs, times, locations, slots, icons and prerequisites",
                "raw effects, target IDs/classes, gates and civilization disables",
                "Outpost and supporting graphic-to-SLP/sound-to-WAV links",
                "raw garrison capacity, type mask, heal and repair fields",
            ],
            "original_runtime_required": [
                "garrison type-mask interpretation and accepted unit classes",
                "capacity modifiers, occupancy cost, ejection and death behavior",
                "garrisoned projectile count, attack contribution and targeting",
                "fog reveal persistence and Town Watch/Patrol update timing",
                "Ballistics projectile lead algorithm and miss/collision behavior",
                "Heated Shot target filtering and damage resolution",
                "building HP/armor rounding and repair interaction",
                "Sappers villager-only building-damage resolution",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

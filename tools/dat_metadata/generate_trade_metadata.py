#!/usr/bin/env python3
"""Generate bounded trade and fish-trap evidence from VER 5.7 JSON."""

import argparse
import json
from pathlib import Path

from generate_religious_metadata import CIV_EFFECT, parse_tasks, parse_tech


TECHNOLOGIES = {
    "coinage": 23, "banking": 17, "cartography": 19,
    "caravan": 48, "guilds": 15,
}
GATES = {"trade_cog": 180, "fish_trap": 357}
UNIT_IDS = {17, 199, 84, 45, 13, 128}


def sprite_id(value):
    if value in (None, "None"):
        return None
    return int(value.removeprefix("Some(SpriteID(").removesuffix("))"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--output", default="generated/trade_dat_metadata.json")
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    effects = {effect["id"]: effect for effect in data["effects"]}
    techs = {technology["id"]: technology for technology in data["techs"]}
    graphics = {graphic["id"]: graphic for graphic in data["graphics"]}
    sounds = {sound["id"]: sound for sound in data["sounds"]}
    sprite_sounds = {
        item["graphic_id"]: item for item in data["sprite_sounds"]
    }

    units = {}
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            if unit["id"] not in UNIT_IDS or str(unit["id"]) in units:
                continue
            record = dict(unit)
            if record["action"]:
                action = dict(record["action"])
                action["tasks_structured"] = parse_tasks(action.pop("tasks"))
                record["action"] = action
            units[str(unit["id"])] = record

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

    gate_records = {
        name: parse_tech(techs[technology_id], effects)
        for name, technology_id in GATES.items()
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
            "entities": {
                name: "available" if technology_id not in disabled else "unavailable"
                for name, technology_id in GATES.items()
            },
            "technologies": {
                name: "available" if technology_id not in disabled else "unavailable"
                for name, technology_id in TECHNOLOGIES.items()
            },
        })

    output = {
        "source_format": data["format"],
        "scope": "Trade Cog, Fish Trap and five Market technologies",
        "entities": {"trade_cog": units["17"], "fish_trap": units["199"]},
        "supporting_entities": {
            name: units[str(unit_id)] for name, unit_id in {
                "market": 84, "dock": 45, "fishing_ship": 13, "trade_cart": 128
            }.items()
        },
        "gates": gate_records,
        "technologies": {
            name: parse_tech(techs[technology_id], effects)
            for name, technology_id in TECHNOLOGIES.items()
        },
        "availability": availability,
        "graphics": graphic_records,
        "sounds": [sounds[sound_id] for sound_id in sorted(linked_sounds)],
        "validation_boundary": {
            "dat_proves": [
                "records, producers/builders, costs, times, slots and icons",
                "raw effect commands, tasks, gates and civilization disables",
                "graphic-to-SLP and conceptual sound-to-WAV resource links",
            ],
            "original_runtime_required": [
                "trade distance-to-gold formula and rounding",
                "ally/endpoint interruption and team-vision semantics",
                "market fee behavior and Fish Trap depletion/rebuild ordering",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

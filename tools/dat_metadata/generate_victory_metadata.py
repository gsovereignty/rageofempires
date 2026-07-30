#!/usr/bin/env python3
"""Generate bounded Wonder and victory evidence from VER 5.7 JSON."""

import argparse
import json
from pathlib import Path

from generate_religious_metadata import CIV_EFFECT, parse_tasks, parse_tech


WONDER_ID = 276
WONDER_GATE_ID = 144
ATHEISM_ID = 21
VICTORY_RESOURCE_IDS = {196, 197}


def sprite_id(value):
    if value in (None, "None"):
        return None
    return int(value.removeprefix("Some(SpriteID(").removesuffix("))"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument(
        "--output", default="generated/victory_dat_metadata.json"
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

    wonder = None
    for civilization in data["civilizations"]:
        for unit in civilization["units"]:
            if unit["id"] == WONDER_ID:
                wonder = dict(unit)
                break
        if wonder is not None:
            break
    if wonder["action"]:
        action = dict(wonder["action"])
        action["tasks_structured"] = parse_tasks(action.pop("tasks"))
        wonder["action"] = action

    linked_graphics = {
        wonder[key] for key in (
            "standing_graphic", "standing_graphic_2", "dying_graphic",
            "construction_graphic",
        ) if wonder[key] is not None
    }
    linked_sounds = {
        wonder[key] for key in (
            "selected_sound", "damage_sound", "death_sound", "train_sound",
        ) if wonder[key] is not None
    }
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

    gate = parse_tech(techs[WONDER_GATE_ID], effects)
    atheism = parse_tech(techs[ATHEISM_ID], effects)
    victory_resource_commands = []
    for effect in data["effects"]:
        for command in effect["commands"]:
            if command["type"] in (1, 6) and command["a"] in VICTORY_RESOURCE_IDS:
                victory_resource_commands.append({
                    "effect_id": effect["id"],
                    "effect_name": effect["name"],
                    "command": command,
                })

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
            "wonder": (
                "available" if WONDER_GATE_ID not in disabled else "unavailable"
            ),
            "wonder_plans": (
                "available" if WONDER_GATE_ID not in disabled else "unavailable"
            ),
            "atheism": (
                "available" if civilization["id"] == 17 else "unavailable"
            ),
        })

    output = {
        "source_format": data["format"],
        "scope": "Wonder, Wonder Plans and bounded victory-resource evidence",
        "wonder": wonder,
        "wonder_gate": gate,
        "atheism": atheism,
        "victory_resource_commands": victory_resource_commands,
        "availability": availability,
        "graphics": graphic_records,
        "sounds": [sounds[sound_id] for sound_id in sorted(linked_sounds)],
        "extractor_absence": [
            "pinned genie-rs exposes no victory-mode or countdown table",
            "pinned extractor emits no localized message table",
            "civilization resource defaults are not exposed by pinned API",
        ],
        "validation_boundary": {
            "dat_proves": [
                "Wonder record, gate, costs, time, tasks and civilization presence",
                "Atheism technology and raw resource 196/197 commands",
                "Wonder graphic-to-SLP and conceptual sound-to-WAV links",
            ],
            "original_runtime_required": [
                "Wonder/relic countdown lengths, units and reset behavior",
                "relic ownership threshold and team/allied aggregation",
                "score, time-limit and conquest victory rules",
                "Atheism resource meanings and countdown/relic-income semantics",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

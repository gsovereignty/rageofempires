#!/usr/bin/env python3
"""Generate bounded Missionary and monastery-tech evidence from VER 5.7 JSON."""

import argparse
import json
import re
from pathlib import Path


CIV_EFFECT = {
    1: 254, 2: 258, 3: 259, 4: 262, 5: 255, 6: 257, 7: 256,
    8: 260, 9: 261, 10: 263, 11: 276, 12: 277, 13: 275,
    14: 446, 15: 447, 16: 449, 17: 448, 18: 504,
}

TECHNOLOGIES = {
    "sanctity": 231,
    "fervor": 252,
    "redemption": 316,
    "atonement": 319,
    "illumination": 233,
    "block_printing": 230,
    "faith": 45,
    "theocracy": 438,
    "heresy": 439,
}


def capture(record, pattern, transform=int):
    match = re.search(pattern, record)
    if not match:
        raise ValueError(f"cannot parse tech record with {pattern!r}: {record}")
    return transform(match.group(1))


def parse_tech(technology, effects):
    record = technology["record"]
    required = [
        int(value)
        for value in capture(
            record, r"required_techs: \[([^\]]*)\]", str
        ).replace("TechID(", "").replace(")", "").split(", ")
        if value
    ]
    costs = [
        {"resource": int(resource), "amount": int(amount)}
        for resource, amount in re.findall(
            r"effect_type: (\d+), amount: (\d+), enabled: true", record
        )
    ]
    location_match = re.search(r"location: Some\(UnitTypeID\((\d+)\)\)", record)
    icon_match = re.search(r"icon_id: Some\((\d+)\)", record)
    effect_id = capture(record, r"time2: (\d+)")
    return {
        "id": technology["id"],
        "name": technology["name"],
        "required_techs": required,
        "costs": costs,
        "location": int(location_match.group(1)) if location_match else None,
        "research_time": capture(record, r"time: (\d+)"),
        "effect_id_raw": effect_id,
        "effect_commands": effects[effect_id]["commands"],
        "icon": int(icon_match.group(1)) if icon_match else None,
        "button": capture(record, r"button_id: (\d+)"),
        "civilization_id": (
            int(match.group(1))
            if (match := re.search(
                r"civilization_id: Some\(CivilizationID\((\d+)\)\)", record
            ))
            else None
        ),
    }


def parse_tasks(text):
    tasks = []
    for body in re.findall(r"Task \{ (.*?) \}(?:,|])", text):
        def field(name, pattern, transform=int):
            match = re.search(rf"{name}: {pattern}", body)
            return transform(match.group(1)) if match else None

        tasks.append({
            "id": field("id", r"(\d+)"),
            "is_default": field(
                "is_default", r"(true|false)", lambda value: value == "true"
            ),
            "action_type": field("action_type", r"(\d+)"),
            "object_class": field("object_class", r"(-?\d+)"),
            "object_id": field("object_id", r"(None|Some\(UnitTypeID\(\d+\)\))", str),
            "terrain_id": field("terrain_id", r"(-?\d+)"),
            "attribute_types": field("attribute_types", r"(\([^)]+\))", str),
            "work_values": field("work_values", r"(\([^)]+\))", str),
            "work_range": field("work_range", r"(-?[\d.]+)", float),
            "auto_search_targets": field(
                "auto_search_targets", r"(true|false)", lambda value: value == "true"
            ),
            "search_wait_time": field("search_wait_time", r"(-?[\d.]+)", float),
            "enable_targeting": field(
                "enable_targeting", r"(true|false)", lambda value: value == "true"
            ),
            "combat_level": field("combat_level", r"(\d+)"),
            "work_flags": field("work_flags", r"(\([^)]+\))", str),
            "owner_type": field("owner_type", r"(\d+)"),
            "holding_attribute": field("holding_attribute", r"(-?\d+)"),
            "state_building": field("state_building", r"(-?\d+)"),
            "move_sprite": field(
                "move_sprite", r"(None|Some\(SpriteID\(\d+\)\))", str
            ),
            "work_sprite": field(
                "work_sprite", r"(None|Some\(SpriteID\(\d+\)\))", str
            ),
            "work_sprite2": field(
                "work_sprite2", r"(None|Some\(SpriteID\(\d+\)\))", str
            ),
            "carry_sprite": field(
                "carry_sprite", r"(None|Some\(SpriteID\(\d+\)\))", str
            ),
            "work_sound": field(
                "work_sound", r"(None|Some\(SoundID\(\d+\)\))", str
            ),
            "work_sound2": field(
                "work_sound2", r"(None|Some\(SoundID\(\d+\)\))", str
            ),
        })
    return tasks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--output", default="generated/religious_dat_metadata.json")
    args = parser.parse_args()

    data = json.loads(Path(args.metadata).read_text())
    effects = {effect["id"]: effect for effect in data["effects"]}
    technologies = {technology["id"]: technology for technology in data["techs"]}
    sounds = {sound["id"]: sound for sound in data["sounds"]}
    graphics = {graphic["id"]: graphic for graphic in data["graphics"]}
    sprite_sounds = {
        item["graphic_id"]: item for item in data["sprite_sounds"]
    }

    missionary = next(
        unit
        for civilization in data["civilizations"]
        for unit in civilization["units"]
        if unit["id"] == 775
    )
    gate = parse_tech(technologies[84], effects)
    action = dict(missionary["action"])
    action["tasks_structured"] = parse_tasks(action.pop("tasks"))
    missionary = dict(missionary)
    missionary["action"] = action

    linked_graphic_ids = {
        missionary[key]
        for key in (
            "standing_graphic", "dying_graphic", "walking_graphic",
            "running_graphic", "attack_graphic",
        )
        if missionary[key] is not None
    }
    linked_graphic_ids.update(
        int(value.removeprefix("Some(SpriteID(").removesuffix("))"))
        for task in action["tasks_structured"]
        if (value := task["work_sprite"]) != "None"
    )
    graphic_records = []
    linked_sound_ids = {
        missionary[key]
        for key in (
            "train_sound", "damage_sound", "selected_sound", "death_sound"
        )
        if missionary[key] is not None
    }
    linked_sound_ids.update(
        action[key] for key in ("command_sound", "move_sound")
        if action[key] is not None
    )
    for graphic_id in sorted(linked_graphic_ids):
        record = dict(graphics[graphic_id])
        trigger = sprite_sounds.get(graphic_id)
        record["sound_triggers"] = trigger
        if trigger:
            if trigger["sound_id"] is not None:
                linked_sound_ids.add(trigger["sound_id"])
            for angle in trigger["attack_sounds"]:
                linked_sound_ids.update(item["sound_id"] for item in angle)
        graphic_records.append(record)

    availability = []
    for civilization in data["civilizations"][1:]:
        civilization_id = civilization["id"]
        disabled = {
            int(command["d"])
            for command in effects[CIV_EFFECT[civilization_id]]["commands"]
            if command["type"] == 102 and command["d"] >= 0
        }
        availability.append({
            "id": civilization_id,
            "name": civilization["name"],
            "missionary": (
                "available" if civilization_id == gate["civilization_id"]
                and 84 not in disabled else "unavailable"
            ),
            "technologies": {
                name: "available" if technology_id not in disabled else "unavailable"
                for name, technology_id in TECHNOLOGIES.items()
            },
        })

    output = {
        "source_format": data["format"],
        "scope": "Missionary 775 and nine monastery technologies",
        "missionary": missionary,
        "missionary_gate": gate,
        "technologies": {
            name: parse_tech(technologies[technology_id], effects)
            for name, technology_id in TECHNOLOGIES.items()
        },
        "availability": availability,
        "graphics": graphic_records,
        "sounds": [
            {
                **sounds[sound_id],
                "items": [
                    item for item in sounds[sound_id]["items"]
                    if item["civilization"] in (-1, gate["civilization_id"])
                ],
            }
            for sound_id in sorted(linked_sound_ids)
        ],
        "validation_boundary": {
            "dat_proves": [
                "records, costs, prerequisites, producer, panel slots and icons",
                "raw effect commands and civilization disable boundaries",
                "task restrictions, graphic-to-SLP links and sound resources",
            ],
            "original_runtime_required": [
                "conversion timing randomness and resistance",
                "Theocracy group charge semantics",
                "Heresy ownership and death edge cases",
            ],
        },
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

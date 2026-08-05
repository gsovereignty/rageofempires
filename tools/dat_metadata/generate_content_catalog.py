#!/usr/bin/env python3
"""Reduce full genie-rs DAT JSON into trackable gameplay catalog metadata.

Input is output from aoe-dat-metadata. Output contains no source DAT bytes and
no opaque Rust Debug records: every retained field has an explicit semantic
name and stable numeric identity.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TECH_RE = re.compile(
    r"^Tech \{ required_techs: \[(?P<required>.*?)\], effects: "
    r"\[(?P<effects>.*?)\], civilization_id: (?P<civ>.*?), "
    r"full_tech_mode: (?P<full>\d+), location: (?P<location>.*?), "
    r"language_dll_name: .*?, language_dll_description: .*?, "
    r"time: (?P<time>\d+), time2: (?P<time2>\d+), "
    r"type_: (?P<type>\d+), icon_id: (?P<icon>.*?), "
    r"button_id: (?P<button>\d+), language_dll_help: .*?, "
    r"help_page_id: \d+, hotkey: .*?, name: .* \}$"
)
ID_RE = re.compile(r"(?:TechID|CivilizationID|UnitTypeID)\((\d+)\)")
EFFECT_RE = re.compile(
    r"TechEffectRef \{ effect_type: (\d+), amount: (\d+), "
    r"enabled: (true|false) \}"
)
TASK_RE = re.compile(
    r"Task \{ id: (?P<id>\d+), is_default: (?P<default>true|false), "
    r"action_type: (?P<action>\d+), object_class: (?P<object_class>-?\d+), "
    r"object_id: (?P<object_id>.*?), terrain_id: (?P<terrain>-?\d+), "
    r"attribute_types: \((?P<attributes>.*?)\), work_values: "
    r"\((?P<work_values>.*?)\), work_range: (?P<work_range>[-\d.e+]+), "
    r"auto_search_targets: (?P<auto>true|false), search_wait_time: "
    r"(?P<wait>[-\d.e+]+), enable_targeting: (?P<target>true|false), "
    r"combat_level: (?P<combat>\d+), work_flags: \((?P<flags>.*?)\), "
    r"owner_type: (?P<owner>\d+), holding_attribute: (?P<holding>\d+), "
    r"state_building: (?P<state>\d+), move_sprite: (?P<move>.*?), "
    r"work_sprite: (?P<work>.*?), work_sprite2: (?P<work2>.*?), "
    r"carry_sprite: (?P<carry>.*?), work_sound: (?P<sound>.*?), "
    r"work_sound2: (?P<sound2>.*?) \}"
)
CIV_EFFECT_RE = re.compile(
    r"civ_effect: (?P<civ>\d+), bonus_effect: "
    r"(?P<bonus>None|Some\((?P<bonus_id>\d+)\))"
)


def optional_id(value: str) -> int | None:
    if value == "None":
        return None
    match = ID_RE.search(value)
    if not match:
        raise ValueError(f"invalid optional ID: {value}")
    return int(match.group(1))


def optional_numeric(value: str) -> int | None:
    if value == "None":
        return None
    match = re.search(r"\((\d+)\)", value)
    if not match:
        raise ValueError(f"invalid optional numeric value: {value}")
    return int(match.group(1))


def parse_tasks(value: str) -> list[dict[str, Any]]:
    if value in ("None", "Some(TaskList([]))"):
        return []
    matches = list(TASK_RE.finditer(value))
    if not matches:
        raise ValueError("cannot decode task list")
    return [{
        "id": int(match.group("id")),
        "is_default": match.group("default") == "true",
        "action_type": int(match.group("action")),
        "object_class": int(match.group("object_class")),
        "object_id": optional_numeric(match.group("object_id")),
        "terrain_id": int(match.group("terrain")),
        "attribute_types": [
            int(part.strip()) for part in match.group("attributes").split(",")
        ],
        "work_values": [
            float(part.strip()) for part in match.group("work_values").split(",")
        ],
        "work_range": float(match.group("work_range")),
        "auto_search_targets": match.group("auto") == "true",
        "search_wait_time": float(match.group("wait")),
        "enable_targeting": match.group("target") == "true",
        "combat_level": int(match.group("combat")),
        "work_flags": [
            int(part.strip()) for part in match.group("flags").split(",")
        ],
        "owner_type": int(match.group("owner")),
        "holding_attribute": int(match.group("holding")),
        "state_building": int(match.group("state")),
        "move_graphic": optional_numeric(match.group("move")),
        "work_graphic": optional_numeric(match.group("work")),
        "secondary_work_graphic": optional_numeric(match.group("work2")),
        "carry_graphic": optional_numeric(match.group("carry")),
        "work_sound": optional_numeric(match.group("sound")),
        "secondary_work_sound": optional_numeric(match.group("sound2")),
    } for match in matches]


def parse_tech(record: dict[str, Any]) -> dict[str, Any]:
    match = TECH_RE.match(record["record"])
    if not match:
        raise ValueError(f"cannot decode technology {record['id']}")
    return {
        "id": record["id"],
        "name": record["name"],
        "required_technologies": [
            int(value) for value in ID_RE.findall(match.group("required"))
        ],
        "costs": [
            {
                "resource_id": int(effect),
                "amount": int(amount),
                "enabled": enabled == "true",
            }
            for effect, amount, enabled in EFFECT_RE.findall(
                match.group("effects")
            )
        ],
        "civilization_id": optional_id(match.group("civ")),
        "full_technology_mode": int(match.group("full")),
        "research_location_object_id": optional_id(match.group("location")),
        "research_time": int(match.group("time")),
        "effect_id": int(match.group("time2")),
        "type": int(match.group("type")),
        "icon_id": (
            None if match.group("icon") == "None"
            else int(re.search(r"\d+", match.group("icon")).group())
        ),
        "button_id": int(match.group("button")),
    }


def reduce_unit(unit: dict[str, Any]) -> dict[str, Any]:
    keep = (
        "id", "copy_id", "unit_group", "base_class", "unit_class",
        "enabled", "disabled", "hit_points", "line_of_sight", "speed",
        "garrison_capacity", "terrain_restriction_id", "resource_group",
        "track_as_resource", "radius", "outline_radius", "obstruction_type",
        "selection_shape", "attribute_max_amount", "attribute_rot",
        "area_effect_level", "standing_graphic", "standing_graphic_2",
        "walking_graphic", "running_graphic", "dying_graphic",
        "attack_graphic", "construction_graphic", "button_icon",
        "portrait_icon", "train_sound", "selected_sound", "damage_sound",
        "death_sound", "attributes", "damage_sprites", "combat", "missile",
        "creation", "building", "moving",
    )
    result = {field: unit.get(field) for field in keep}
    action = unit.get("action")
    if action is not None:
        result["action"] = {
            key: action.get(key) for key in (
                "default_task", "search_radius", "work_rate",
                "command_sound", "move_sound", "task_list_source",
            )
        }
        result["action"]["tasks"] = parse_tasks(action["tasks"])
        result["action"]["tasks_semantically_available"] = True
    else:
        result["action"] = None
    return result


def generate(source: dict[str, Any]) -> dict[str, Any]:
    object_variants: list[dict[str, Any]] = []
    variant_ids: dict[str, int] = {}
    civilizations = []
    object_record_count = 0
    for civilization in source["civilizations"]:
        effects = CIV_EFFECT_RE.search(civilization["record"])
        if effects is None:
            raise ValueError("civilization effect bindings unavailable")
        units = [reduce_unit(unit) for unit in civilization["units"]]
        object_record_count += len(units)
        object_variant_ids = [65535] * (
            max((unit["id"] for unit in units), default=-1) + 1
        )
        for unit in units:
            key = json.dumps(unit, sort_keys=True, separators=(",", ":"))
            variant_id = variant_ids.get(key)
            if variant_id is None:
                variant_id = len(object_variants)
                variant_ids[key] = variant_id
                object_variants.append(unit)
            object_variant_ids[unit["id"]] = variant_id
        civilizations.append({
            "id": civilization["id"],
            "name": civilization["name"],
            "civilization_effect_id": int(effects.group("civ")),
            "bonus_effect_id": (
                None if effects.group("bonus") == "None"
                else int(effects.group("bonus_id"))
            ),
            "object_variant_ids": object_variant_ids,
        })
    effects = json.loads(json.dumps(source["effects"]))
    for effect in effects:
        for command in effect["commands"]:
            if command["type"] not in (0, 4, 5) or command["c"] not in (8, 9):
                continue
            raw = int(command["d"]) & 0xFFFF
            packed_class = (raw >> 8) & 0xFF
            packed_amount = raw & 0xFF
            if packed_class >= 128:
                packed_class -= 256
            if packed_amount >= 128:
                packed_amount -= 256
            command["packed_class"] = packed_class
            command["packed_amount"] = packed_amount
            if command["c"] == 9:
                command["packed_attack_class"] = packed_class
                command["packed_attack_amount"] = packed_amount
    return {
        "schema": "aoe-content-catalog-2",
        "source_format": source["format"],
        "civilization_count": len(civilizations),
        "object_record_count": object_record_count,
        "object_variant_count": len(object_variants),
        "technology_count": len(source["techs"]),
        "effect_count": len(source["effects"]),
        "civilizations": civilizations,
        "object_variants": object_variants,
        "technologies": [parse_tech(tech) for tech in source["techs"]],
        "effects": effects,
    }


def cpp_optional(value: Any) -> str:
    return "std::nullopt" if value is None else str(value)


def cpp_float(value: Any) -> str:
    rendered = f"{float(value or 0):.9g}"
    if "." not in rendered and "e" not in rendered:
        rendered += ".0"
    return rendered + "f"


def cpp_string(value: str) -> str:
    return json.dumps(value)


def cpp_vector(type_name: str, values: list[str]) -> str:
    return f"std::vector<{type_name}>{{{','.join(values)}}}"


def emit_cpp(catalog: dict[str, Any], output: Path) -> None:
    classes = {
        "Static": "static_object", "Animated": "animated",
        "Doppelganger": "doppelganger", "Moving": "moving",
        "Action": "action", "BaseCombat": "base_combat",
        "Missile": "missile", "Combat": "combat",
        "Building": "building", "Tree": "tree",
    }
    lines = ["// Generated by generate_content_catalog.py. Do not edit."]
    lines.append("catalog.civilization_ids_ = {")
    lines.append(",".join(str(c["id"]) for c in catalog["civilizations"]))
    lines.append("};")
    lines.append("catalog.civilization_effect_ids_ = {")
    lines.append(",".join(
        str(c["civilization_effect_id"]) for c in catalog["civilizations"]
    ))
    lines.append("};")
    lines.append("catalog.civilization_bonus_effect_ids_ = {")
    lines.append(",".join(
        cpp_optional(c["bonus_effect_id"]) for c in catalog["civilizations"]
    ))
    lines.append("};")
    lines.append("catalog.civilization_variant_ids_ = {")
    for civilization in catalog["civilizations"]:
        lines.append("{" + ",".join(
            str(value) for value in civilization["object_variant_ids"]
        ) + "},")
    lines.append("};")
    lines.append("catalog.object_variants_.reserve(%d);" % len(
        catalog["object_variants"]
    ))
    for unit in catalog["object_variants"]:
        combat = unit.get("combat") or {}
        creation = unit.get("creation") or {}
        action = unit.get("action") or {}
        building = unit.get("building") or {}
        attacks = cpp_vector("CommercialClassAmount", [
            "{%s,%s}" % (value["class"], value["amount"])
            for value in combat.get("weapons", [])
        ])
        armors = cpp_vector("CommercialClassAmount", [
            "{%s,%s}" % (value["class"], value["amount"])
            for value in combat.get("armors", [])
        ])
        costs = cpp_vector("CommercialResourceCost", [
            "{%s,%s,%s}" % (
                value["resource"], value["amount"],
                "true" if value["flag"] else "false",
            ) for value in creation.get("costs", [])
        ])
        tasks = cpp_vector("CommercialTask", [
            "{%d,%s,%d,%d,%s,%d,{%s},{%s},%s,%s,%s,%s,%d,{%s},%d,%d,%d,%s,%s,%s,%s,%s,%s}" % (
                task["id"], "true" if task["is_default"] else "false",
                task["action_type"], task["object_class"],
                cpp_optional(task["object_id"]), task["terrain_id"],
                ",".join(str(v) for v in task["attribute_types"]),
                ",".join(cpp_float(v) for v in task["work_values"]),
                cpp_float(task["work_range"]),
                "true" if task["auto_search_targets"] else "false",
                cpp_float(task["search_wait_time"]),
                "true" if task["enable_targeting"] else "false",
                task["combat_level"],
                ",".join(str(v) for v in task["work_flags"]),
                task["owner_type"], task["holding_attribute"],
                task["state_building"], cpp_optional(task["move_graphic"]),
                cpp_optional(task["work_graphic"]),
                cpp_optional(task["secondary_work_graphic"]),
                cpp_optional(task["carry_graphic"]),
                cpp_optional(task["work_sound"]),
                cpp_optional(task["secondary_work_sound"]),
            ) for task in action.get("tasks", [])
        ])
        stored_resources = cpp_vector("CommercialStoredResource", [
            "{%s,%s,%s}" % (
                value["type"], cpp_float(value["amount"]), value["flag"]
            ) for value in unit.get("attributes", [])
        ])
        graphics = [cpp_optional(unit.get(name)) for name in (
            "standing_graphic", "standing_graphic_2", "walking_graphic",
            "running_graphic", "dying_graphic", "attack_graphic",
            "construction_graphic", "button_icon", "portrait_icon",
            "train_sound", "selected_sound", "damage_sound", "death_sound",
        )]
        radius = unit.get("radius") or [0, 0, 0]
        fields = [
            str(unit["id"]), str(unit["copy_id"]), str(unit["unit_group"]),
            "CommercialObjectBaseClass::" + classes[unit["base_class"]],
            str(unit["unit_class"]),
            "true" if unit["enabled"] else "false",
            "true" if unit["disabled"] else "false",
            str(unit["hit_points"]), cpp_float(unit["line_of_sight"]),
            cpp_float(unit["speed"]), cpp_float(action.get("work_rate", 0)),
            cpp_float(building.get("garrison_heal_rate", 0)),
            cpp_optional(action.get("command_sound")),
            cpp_optional(action.get("move_sound")),
            str(unit["garrison_capacity"]),
            str(unit["terrain_restriction_id"]), str(unit["resource_group"]),
            "true" if unit["track_as_resource"] else "false",
            "{" + ",".join(cpp_float(v) for v in radius) + "}",
            str(unit["obstruction_type"]), str(unit["selection_shape"]),
            str(combat.get("displayed_attack", 0)), str(combat.get("armor", 0)),
            "0", cpp_float(combat.get("range_max", 0)),
            cpp_float(combat.get("range_min", 0)),
            cpp_float(combat.get("reload_time", 0)),
            cpp_float(combat.get("area_effect_range", 0)),
            str(combat.get("accuracy", 0)), str(combat.get("frame_delay", 0)),
            cpp_optional(combat.get("missile_unit")), attacks, armors, costs,
            str(creation.get("create_time", 0)),
            cpp_optional(creation.get("create_at_unit")),
            str(creation.get("create_button", 0)), *graphics, tasks,
            stored_resources,
        ]
        lines.append(
            "catalog.object_variants_.push_back({" + ",".join(fields) + "});"
        )
    lines.append("catalog.technologies_.reserve(%d);" % len(
        catalog["technologies"]
    ))
    for tech in catalog["technologies"]:
        prerequisites = cpp_vector("CommercialTechnologyId", [
            str(value) for value in tech["required_technologies"]
        ])
        costs = cpp_vector("CommercialTechnologyCost", [
            "{%d,%d,%s}" % (
                cost["resource_id"], cost["amount"],
                "true" if cost["enabled"] else "false",
            ) for cost in tech["costs"]
        ])
        lines.append(
            "catalog.technologies_.push_back({%d,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s,%d});"
            % (tech["id"], cpp_string(tech["name"]), prerequisites, costs,
               cpp_optional(tech["civilization_id"]),
               "true" if tech["full_technology_mode"] else "false",
               cpp_optional(tech["research_location_object_id"]),
               tech["research_time"], tech["effect_id"],
               tech["type"], cpp_optional(tech["icon_id"]), tech["button_id"])
        )
    lines.append("catalog.effects_.reserve(%d);" % len(catalog["effects"]))
    for effect in catalog["effects"]:
        commands = cpp_vector("CommercialEffectCommand", [
            "{%d,%d,%d,%d,%s,%s,%s}" % (
                command["type"], command["a"], command["b"], command["c"],
                cpp_float(command["d"]),
                cpp_optional(command.get("packed_class")),
                cpp_optional(command.get("packed_amount")),
            ) for command in effect["commands"]
        ])
        lines.append("catalog.effects_.push_back({%d,%s,%s});" % (
            effect["id"], cpp_string(effect["name"]), commands
        ))
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cpp-output", type=Path)
    args = parser.parse_args()
    source = json.loads(args.input.read_text(encoding="utf-8"))
    output = generate(source)
    args.output.write_text(
        json.dumps(output, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    if args.cpp_output:
        emit_cpp(output, args.cpp_output)


if __name__ == "__main__":
    main()

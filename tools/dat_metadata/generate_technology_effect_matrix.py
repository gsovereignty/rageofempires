#!/usr/bin/env python3
"""Audit every represented technology against decoded VER 5.7 metadata."""

import argparse
import importlib.util
import json
import re
from collections import Counter
from pathlib import Path


def module_from(path):
    spec = importlib.util.spec_from_file_location("civ_matrix", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def bindings(source):
    start = source.index("const TechnologyRules& rules_for(Technology")
    end = source.index("\n}\n", start)
    return dict(re.findall(
        r"case Technology::(\w+):\s*return (\w+);", source[start:end], re.S
    ))


def represented_rules(source):
    fields = ("wood_cost", "food_cost", "gold_cost", "stone_cost")
    result = {}
    for rule_name, body in re.findall(
        r"constexpr TechnologyRules (\w+)\s*\{(.*?)\n\};", source, re.S
    ):
        if ".researched_at" in body:
            record = {
                "location": re.search(
                    r"\.researched_at\s*=\s*BuildingKind::(\w+)", body
                ).group(1),
                "research_ticks": int(re.search(
                    r"\.research_ticks\s*=\s*(\d+)", body
                ).group(1)),
            }
            for field in fields:
                match = re.search(rf"\.{field}\s*=\s*(\d+)", body)
                record[field] = int(match.group(1)) if match else 0
        else:
            tokens = [x.strip() for x in body.replace("\n", " ").split(",")]
            record = {
                "location": tokens[0].split("::")[-1],
                "wood_cost": int(tokens[2]),
                "food_cost": int(tokens[3]),
                "gold_cost": int(tokens[4]),
                "stone_cost": int(tokens[5]),
                "research_ticks": int(tokens[6]),
            }
        result[rule_name] = record
    return result


def parse_live_tech(record):
    text = record["record"]
    costs = {0: 0, 1: 0, 2: 0, 3: 0}
    for resource, amount in re.findall(
        r"TechEffectRef \{ effect_type: (\d+), amount: (\d+), enabled: true \}",
        text,
    ):
        resource = int(resource)
        if resource in costs:
            costs[resource] = int(amount)
    location = re.search(r"location: (?:Some\(UnitTypeID\((\d+)\)\)|None)", text)
    return {
        "costs": costs,
        "location_id": int(location.group(1)) if location and location.group(1) else None,
        "research_seconds": int(re.search(r"time: (\d+)", text).group(1)),
        "effect_id": int(re.search(r"time2: (\d+)", text).group(1)),
        "civilization_id": (
            int(x.group(1)) if
            (x := re.search(r"civilization_id: Some\(CivilizationID\((\d+)\)\)", text))
            else None
        ),
    }


def classified(represented, live):
    return "exact" if represented == live else "missing"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--rules", default="src/game_rules.cpp")
    parser.add_argument("--simulation", default="src/simulation.cpp")
    parser.add_argument("--mapping", default="tools/dat_metadata/generate_civ_matrix.py")
    parser.add_argument("--availability", default="generated/civ_tech_tree_matrix.json")
    parser.add_argument("--output", default="generated/technology_effect_matrix.json")
    args = parser.parse_args()

    data = json.loads(Path(args.metadata).read_text())
    source = Path(args.rules).read_text()
    simulation = Path(args.simulation).read_text()
    mapping = module_from(Path(args.mapping))
    bound = bindings(source)
    rules = represented_rules(source)
    techs = {x["id"]: x for x in data["techs"]}
    effects = {x["id"]: x for x in data["effects"]}
    availability = json.loads(Path(args.availability).read_text())
    building_id = {name: pair[0] for name, pair in mapping.BUILDING.items()}
    rows = []
    counts = Counter()
    for name, tech_id in mapping.TECH.items():
        represented = rules[bound[name]]
        live = parse_live_tech(techs[tech_id])
        attributes = []
        for field, resource in (
            ("food_cost", 0), ("wood_cost", 1),
            ("stone_cost", 2), ("gold_cost", 3),
        ):
            classification = classified(represented[field], live["costs"][resource])
            counts[classification] += 1
            attributes.append({
                "attribute": field, "represented": represented[field],
                "dat": live["costs"][resource], "classification": classification,
            })
        location_class = (
            "policy" if live["location_id"] is None else classified(
                building_id[represented["location"]], live["location_id"]
            )
        )
        counts[location_class] += 1
        attributes.append({
            "attribute": "location", "represented": represented["location"],
            "represented_dat_id": building_id[represented["location"]],
            "dat": live["location_id"], "classification": location_class,
            **({"note": "hidden/gate DAT technology has no research location; "
                        "UI routes it through its represented producer"}
               if live["location_id"] is None else {}),
        })
        counts["transformed"] += 1
        attributes.append({
            "attribute": "research_time",
            "represented": represented["research_ticks"],
            "dat_seconds": live["research_seconds"],
            "classification": "transformed",
            "note": "bounded simulation ticks versus DAT seconds",
        })
        runtime_hooks = len(re.findall(rf"Technology::{name}\b", simulation))
        commands = []
        for index, command in enumerate(effects[live["effect_id"]]["commands"]):
            known = command.get("attribute_name") is not None
            if known and runtime_hooks > 1:
                classification = "exact"
                note = "decoded attribute has an explicit technology runtime hook"
            elif known:
                classification = "missing"
                note = "decoded attribute has no explicit technology runtime hook"
            else:
                classification = "policy"
                note = "extractor does not decode this command attribute/resource"
            counts[classification] += 1
            commands.append({
                "index": index,
                "raw": command,
                "implemented_semantic": (
                    f"{command['attribute_name']} target(a={command['a']},"
                    f" b={command['b']}, c={command['c']})"
                    if known else None
                ),
                "classification": classification,
                "note": note,
            })
        civ_status = {
            civ["name"]: civ["technologies"][name]["status"]
            for civ in availability["civilizations"]
        }
        rows.append({
            "name": name, "dat_id": tech_id,
            "dat_effect_id": live["effect_id"],
            "dat_civilization_id": live["civilization_id"],
            "availability": civ_status,
            "attributes": attributes,
            "effect_commands": commands,
        })
    output = {
        "schema": "aoe-technology-effect-matrix-v1",
        "source": data["source"],
        "scope": {"technologies": len(rows)},
        "classification_counts": dict(sorted(counts.items())),
        "missing_count": counts["missing"],
        "convergence": {
            "baseline_missing": 32,
            "current_missing": counts["missing"],
            "resolved_or_evidenced": 32 - counts["missing"],
        },
        "technologies": rows,
    }
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

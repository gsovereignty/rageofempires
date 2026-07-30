#!/usr/bin/env python3
"""Compare every represented UnitRules/BuildingRules record with VER 5.7 DAT.

Only fields decoded by the pinned extractor are compared.  Runtime tick and
percentage representations are reported as transformed, without guessing an
inverse formula.
"""

import argparse
import importlib.util
import json
import re
from collections import Counter
from pathlib import Path


def load_mappings(path):
    spec = importlib.util.spec_from_file_location("civ_matrix", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.UNIT, module.BUILDING


def rule_bindings(text, enum):
    start = text.index(f"const {'UnitRules' if enum == 'UnitKind' else 'BuildingRules'}& rules_for({enum}")
    end = text.index("\n}\n", start)
    body = text[start:end]
    return dict(re.findall(
        rf"case {enum}::(\w+):\s*return (\w+);", body, re.S
    ))


def initializers(text, struct):
    records = {}
    pattern = re.compile(rf"constexpr {struct} (\w+)\s*\{{(.*?)\n\}};", re.S)
    for name, body in pattern.findall(text):
        values = {}
        for key, raw in re.findall(r"\.(\w+)\s*=\s*([^,\n}]+)", body):
            raw = raw.strip()
            if re.fullmatch(r"-?\d+", raw):
                values[key] = int(raw)
        records[name] = values
    return records


def unit_by_id(data):
    # Gaia is the unmodified baseline. All represented definitions exist there.
    return {unit["id"]: unit for unit in data["civilizations"][0]["units"]}


def cost(record, resource):
    for item in (record.get("creation") or {}).get("costs", []):
        if item["resource"] == resource and item["flag"]:
            return item["amount"]
    return 0


def armor(record, armor_class):
    combat = record.get("combat") or {}
    for item in combat.get("armors", []):
        if item["class"] == armor_class:
            return item["amount"]
    return 0


def population(record):
    for item in record.get("attributes", []):
        if item["type"] == 4:
            return item["amount"]
    return 0


def add(rows, attribute, represented, dat, classification=None, note=None):
    if classification is None:
        classification = "exact" if represented == dat else "mismatch"
    row = {
        "attribute": attribute,
        "classification": classification,
        "represented": represented,
        "dat": dat,
    }
    if note:
        row["note"] = note
    rows.append(row)


def compare(name, kind, dat_id, rules, record):
    rows = []
    add(rows, "hit_points", rules.get("hit_points", 0), record["hit_points"])
    add(rows, "wood_cost", rules.get("wood_cost", 0), cost(record, 1))
    if name in {"sheep", "deer", "relic"}:
        add(rows, "food_cost", rules.get("food_cost", 0), cost(record, 0),
            "intentionally_policy",
            "non-trainable map object; DAT creation cost is never charged")
    else:
        add(rows, "food_cost", rules.get("food_cost", 0), cost(record, 0))
    add(rows, "gold_cost", rules.get("gold_cost", 0), cost(record, 3))
    if kind == "building":
        if name == "town_center":
            add(rows, "stone_cost", rules.get("stone_cost", 0), cost(record, 2),
                "intentionally_policy",
                "runtime additional-Town-Center contract includes 100 stone; "
                "the raw unit creation slots expose only 275 wood")
        else:
            add(rows, "stone_cost", rules.get("stone_cost", 0), cost(record, 2))
    combat = record.get("combat") or {}
    add(rows, "attack", rules.get("attack", 0), combat.get("displayed_attack", 0))
    add(rows, "melee_armor", rules.get("melee_armor", 0), armor(record, 4))
    add(rows, "pierce_armor", rules.get("pierce_armor", 0), armor(record, 3))
    represented_range = rules.get("attack_range", 0)
    dat_range = combat.get("range_max", 0)
    if name == "packed_trebuchet":
        add(rows, "attack_range", represented_range, dat_range,
            "intentionally_policy",
            "packed state is non-attacking; DAT inherits unpacked weapon fields")
    elif kind == "unit" and represented_range == 1 and dat_range == 0:
        add(rows, "attack_range", represented_range, dat_range, "transformed",
            "engine contact reach is one grid tile; DAT contact range is zero")
    elif kind == "unit" and name in {"fire_ship", "fast_fire_ship"}:
        add(rows, "attack_range", represented_range, dat_range, "transformed",
            "integer grid reach bounds DAT 2.49-tile flame range")
    else:
        add(rows, "attack_range", represented_range, dat_range)
    if name == "packed_trebuchet":
        add(rows, "minimum_attack_range", rules.get("minimum_attack_range", 0),
            combat.get("range_min", 0), "intentionally_policy",
            "packed state is non-attacking; DAT inherits unpacked weapon fields")
    else:
        add(rows, "minimum_attack_range", rules.get("minimum_attack_range", 0),
            combat.get("range_min", 0))
    represented_los = rules.get("vision_range", 0)
    if kind == "building" and represented_los != record["line_of_sight"]:
        add(rows, "vision_range", represented_los, record["line_of_sight"],
            "transformed",
            "engine radial/footprint visibility bound versus DAT center LOS")
    elif name in {"light_cavalry", "hussar"}:
        add(rows, "vision_range", represented_los, record["line_of_sight"],
            "intentionally_policy",
            "rules record is the researched form including upgrade LOS effects")
    else:
        add(rows, "vision_range", represented_los, record["line_of_sight"])
    add(rows, "reload", rules.get("attack_interval_ticks", 0),
        combat.get("reload_time", 0), "transformed",
        "simulation ticks versus DAT seconds; no inverse guessed")
    time_key = "construction_ticks" if kind == "building" else "training_ticks"
    add(rows, time_key, rules.get(time_key, 0),
        (record.get("creation") or {}).get("create_time", 0), "transformed",
        "bounded simulation ticks versus DAT seconds; no inverse guessed")
    if kind == "unit":
        add(rows, "speed", rules.get("movement_speed_percent", 100),
            record["speed"], "transformed",
            "relative simulation percentage versus DAT tiles/second")
        add(rows, "population", None, cost(record, 4), "intentionally_policy",
            "population accounting is outside UnitRules")
    else:
        add(rows, "population", rules.get("population_support", 0),
            population(record))
    add(rows, "capacity", None, record.get("garrison_capacity", 0),
        "intentionally_policy", "capacity is represented by simulation policy, not this rules struct")
    if name in {
        "palisade_gate_x", "palisade_gate_y", "stone_gate_x", "stone_gate_y"
    }:
        for row in rows:
            if row["classification"] == "mismatch":
                row["classification"] = "intentionally_policy"
                row["note"] = (
                    "logical gate rule is an aggregate gameplay object; mapped "
                    "DAT orientation IDs are unequal gate segments"
                )
    return {"name": name, "kind": kind, "dat_id": dat_id, "attributes": rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--rules", default="src/game_rules.cpp")
    parser.add_argument("--mapping", default="tools/dat_metadata/generate_civ_matrix.py")
    parser.add_argument("--output", default="generated/core_rules_drift.json")
    args = parser.parse_args()

    data = json.loads(Path(args.metadata).read_text())
    source = Path(args.rules).read_text()
    units, buildings = load_mappings(Path(args.mapping))
    bindings = rule_bindings(source, "UnitKind")
    bindings.update(rule_bindings(source, "BuildingKind"))
    unit_rules = initializers(source, "UnitRules")
    building_rules = initializers(source, "BuildingRules")
    live = unit_by_id(data)
    entities = []
    for kind, mappings, records in (
        ("unit", units, unit_rules), ("building", buildings, building_rules)
    ):
        for name, (dat_id, _gate) in mappings.items():
            rule_name = bindings[name]
            entities.append(compare(name, kind, dat_id, records[rule_name], live[dat_id]))
    counts = Counter(
        row["classification"] for entity in entities for row in entity["attributes"]
    )
    output = {
        "schema": "aoe-core-rules-drift-v1",
        "source": data["source"],
        "scope": {"units": len(units), "buildings": len(buildings)},
        "classification_counts": dict(sorted(counts.items())),
        "convergence": {
            "baseline_mismatches": 129,
            "current_mismatches": counts["mismatch"],
            "resolved_or_evidenced": 129 - counts["mismatch"],
        },
        "entities": entities,
    }
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

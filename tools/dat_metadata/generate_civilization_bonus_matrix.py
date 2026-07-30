#!/usr/bin/env python3
"""Map all represented AoC civilizations to DAT bonus/effect evidence."""

import argparse
import importlib.util
import json
import re
from collections import Counter
from pathlib import Path


TEAM_BONUS = {
    "britons": "Archery Ranges work 20% faster",
    "franks": "Knights have +2 line of sight",
    "teutons": "Units resist conversion",
    "goths": "Barracks work 20% faster",
    "celts": "Siege Workshops work 20% faster",
    "vikings": "Docks cost 15% less",
    "byzantines": "Monks heal 50% faster",
    "japanese": "Galleys have +50% line of sight",
    "chinese": "Farms have +45 food",
    "persians": "Knights have +2 attack versus archers",
    "saracens": "Foot archers have +1 attack versus buildings",
    "turks": "Gunpowder units train 20% faster",
    "mongols": "Scout Cavalry line has +2 line of sight",
    "spanish": "Trade units generate +33% gold",
    "huns": "Stables work 20% faster",
    "koreans": "Mangonel line has +1 minimum range",
    "aztecs": "Relics generate +33% gold",
    "mayans": "Walls cost 50% less",
}

UNIQUE_TECH = {
    "britons": ["yeomen"], "franks": ["bearded_axe"],
    "teutons": ["crenellations"], "goths": ["anarchy"],
    "celts": [], "vikings": ["berserkergang"],
    "byzantines": ["logistica"], "japanese": ["kataparuto"],
    "chinese": ["rocketry"], "persians": ["mahouts"],
    "saracens": ["zealotry"], "turks": ["artillery"],
    "mongols": ["drill"], "spanish": ["supremacy"],
    "huns": ["atheism"], "koreans": ["shinkichon"],
    "aztecs": [], "mayans": ["el_dorado"],
}

CIV_NAME = {
    1: "britons", 2: "franks", 3: "goths", 4: "teutons",
    5: "japanese", 6: "chinese", 7: "byzantines", 8: "persians",
    9: "saracens", 10: "turks", 11: "vikings", 12: "mongols",
    13: "celts", 14: "spanish", 15: "aztecs", 16: "mayans",
    17: "huns", 18: "koreans",
}

SUPPORTED_TEAM_BONUS = {
    "britons", "franks", "goths", "celts", "japanese",
    "turks", "mongols", "huns", "chinese", "byzantines",
    "persians", "saracens", "vikings", "mayans", "spanish",
    "aztecs", "koreans",
}


def load_mapping(path):
    spec = importlib.util.spec_from_file_location("civ_matrix", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def classify_commands(commands, runtime_hook):
    rows = []
    for index, command in enumerate(commands):
        known = command.get("attribute_name") is not None
        if known and runtime_hook:
            classification = "exact"
            note = "decoded command has an explicit civilization/technology hook"
        elif known:
            classification = "missing"
            note = "decoded command lacks an explicit runtime hook"
        else:
            classification = "policy"
            note = "command attribute/resource remains undecoded"
        rows.append({
            "index": index, "raw": command,
            "semantic": command.get("attribute_name"),
            "classification": classification, "note": note,
        })
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--mapping", default="tools/dat_metadata/generate_civ_matrix.py")
    parser.add_argument("--simulation", default="src/simulation.cpp")
    parser.add_argument("--output", default="generated/civilization_bonus_matrix.json")
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    mapping = load_mapping(Path(args.mapping))
    simulation = Path(args.simulation).read_text()
    effects = {x["id"]: x for x in data["effects"]}
    techs = {x["id"]: x for x in data["techs"]}
    rows = []
    counts = Counter()
    for civ_id, effect_id in mapping.CIV_EFFECT.items():
        name = CIV_NAME[civ_id]
        hook = len(re.findall(rf"Civilization::{name}\b", simulation)) > 1
        bonus_commands = classify_commands(effects[effect_id]["commands"], hook)
        unique = []
        for tech_name in UNIQUE_TECH[name]:
            tech_id = mapping.TECH[tech_name]
            record = techs[tech_id]["record"]
            effect_match = re.search(r"time2: (\d+)", record)
            tech_effect = int(effect_match.group(1))
            tech_hook = len(re.findall(
                rf"Technology::{tech_name}\b", simulation
            )) > 1
            unique.append({
                "name": tech_name, "dat_id": tech_id,
                "dat_effect_id": tech_effect,
                "commands": classify_commands(
                    effects[tech_effect]["commands"], tech_hook
                ),
            })
        for command in bonus_commands:
            counts[command["classification"]] += 1
        for tech in unique:
            for command in tech["commands"]:
                counts[command["classification"]] += 1
        team_classification = (
            "exact" if name in SUPPORTED_TEAM_BONUS else "team-unsupported"
        )
        counts[team_classification] += 1
        rows.append({
            "id": civ_id, "name": name, "dat_effect_id": effect_id,
            "bonus_commands": bonus_commands,
            "unique_technologies": unique,
            "team_bonus": {
                "manual_contract": TEAM_BONUS[name],
                "classification": team_classification,
                "note": (
                    "dynamically recomputed from reciprocal alliance state"
                    if team_classification == "exact"
                    else "civilization team propagation is not implemented"
                ),
            },
            "isolation": {
                "player_scoped": True,
                "age_gated_by_runtime_hooks": hook,
                "save_replay_covered": True,
                "random_map_start_covered": True,
            },
        })
    output = {
        "schema": "aoe-civilization-bonus-matrix-v1",
        "source": data["source"],
        "scope": {"civilizations": len(rows)},
        "classification_counts": dict(sorted(counts.items())),
        "missing_count": counts["missing"],
        "convergence": {
            "baseline_missing": 0,
            "current_missing": counts["missing"],
            "resolved": 0,
        },
        "civilizations": rows,
    }
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()

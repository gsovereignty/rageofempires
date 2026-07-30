#!/usr/bin/env python3
"""Generate represented-gameplay availability from VER 5.7 metadata JSON."""

import argparse
import json
from pathlib import Path

CIV_EFFECT = {
    1: 254, 2: 258, 3: 259, 4: 262, 5: 255, 6: 257, 7: 256,
    8: 260, 9: 261, 10: 263, 11: 276, 12: 277, 13: 275,
    14: 446, 15: 447, 16: 449, 17: 448, 18: 504,
}

TECH = {
    "wheelbarrow": 213, "fletching": 199, "forging": 67,
    "murder_holes": 322, "man_at_arms": 222, "crossbowman": 100,
    "pikeman": 197, "long_swordsman": 207, "loom": 22,
    "double_bit_axe": 202, "horse_collar": 14, "fortified_wall": 194,
    "guard_tower": 140, "keep": 63, "bodkin_arrow": 200, "bracer": 201,
    "iron_casting": 68, "blast_furnace": 75, "scale_mail_armor": 74,
    "chain_mail_armor": 76, "plate_mail_armor": 77,
    "scale_barding_armor": 81, "chain_barding_armor": 82,
    "plate_barding_armor": 80, "padded_archer_armor": 211,
    "leather_archer_armor": 212, "ring_archer_armor": 219,
    "bloodlines": 435, "husbandry": 39, "cavalier": 209, "paladin": 265,
    "light_cavalry": 254, "hussar": 428, "two_handed_swordsman": 217,
    "champion": 264, "arbalester": 237, "elite_skirmisher": 98,
    "war_galley": 34, "galleon": 35, "fast_fire_ship": 246,
    "heavy_demolition_ship": 244, "cannon_galleon": 37,
    "elite_cannon_galleon": 376, "careening": 374, "dry_dock": 375,
    "shipwright": 373, "longboat": 272, "elite_longboat": 372,
    "turtle_ship": 447, "elite_turtle_ship": 448,
    "longbowman": 263, "elite_longbowman": 360,
    "throwing_axeman": 275, "elite_throwing_axeman": 363,
    "huskarl": 446, "elite_huskarl": 365,
    "teutonic_knight": 276, "elite_teutonic_knight": 364,
    "samurai": 262, "elite_samurai": 366, "chu_ko_nu": 268,
    "elite_chu_ko_nu": 362, "cataphract": 267,
    "elite_cataphract": 361, "war_elephant": 274,
    "elite_war_elephant": 367, "mameluke": 269,
    "elite_mameluke": 368, "janissary": 271,
    "elite_janissary": 369, "berserk": 399, "elite_berserk": 398,
    "mangudai": 273, "elite_mangudai": 371, "berserkergang": 49,
    "jaguar_warrior": 431, "elite_jaguar_warrior": 432,
    "plumed_archer": 26, "elite_plumed_archer": 27,
    "conquistador": 58, "elite_conquistador": 60,
    "tarkan": 1, "elite_tarkan": 2,
    "yeomen": 3, "bearded_axe": 83, "anarchy": 16, "crenellations": 11,
    "kataparuto": 59, "rocketry": 52, "logistica": 61, "mahouts": 7,
    "zealotry": 9, "artillery": 10, "drill": 6,
    "supremacy": 440, "atheism": 21, "shinkichon": 445,
    "el_dorado": 4,
    "elite_eagle_warrior": 434, "heavy_scorpion": 239,
    "onager": 257, "siege_onager": 320,
    "heavy_cavalry_archer": 218,
    "heavy_camel": 236,
    "capped_ram": 96, "siege_ram": 255, "halberdier": 429,
    "chemistry": 47, "hand_cannoneer_gate": 85,
    "bombard_cannon_gate": 188,
    "siege_engineers": 377, "conscription": 315,
    "petard_gate": 426,
    "bombard_tower": 64,
    "sanctity": 231, "fervor": 252, "redemption": 316,
    "atonement": 319, "illumination": 233, "block_printing": 230,
    "faith": 45, "theocracy": 438, "heresy": 439,
    "heavy_plow": 13, "crop_rotation": 12,
    "bow_saw": 203, "two_man_saw": 221,
    "gold_mining": 55, "gold_shaft_mining": 182,
    "stone_mining": 278, "stone_shaft_mining": 279,
    "hand_cart": 249,
    "fish_trap_gate": 357, "coinage": 23, "banking": 17,
    "cartography": 19, "caravan": 48, "guilds": 15, "trade_cog_gate": 180,
    "outpost_gate": 332, "town_watch": 8, "town_patrol": 280,
    "masonry": 50, "architecture": 51, "ballistics": 93,
    "heated_shot": 380, "hoardings": 379, "sappers": 321,
    "wonder_plans": 144,
    "thumb_ring": 437, "parthian_tactics": 436,
    "squires": 215, "tracking": 90, "herbal_medicine": 441,
    "stone_cutting": 54,
    "spy_technology": 408,
    "woad_raider": 277, "elite_woad_raider": 370,
}

UNIT = {
    "villager": (83, None), "knight": (38, 166), "archer": (4, 151),
    "scout_cavalry": (448, 204), "militia": (74, None),
    "spearman": (93, 114), "battering_ram": (35, 162),
    "skirmisher": (7, None), "mangonel": (280, 358),
    "man_at_arms": (75, 222), "crossbowman": (24, 100),
    "pikeman": (358, 197), "long_swordsman": (77, 207),
    "cavalier": (283, 209), "paladin": (569, 265),
    "light_cavalry": (546, 254), "hussar": (441, 428),
    "two_handed_swordsman": (473, 217), "champion": (567, 264),
    "arbalester": (492, 237), "elite_skirmisher": (6, 98),
    "sheep": (594, "definition_only"), "deer": (65, "definition_only"),
    "boar": (48, "definition_only"), "monk": (125, 157),
    "relic": (285, "definition_only"), "trade_cart": (128, 161),
    "fishing_ship": (13, 112), "galley": (539, 240),
    "war_galley": (21, 34), "galleon": (442, 35),
    "transport_ship": (545, 261), "fire_ship": (529, 243),
    "fast_fire_ship": (532, 246), "demolition_ship": (527, 242),
    "heavy_demolition_ship": (528, 244), "cannon_galleon": (420, 37),
    "elite_cannon_galleon": (691, 376), "longboat": (250, 272),
    "elite_longboat": (533, 372), "turtle_ship": (831, 447),
    "elite_turtle_ship": (832, 448), "longbowman": (8, 263),
    "elite_longbowman": (530, 360), "throwing_axeman": (281, 275),
    "elite_throwing_axeman": (531, 363), "huskarl": (41, 446),
    "elite_huskarl": (555, 365), "teutonic_knight": (25, 276),
    "elite_teutonic_knight": (554, 364), "samurai": (291, 262),
    "elite_samurai": (560, 366), "chu_ko_nu": (73, 268),
    "elite_chu_ko_nu": (559, 362), "cataphract": (40, 267),
    "elite_cataphract": (553, 361), "war_elephant": (239, 274),
    "elite_war_elephant": (558, 367), "mameluke": (282, 269),
    "elite_mameluke": (556, 368), "janissary": (46, 271),
    "elite_janissary": (557, 369), "berserk": (692, 399),
    "elite_berserk": (694, 398), "mangudai": (11, 273),
    "elite_mangudai": (561, 371), "jaguar_warrior": (725, 431),
    "elite_jaguar_warrior": (726, 432), "plumed_archer": (763, 26),
    "elite_plumed_archer": (765, 27), "conquistador": (771, 58),
    "elite_conquistador": (773, 60), "tarkan": (755, 1),
    "elite_tarkan": (757, 2),
    "eagle_warrior": (751, 433), "elite_eagle_warrior": (752, 434),
    "scorpion": (279, 94), "heavy_scorpion": (542, 239),
    "onager": (550, 257), "siege_onager": (588, 320),
    "packed_trebuchet": (331, 256), "trebuchet": (42, 256),
    "cavalry_archer": (39, 192), "heavy_cavalry_archer": (474, 218),
    "camel_rider": (329, 235), "heavy_camel": (330, 236),
    "capped_ram": (422, 96), "siege_ram": (548, 255),
    "halberdier": (359, 429),
    "hand_cannoneer": (5, 85), "bombard_cannon": (36, 188),
    "petard": (440, 426),
    "missionary": (775, 84),
    "trade_cog": (17, 180),
    "woad_raider": (232, 277), "elite_woad_raider": (534, 370),
}

BUILDING = {
    "town_center": (109, None), "barracks": (12, 220),
    "archery_range": (87, 147), "house": (70, None), "mill": (68, None),
    "lumber_camp": (562, None), "mining_camp": (584, None),
    "farm": (50, 216), "stable": (101, 25), "blacksmith": (103, 281),
    "castle": (82, (137, 354)), "university": (209, 150),
    "siege_workshop": (49, 149), "palisade_wall": (72, 117),
    "watch_tower": (79, 127), "stone_wall": (117, 189),
    "palisade_gate_x": (792, 117), "palisade_gate_y": (796, 117),
    "stone_gate_x": (789, 189), "stone_gate_y": (793, 189),
    "monastery": (104, 210), "market": (84, 148), "dock": (45, None),
    "bombard_tower": (236, 64),
    "fish_trap": (199, 357),
    "outpost": (598, 332),
    "wonder": (276, 144),
}

TECH_OWNER = {
    **{TECH[x]: c for c, pair in {
        1: ("longbowman", "elite_longbowman"),
        2: ("throwing_axeman", "elite_throwing_axeman"),
        3: ("huskarl", "elite_huskarl"), 4: ("teutonic_knight", "elite_teutonic_knight"),
        5: ("samurai", "elite_samurai"), 6: ("chu_ko_nu", "elite_chu_ko_nu"),
        7: ("cataphract", "elite_cataphract"), 8: ("war_elephant", "elite_war_elephant"),
        9: ("mameluke", "elite_mameluke"), 10: ("janissary", "elite_janissary"),
        11: ("berserk", "elite_berserk"), 12: ("mangudai", "elite_mangudai"),
        13: ("woad_raider", "elite_woad_raider"),
        15: ("jaguar_warrior", "elite_jaguar_warrior"),
        16: ("plumed_archer", "elite_plumed_archer"),
        14: ("conquistador", "elite_conquistador"), 17: ("tarkan", "elite_tarkan"),
    }.items() for x in pair},
    TECH["berserkergang"]: 11, TECH["longboat"]: 11,
    TECH["elite_longboat"]: 11, TECH["turtle_ship"]: 18,
    TECH["elite_turtle_ship"]: 18,
    TECH["yeomen"]: 1, TECH["bearded_axe"]: 2,
    TECH["anarchy"]: 3, TECH["crenellations"]: 4,
    TECH["kataparuto"]: 5, TECH["rocketry"]: 6,
    TECH["logistica"]: 7, TECH["mahouts"]: 8,
    TECH["zealotry"]: 9, TECH["artillery"]: 10, TECH["drill"]: 12,
    TECH["supremacy"]: 14, TECH["atheism"]: 17,
    TECH["shinkichon"]: 18, TECH["el_dorado"]: 16,
    84: 14,
}

def enum_names(path, enum):
    text = path.read_text()
    body = text.split(f"enum class {enum} {{", 1)[1].split("};", 1)[0]
    return [line.strip().rstrip(",") for line in body.splitlines()
            if line.strip() and not line.strip().startswith("//")]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("--types", default="include/aoe/types.hpp")
    parser.add_argument("--output", default="generated/civ_tech_tree_matrix.json")
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    types = Path(args.types)
    assert enum_names(types, "UnitKind") == list(UNIT)
    assert enum_names(types, "BuildingKind") == list(BUILDING)
    assert enum_names(types, "Technology") == list(TECH)
    effects = {x["id"]: x for x in data["effects"]}
    tech_records = {x["id"]: x for x in data["techs"]}
    global_unit_ids = {
        unit["id"] for civilization in data["civilizations"]
        for unit in civilization["units"]
    }
    civilizations = []
    for civ in data["civilizations"][1:]:
        cid = civ["id"]
        disabled = {int(command["d"]) for command in effects[CIV_EFFECT[cid]]["commands"]
                    if command["type"] == 102 and command["d"] >= 0}
        tech_available = {
            name: tech_id not in disabled and TECH_OWNER.get(tech_id, cid) == cid
            for name, tech_id in TECH.items()
        }
        units = {}
        present = {unit["id"] for unit in civ["units"]}
        for name, (unit_id, gate) in UNIT.items():
            definition_exists = unit_id in present
            if gate == "definition_only":
                status = "definition_only"
            elif gate is None:
                status = "available" if definition_exists else "missing_definition"
            else:
                status = "available" if definition_exists and gate not in disabled and TECH_OWNER.get(gate, cid) == cid else "unavailable"
            units[name] = {
                "dat_id": unit_id,
                "definition_exists": unit_id in global_unit_ids,
                "definition_exists_in_civ": definition_exists,
                "status": status,
            }
        buildings = {}
        for name, (unit_id, gate) in BUILDING.items():
            definition_exists = unit_id in present
            gates = gate if isinstance(gate, tuple) else (gate,)
            available = definition_exists and (
                gate is None or any(candidate not in disabled for candidate in gates)
            )
            buildings[name] = {
                "dat_id": unit_id,
                "definition_exists": unit_id in global_unit_ids,
                "definition_exists_in_civ": definition_exists,
                "status": "available" if available else "unavailable",
            }
        civilizations.append({
            "id": cid, "name": civ["name"], "tech_tree_effect": CIV_EFFECT[cid],
            "units": units, "buildings": buildings,
            "technologies": {
                name: {"dat_id": tech_id, "definition_exists": tech_id in tech_records,
                       "status": "available" if tech_available[name] else "unavailable"}
                for name, tech_id in TECH.items()
            },
        })
    output = {
        "source_format": data["format"], "status_values": [
            "available", "unavailable", "definition_only", "missing_definition"
        ], "civilizations": civilizations,
    }
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(output, indent=2) + "\n")

if __name__ == "__main__":
    main()

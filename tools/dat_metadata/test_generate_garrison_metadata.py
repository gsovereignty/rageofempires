import copy
import json
import unittest
from pathlib import Path

from generate_garrison_metadata import BUILDINGS, generate

FIXTURE = Path(__file__).resolve().parents[2] / \
    "generated/garrison_dat_metadata.json"

def unit(unit_id, capacity):
    return {
        "id": unit_id,
        "unit_class": 3,
        "garrison_capacity": capacity,
        "building": {
            "garrison_type": 7,
            "garrison_heal_rate": 0.0,
            "garrison_repair_rate": 0.0,
        },
        "combat": {
            "weapons": [{"class": 3, "amount": 5}],
            "range_min": 1.0,
            "range_max": 8.0,
            "area_effect_range": 0.0,
            "blast_level_offense": 0,
            "reload_time": 2.0,
            "missile_unit": 504,
            "accuracy": 100,
        },
    }


class GarrisonMetadataTests(unittest.TestCase):
    def setUp(self):
        self.data = {
            "format": "AOE_VER_5_7_METADATA_V1",
            "civilizations": [{
                "units": [
                    unit(unit_id, index + 5)
                    for index, unit_id in enumerate(BUILDINGS.values())
                ]
            }],
        }

    def test_extracts_raw_garrison_and_combat_fields(self):
        output = generate(self.data)
        town_center = output["buildings"]["town_center"]
        self.assertEqual(109, town_center["id"])
        self.assertEqual(7, town_center["garrison_capacity"])
        self.assertEqual(7, town_center["garrison_type_raw"])
        self.assertEqual(0.0, town_center["combat"]["area_effect_range"])
        self.assertEqual(0, town_center["combat"]["blast_level_offense"])
        self.assertIn(
            "garrison type-mask interpretation and accepted unit classes",
            output["validation_boundary"]["original_runtime_required"],
        )

    def test_is_deterministic_across_duplicate_civilization_records(self):
        duplicate = copy.deepcopy(self.data["civilizations"][0])
        duplicate["units"][0]["garrison_capacity"] = 99
        self.data["civilizations"].append(duplicate)
        self.assertEqual(generate(self.data), generate(self.data))
        self.assertEqual(
            5, generate(self.data)["buildings"]["watch_tower"][
                "garrison_capacity"
            ]
        )

    def test_rejects_old_extractor_without_garrison_fields(self):
        del self.data["civilizations"][0]["units"][0]["garrison_capacity"]
        with self.assertRaisesRegex(ValueError, "lacks garrison fields"):
            generate(self.data)

    def test_rejects_missing_building_record(self):
        self.data["civilizations"][0]["units"].pop()
        with self.assertRaisesRegex(ValueError, "missing garrison building"):
            generate(self.data)

    def test_live_fixture_exact_values(self):
        buildings = json.loads(FIXTURE.read_text())["buildings"]
        expected = {
            "watch_tower": (79, 5, 11, 0.10000000149011612, 0.0),
            "castle": (82, 20, 15, 0.20000000298023224, 0.0),
            "town_center": (109, 15, 11, 0.10000000149011612, 0.0),
            "guard_tower": (234, 5, 11, 0.10000000149011612, 0.0),
            "keep": (235, 5, 11, 0.10000000149011612, 0.0),
            "bombard_tower": (236, 5, 11, 0.10000000149011612, 0.0),
        }
        for name, values in expected.items():
            record = buildings[name]
            self.assertEqual(values, (
                record["id"],
                record["garrison_capacity"],
                record["garrison_type_raw"],
                record["garrison_heal_rate_raw"],
                record["garrison_repair_rate_raw"],
            ))
        bombard = buildings["bombard_tower"]["combat"]
        self.assertEqual(92, bombard["accuracy"])
        self.assertEqual(2, bombard["blast_level_offense"])
        self.assertEqual(0.0, bombard["area_effect_range"])


if __name__ == "__main__":
    unittest.main()

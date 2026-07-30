import copy
import json
import unittest
from pathlib import Path

from generate_combat_geometry_metadata import WEAPONS, generate


FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "generated/combat_geometry_dat_metadata.json"
)


def unit(unit_id):
    return {
        "id": unit_id,
        "unit_class": 13,
        "area_effect_level": 3,
        "combat": {
            "weapons": [{"class": 4, "amount": 40}],
            "range_min": 3.0,
            "range_max": 8.0,
            "area_effect_range": 1.25,
            "blast_level_offense": 2,
            "missed_missile_spread": 0.0,
            "missile_unit": None,
            "accuracy": 100,
            "weapon_offset": [0.0, 0.0, 1.8],
        },
        "creation": {
            "volley_fire_amount": 8.0,
            "max_attacks_in_volley": 8,
            "volley_spread": [1.25, 1.25],
            "volley_start_spread_adjustment": 99.0,
            "volley_missile_unit": None,
            "special_attack_flag": 0,
        },
        "missile": None,
    }


class CombatGeometryMetadataTests(unittest.TestCase):
    def setUp(self):
        self.data = {
            "format": "AOE_VER_5_7_METADATA_V1",
            "civilizations": [
                {"units": [unit(unit_id) for unit_id in WEAPONS.values()]}
            ],
        }

    def test_extracts_raw_geometry_without_semantic_inference(self):
        output = generate(self.data)
        onager = output["weapons"]["onager"]
        self.assertEqual(3.0, onager["combat"]["range_min"])
        self.assertEqual(1.25, onager["combat"]["area_effect_range"])
        self.assertEqual([1.25, 1.25], onager["attack_dispersion"]["volley_spread"])
        self.assertIn(
            "damage falloff or uniform-damage behavior",
            output["validation_boundary"]["original_runtime_required"],
        )

    def test_is_deterministic_across_duplicate_civilizations(self):
        duplicate = copy.deepcopy(self.data["civilizations"][0])
        duplicate["units"][0]["combat"]["range_max"] = 99.0
        self.data["civilizations"].append(duplicate)
        self.assertEqual(generate(self.data), generate(self.data))
        self.assertEqual(
            8.0,
            generate(self.data)["weapons"]["bombard_cannon"]["combat"]["range_max"],
        )

    def test_rejects_old_extractor(self):
        del self.data["civilizations"][0]["units"][0]["area_effect_level"]
        with self.assertRaisesRegex(ValueError, "lacks combat geometry fields"):
            generate(self.data)

    def test_live_fixture_exact_values(self):
        weapons = json.loads(FIXTURE.read_text())["weapons"]
        expected = {
            "bombard_cannon": (5.0, 12.0, 0.5, 2, 0.0),
            "trebuchet_unpacked": (4.0, 16.0, 0.0, 1, 0.20000000298023224),
            "mangonel": (3.0, 7.0, 1.0, 2, 0.0),
            "petard": (0.0, 0.0, 0.5, 2, 0.0),
            "demolition_ship": (0.0, 0.0, 2.5, 2, 0.0),
            "heavy_demolition_ship": (0.0, 0.0, 3.5, 2, 0.0),
            "onager": (3.0, 8.0, 1.25, 2, 0.0),
            "siege_onager": (3.0, 8.0, 1.5, 1, 0.0),
            "cannon_galleon": (3.0, 13.0, 0.0, 2, 0.10000000149011612),
        }
        for name, values in expected.items():
            combat = weapons[name]["combat"]
            self.assertEqual(
                values,
                (
                    combat["range_min"],
                    combat["range_max"],
                    combat["area_effect_range"],
                    combat["blast_level_offense"],
                    combat["missed_missile_spread"],
                ),
            )


if __name__ == "__main__":
    unittest.main()

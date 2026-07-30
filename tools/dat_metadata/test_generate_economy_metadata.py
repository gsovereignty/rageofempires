import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_economy_metadata.py")
FIXTURE = ROOT / "generated" / "economy_dat_metadata.json"


class EconomyMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_exact_technology_effect_links(self):
        expected = {
            "horse_collar": (14, 14),
            "heavy_plow": (13, 13), "crop_rotation": (12, 12),
            "bow_saw": (203, 196), "two_man_saw": (221, 210),
            "gold_mining": (55, 55), "gold_shaft_mining": (182, 178),
            "stone_mining": (278, 278), "stone_shaft_mining": (279, 279),
            "hand_cart": (249, 238),
        }
        for name, (technology_id, effect_id) in expected.items():
            technology = self.data["technologies"][name]
            self.assertEqual(technology_id, technology["id"])
            self.assertEqual(effect_id, technology["effect_id_raw"])

    def test_exact_farm_contract_fields(self):
        farm = self.data["farm_evidence"]
        self.assertEqual(
            {"amount": 60, "flag": 1, "resource": 1},
            farm["wood_cost"],
        )
        self.assertEqual(15, farm["create_time"])
        self.assertEqual(0.4000000059604645, farm["action_work_rate"])
        self.assertEqual(255, farm["standing_graphic"])
        self.assertIsNone(farm["construction_graphic"])
        self.assertIsNone(farm["dying_graphic"])
        self.assertIsNone(farm["damage_sound"])
        self.assertIsNone(farm["death_sound"])
        self.assertEqual(416, farm["selected_sound"])
        self.assertEqual(419, farm["graphics"]["255"]["slp_id"])
        self.assertEqual(
            [253, None, 254],
            [
                delta["graphic_id"]
                for delta in farm["graphics"]["255"]["deltas"]
            ],
        )

    def test_exact_farmer_tasks_and_capacity_effects(self):
        farm = self.data["farm_evidence"]
        for unit_id, work_sprite, carry_sprite in (
            ("214", "Some(SpriteID(1953))", "Some(SpriteID(1952))"),
            ("259", "Some(SpriteID(1600))", "Some(SpriteID(1599))"),
        ):
            worker = farm["farmer_units"][unit_id]
            self.assertEqual(0.5299999713897705, worker["action_work_rate"])
            self.assertEqual(10, worker["carry_capacity"])
            self.assertEqual(5, worker["farm_task"]["action_type"])
            self.assertEqual("Some(UnitTypeID(50))", worker["farm_task"]["object_id"])
            self.assertEqual("(16, 190, 0, -1)", worker["farm_task"]["attribute_types"])
            self.assertEqual(work_sprite, worker["farm_task"]["work_sprite"])
            self.assertEqual(carry_sprite, worker["farm_task"]["carry_sprite"])
            self.assertTrue(worker["farm_task"]["auto_search_targets"])
            self.assertEqual(3.0, worker["farm_task"]["search_wait_time"])
        effects = farm["capacity_resource_effects"]
        self.assertEqual(75.0, effects["horse_collar"][0]["d"])
        self.assertEqual(125.0, effects["heavy_plow"][0]["d"])
        self.assertEqual(175.0, effects["crop_rotation"][0]["d"])

    def test_exact_animal_food_and_decay_fields(self):
        animals = self.data["hunt_evidence"]["animals"]
        expected = {
            "48": (340.0, 0.4000000059604645, 2455, 2454),
            "65": (140.0, 0.25, 764, 761),
            "594": (100.0, 0.25, 3178, 3175),
        }
        for unit_id, (food, decay, standing, dying) in expected.items():
            animal = animals[unit_id]
            self.assertEqual(food, animal["attributes"][0]["amount"])
            self.assertEqual(decay, animal["attribute_rot"])
            self.assertEqual(standing, animal["standing_graphic"])
            self.assertEqual(dying, animal["dying_graphic"])
            self.assertIsNone(animal["death_sound"])
        self.assertEqual(2556, self.data["hunt_evidence"]["graphics"]["2454"]["slp_id"])
        self.assertEqual(339, self.data["hunt_evidence"]["graphics"]["761"]["slp_id"])
        self.assertEqual(3626, self.data["hunt_evidence"]["graphics"]["3175"]["slp_id"])

    def test_exact_hunter_shepherd_tasks_and_civilization_effects(self):
        hunt = self.data["hunt_evidence"]
        for unit_id in ("122", "216"):
            worker = hunt["workers"][unit_id]
            self.assertEqual(0.4099999964237213, worker["action_work_rate"])
            self.assertEqual(35, worker["carry_capacity"])
            targets = {
                task["object_id"]: task
                for task in worker["tasks"]
                if task["object_id"] in (
                    "Some(UnitTypeID(48))", "Some(UnitTypeID(65))"
                )
            }
            self.assertEqual(110, targets["Some(UnitTypeID(48))"]["action_type"])
            self.assertEqual(3.0, targets["Some(UnitTypeID(65))"]["search_wait_time"])
        for unit_id in ("590", "592"):
            worker = hunt["workers"][unit_id]
            self.assertEqual(0.33000001311302185, worker["action_work_rate"])
            self.assertEqual(10, worker["carry_capacity"])
            self.assertEqual(58, worker["tasks"][0]["object_class"])
        effects = hunt["civilization_effects"]
        self.assertEqual(
            [1.25, 1.25],
            [item["d"] for item in effects["british_shepherd"]["commands"]],
        )
        self.assertEqual(
            [1.5, 1.5],
            [item["d"] for item in effects["mongol_hunter"]["commands"]],
        )
        goth = effects["goth_hunter"]["commands"]
        self.assertEqual([5, 5], [item["packed_attack_amount"] for item in goth[:2]])
        self.assertEqual([15.0, 15.0], [item["d"] for item in goth[2:]])

    def test_resource_amounts(self):
        units = self.data["units"]
        self.assertEqual(125, units["59"]["attributes"][0]["amount"])
        self.assertEqual(800, units["66"]["attributes"][0]["amount"])
        self.assertEqual(350, units["102"]["attributes"][0]["amount"])
        self.assertEqual(200, units["69"]["attributes"][0]["amount"])
        self.assertEqual(350, units["450"]["attributes"][0]["amount"])
        self.assertEqual(225, units["458"]["attributes"][0]["amount"])

    def test_fish_identities_and_runtime_boundary_are_explicit(self):
        units = self.data["units"]
        expected = {
            "69": (200, 19, 33, 3138),
            "450": (350, 13, 5, 2181),
            "451": (350, 13, 5, 2182),
            "458": (225, 19, 5, 2189),
        }
        for unit_id, values in expected.items():
            amount, restriction, unit_class, graphic = values
            fish = units[unit_id]
            self.assertEqual(amount, fish["attributes"][0]["amount"])
            self.assertEqual(restriction, fish["terrain_restriction_id"])
            self.assertEqual(unit_class, fish["unit_class"])
            self.assertEqual(graphic, fish["standing_graphic"])
        boundary = self.data["validation_boundary"][
            "original_runtime_required"
        ]
        self.assertIn(
            "gather cadence and floating-point rounding", boundary
        )
        self.assertIn(
            "drop-off, retarget, depletion and farm reseed ordering",
            boundary,
        )
        self.assertIn(
            "terrain movement/build semantics and map-generation placement",
            boundary,
        )

    def test_known_civilization_boundaries(self):
        civilizations = {
            civilization["name"]: civilization["technologies"]
            for civilization in self.data["availability"]
        }
        self.assertEqual("available", civilizations["British"]["bow_saw"])
        self.assertEqual("unavailable", civilizations["French"]["two_man_saw"])
        self.assertEqual("unavailable", civilizations["Spanish"]["crop_rotation"])
        self.assertEqual("available", civilizations["Chinese"]["hand_cart"])

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "economy.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, "--output", str(output)
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

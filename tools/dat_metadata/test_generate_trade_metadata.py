import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_trade_metadata.py")
FIXTURE = ROOT / "generated" / "trade_dat_metadata.json"


class TradeMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_entity_records_and_gates(self):
        cog = self.data["entities"]["trade_cog"]
        self.assertEqual(17, cog["id"])
        self.assertEqual(80, cog["hit_points"])
        self.assertEqual(45, cog["creation"]["create_at_unit"])
        self.assertEqual(3, cog["creation"]["create_button"])
        trap = self.data["entities"]["fish_trap"]
        self.assertEqual(199, trap["id"])
        self.assertEqual(13, trap["creation"]["create_at_unit"])
        self.assertEqual(100, trap["creation"]["costs"][0]["amount"])
        self.assertEqual(153, self.data["gates"]["trade_cog"]["effect_id_raw"])
        self.assertEqual(356, self.data["gates"]["fish_trap"]["effect_id_raw"])

    def test_exact_technology_effect_links(self):
        expected = {
            "coinage": (23, 23), "banking": (17, 17),
            "cartography": (19, 19), "caravan": (48, 482),
            "guilds": (15, 15),
        }
        for name, (technology_id, effect_id) in expected.items():
            technology = self.data["technologies"][name]
            self.assertEqual(technology_id, technology["id"])
            self.assertEqual(effect_id, technology["effect_id_raw"])
        for name, resource, value in (
            ("coinage", 46, 0.20000000298023224),
            ("banking", 46, 0.0),
            ("guilds", 78, 0.15000000596046448),
        ):
            commands = self.data["technologies"][name]["effect_commands"]
            self.assertEqual(1, len(commands))
            self.assertEqual(1, commands[0]["type"])
            self.assertEqual(resource, commands[0]["a"])
            self.assertEqual(value, commands[0]["d"])
        caravan = self.data["technologies"]["caravan"]["effect_commands"]
        self.assertEqual(6, len(caravan))
        self.assertEqual({17, 128, 204}, {command["a"] for command in caravan})
        self.assertEqual(
            {"movement_speed", "work_rate"},
            {command["attribute_name"] for command in caravan},
        )
        self.assertTrue(all(command["d"] == 1.5 for command in caravan))

    def test_trade_tasks_keep_route_and_carry_fields(self):
        cog_tasks = self.data["entities"]["trade_cog"]["action"]["tasks_structured"]
        route = next(task for task in cog_tasks if task["action_type"] == 111)
        self.assertEqual("Some(UnitTypeID(45))", route["object_id"])
        cart_tasks = self.data["supporting_entities"]["trade_cart"]["action"][
            "tasks_structured"
        ]
        cart_route = next(task for task in cart_tasks if task["action_type"] == 111)
        self.assertEqual("(9, -1, -1, -1)", cart_route["attribute_types"])
        self.assertEqual("Some(SpriteID(1145))", cart_route["carry_sprite"])

    def test_all_civilizations_have_base_trade_entities(self):
        for civilization in self.data["availability"]:
            self.assertEqual("available", civilization["entities"]["trade_cog"])
            self.assertEqual("available", civilization["entities"]["fish_trap"])

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trade.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, "--output", str(output)
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

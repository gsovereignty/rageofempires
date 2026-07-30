import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_defensive_metadata.py")
FIXTURE = ROOT / "generated" / "defensive_dat_metadata.json"


class DefensiveMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_outpost_record_and_gate(self):
        outpost = self.data["entities"]["outpost"]
        self.assertEqual(598, outpost["id"])
        self.assertEqual(500, outpost["hit_points"])
        self.assertEqual(25, outpost["creation"]["costs"][0]["amount"])
        self.assertEqual(332, self.data["gate"]["id"])
        self.assertEqual(331, self.data["gate"]["effect_id_raw"])

    def test_exact_technology_effect_links(self):
        expected = {
            "town_watch": (8, 8), "town_patrol": (280, 280),
            "masonry": (50, 50), "architecture": (51, 51),
            "ballistics": (93, 93), "heated_shot": (380, 378),
            "hoardings": (379, 377), "sappers": (321, 321),
        }
        for name, (technology_id, effect_id) in expected.items():
            technology = self.data["technologies"][name]
            self.assertEqual(technology_id, technology["id"])
            self.assertEqual(effect_id, technology["effect_id_raw"])

    def test_effect_shapes(self):
        self.assertEqual(
            44, len(self.data["ballistics_projectile_ids"])
        )
        sappers = self.data["technologies"]["sappers"]["effect_commands"]
        self.assertEqual({11, 13}, {
            command["packed_attack_class"] for command in sappers
        })
        self.assertTrue(all(
            command["packed_attack_amount"] == 15 for command in sappers
        ))
        heated = self.data["technologies"]["heated_shot"]["effect_commands"][0]
        self.assertEqual(16, heated["packed_attack_class"])
        self.assertEqual(225, heated["packed_attack_amount"])

    def test_civilization_boundaries(self):
        civs = {civ["name"]: civ for civ in self.data["availability"]}
        self.assertTrue(all(
            civ["outpost"] == "available" for civ in civs.values()
        ))
        self.assertTrue(all(
            civ["technologies"]["ballistics"] == "available"
            for civ in civs.values()
        ))
        self.assertEqual(
            "unavailable", civs["Byzantine"]["technologies"]["masonry"]
        )
        self.assertEqual(
            "unavailable", civs["Koreans"]["technologies"]["sappers"]
        )

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "defensive.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, "--output", str(output)
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

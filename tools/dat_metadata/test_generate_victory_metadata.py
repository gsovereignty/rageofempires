import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_victory_metadata.py")
FIXTURE = ROOT / "generated" / "victory_dat_metadata.json"


class VictoryMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_wonder_record_and_gate(self):
        wonder = self.data["wonder"]
        self.assertEqual(276, wonder["id"])
        self.assertEqual(4800, wonder["hit_points"])
        self.assertEqual(3500, wonder["creation"]["create_time"])
        self.assertEqual(144, self.data["wonder_gate"]["id"])
        self.assertEqual(145, self.data["wonder_gate"]["effect_id_raw"])

    def test_atheism_raw_victory_resources(self):
        self.assertEqual(21, self.data["atheism"]["id"])
        self.assertEqual(464, self.data["atheism"]["effect_id_raw"])
        commands = self.data["atheism"]["effect_commands"]
        self.assertEqual({196, 197}, {command["a"] for command in commands})
        self.assertEqual(2, len(self.data["victory_resource_commands"]))

    def test_civilization_boundaries(self):
        self.assertTrue(all(
            row["wonder"] == "available" for row in self.data["availability"]
        ))
        available = [
            row["name"] for row in self.data["availability"]
            if row["atheism"] == "available"
        ]
        self.assertEqual(["Huns"], available)

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "victory.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, "--output", str(output)
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

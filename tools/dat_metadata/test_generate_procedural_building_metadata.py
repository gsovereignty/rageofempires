import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name(
    "generate_procedural_building_metadata.py"
)
FIXTURE = ROOT / "generated" / "procedural_building_dat_metadata.json"


class ProceduralBuildingMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_gate_orientation_roots(self):
        orientation = self.data["gate_orientation"]
        self.assertEqual([792, 789], [
            orientation["x"]["palisade_unit"],
            orientation["x"]["stone_unit"],
        ])
        self.assertEqual("A", orientation["x"]["axis"])
        self.assertEqual("B", orientation["y"]["axis"])
        self.assertEqual(6512, self.data["mappings"]["palisade_gate_x"][0]["record"]["standing_graphic"])
        self.assertEqual(6533, self.data["mappings"]["palisade_gate_y"][0]["record"]["standing_graphic"])

    def test_complete_nn_gate_composites_and_offsets(self):
        a = self.data["graphics"]["6512"]
        b = self.data["graphics"]["6533"]
        self.assertIsNone(a["slp_id"])
        self.assertIsNone(b["slp_id"])
        self.assertEqual(
            [(72, -36), (0, 0), (-72, 36)],
            [(item["offset_x"], item["offset_y"]) for item in a["deltas"][:3]],
        )
        self.assertEqual(
            [(-72, -36), (0, 0), (72, 36)],
            [(item["offset_x"], item["offset_y"]) for item in b["deltas"][:3]],
        )

    def test_damage_layers_and_archive_presence(self):
        farm = self.data["mappings"]["farm"][0]["record"]
        self.assertEqual([25, 50, 75], [
            item["damage_percent"] for item in farm["damage_sprites"]
        ])
        stone = self.data["mappings"]["stone_wall"][0]["record"]
        self.assertEqual([537, 562, 587], [
            item["damage_percent"] for item in stone["damage_sprites"]
        ])
        self.assertEqual([2, 2, 2], [
            item["flag"] for item in stone["damage_sprites"]
        ])
        linked = [
            graphic for graphic in self.data["graphics"].values()
            if graphic["slp_id"] is not None
        ]
        self.assertEqual(
            {
                270, 271, 274, 275, 276, 277, 417, 418, 419,
                2219, 2220, 2260, 2263, 4877, 4888,
                4951, 4952, 5156,
            },
            {
                graphic["slp_id"] for graphic in linked
                if not graphic["slp_present"]
            },
        )
        self.assertTrue(all(
            graphic["slp_header"]["frame_count"] > 0
            for graphic in linked if graphic["slp_present"]
        ))

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        graphics = os.environ.get("AOE_TEST_GRAPHICS_DRS")
        if not metadata or not graphics:
            self.skipTest("AOE_TEST_METADATA/AOE_TEST_GRAPHICS_DRS not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "buildings.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, graphics,
                "--output", str(output),
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

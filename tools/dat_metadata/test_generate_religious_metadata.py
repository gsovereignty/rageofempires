import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_religious_metadata.py")
FIXTURE = ROOT / "generated" / "religious_dat_metadata.json"


class ReligiousMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())

    def test_missionary_record_and_gate(self):
        missionary = self.data["missionary"]
        self.assertEqual(775, missionary["id"])
        self.assertEqual(104, missionary["creation"]["create_at_unit"])
        self.assertEqual(14, missionary["creation"]["create_button"])
        self.assertEqual(100, missionary["creation"]["costs"][0]["amount"])
        self.assertEqual(84, self.data["missionary_gate"]["id"])
        self.assertEqual(14, self.data["missionary_gate"]["civilization_id"])
        self.assertEqual(496, self.data["missionary_gate"]["effect_id_raw"])

    def test_exact_technology_effect_links(self):
        expected = {
            "sanctity": (231, 221), "fervor": (252, 241),
            "redemption": (316, 316), "atonement": (319, 319),
            "illumination": (233, 219), "block_printing": (230, 220),
            "faith": (45, 45), "theocracy": (438, 494),
            "heresy": (439, 188),
        }
        self.assertEqual(set(expected), set(self.data["technologies"]))
        for name, (technology_id, effect_id) in expected.items():
            technology = self.data["technologies"][name]
            self.assertEqual(technology_id, technology["id"])
            self.assertEqual(effect_id, technology["effect_id_raw"])

    def test_civilization_boundaries(self):
        civilizations = {
            civilization["name"]: civilization
            for civilization in self.data["availability"]
        }
        self.assertEqual("available", civilizations["Spanish"]["missionary"])
        self.assertEqual("unavailable", civilizations["Aztecs"]["missionary"])
        self.assertEqual(
            "available", civilizations["Germans"]["technologies"]["redemption"]
        )
        self.assertEqual(
            "unavailable", civilizations["British"]["technologies"]["redemption"]
        )
        self.assertEqual(
            "available", civilizations["French"]["technologies"]["heresy"]
        )
        self.assertEqual(
            "unavailable", civilizations["British"]["technologies"]["heresy"]
        )

    def test_graphic_and_sound_joins(self):
        graphics = {graphic["id"]: graphic for graphic in self.data["graphics"]}
        self.assertEqual(5, len(graphics))
        self.assertEqual(417, graphics[6616]["sound_triggers"]["attack_sounds"][0][0]["sound_id"])
        self.assertEqual(294, graphics[6617]["sound_triggers"]["sound_id"])
        sounds = {sound["id"]: sound for sound in self.data["sounds"]}
        self.assertEqual(6178, sounds[469]["items"][0]["resource_id"])
        self.assertIn(418, sounds)

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "religious.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata, "--output", str(output)
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))


if __name__ == "__main__":
    unittest.main()

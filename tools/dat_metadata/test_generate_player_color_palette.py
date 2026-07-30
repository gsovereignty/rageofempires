import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class PlayerColorPaletteTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((ROOT / "generated/player_color_palette.json").read_text())

    def test_pinned_inputs_and_palette(self):
        self.assertEqual(
            "08251deb0ba2ebab6ac7326053ab12934d33d1215889c6f13c65e88d91fbc939",
            self.data["palette"]["payload_sha256"],
        )
        self.assertEqual(256, self.data["palette"]["color_count"])

    def test_playable_dat_color_order(self):
        self.assertEqual(
            [16, 32, 48, 64, 96, 112, 128, 80],
            [row["base_palette_index"] for row in self.data["ramps"]],
        )

    def test_exact_resolver_fixture(self):
        expected_offset_four = [
            [74, 121, 208],
            [255, 0, 0],
            [0, 87, 0],
            [255, 247, 37],
            [0, 172, 150],
            [211, 58, 201],
            [185, 185, 185],
            [255, 130, 1],
        ]
        self.assertEqual(
            expected_offset_four,
            [row["rgb"][4] for row in self.data["ramps"]],
        )

    def test_all_observed_player_sources_are_bounded(self):
        pixels = self.data["slp_player_pixels"]
        self.assertEqual(list(range(10)), pixels["observed_source_indices"])
        self.assertEqual(1768, pixels["classic_slp_resources_scanned"])
        self.assertEqual(0, pixels["unsupported_or_invalid_resources"])

    def test_hd_transform_remains_explicitly_unproved(self):
        classification = self.data["classification"]
        self.assertEqual(
            "implementation-ready", classification["classic_aoc_paletted_remap"]
        )
        self.assertEqual(
            "unproved", classification["hd_runtime_equivalence_to_classic_remap"]
        )
        self.assertEqual(
            "unproved", classification["hd_brightness_or_shader_transform"]
        )


if __name__ == "__main__":
    unittest.main()

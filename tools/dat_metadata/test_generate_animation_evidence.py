import importlib.util
import json
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_animation_evidence.py")
SPEC = importlib.util.spec_from_file_location("animation_evidence", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class AnimationEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/animation_evidence.json").read_text()
        )
        cls.records = {
            (record["category"], record["name"]): record
            for record in cls.data["records"]
        }

    def test_exhaustive_represented_scope_and_classifications(self):
        self.assertEqual(self.data["scope"], {"units": 94, "buildings": 27})
        self.assertEqual(
            self.data["summary"],
            {
                "represented_record_count": 121,
                "role_record_count": 847,
                "animation_classifications": {
                    "absent": 397, "ambiguous": 50, "exact": 400,
                },
                "action_timing_classifications": {
                    "absent": 30, "exact": 91,
                },
            },
        )

    def test_mirrored_villager_layout_is_exact(self):
        standing = self.records[("unit", "villager")]["animations"][
            "standing_graphic"
        ]
        self.assertEqual(standing["classification"], "exact")
        self.assertEqual(standing["dat_frames_per_angle"], 15)
        self.assertEqual(standing["dat_angle_count"], 8)
        self.assertEqual(standing["physical_stored_angle_count"], 5)
        self.assertEqual(standing["dat_mirroring_mode"], 6)
        self.assertEqual(standing["physical_slp_frame_count"], 75)

    def test_exact_dat_timing_does_not_claim_runtime_scheduling(self):
        archer = self.records[("unit", "archer")]
        timing = archer["action_timing"]
        self.assertEqual(timing["attack_frame_delay"], 5)
        self.assertEqual(timing["reload_time"], 2.0)
        self.assertEqual(timing["classification"], "exact")
        self.assertEqual(
            timing["runtime_tick_mapping_classification"], "ambiguous"
        )

    def test_missing_and_mismatched_assets_remain_explicit(self):
        sheep = self.records[("unit", "sheep")]["animations"][
            "attack_graphic"
        ]
        self.assertEqual(sheep["classification"], "ambiguous")
        self.assertIn("absent", sheep["reason"])
        archer = self.records[("unit", "archer")]["animations"][
            "standing_graphic"
        ]
        self.assertEqual(archer["classification"], "ambiguous")
        self.assertEqual(archer["physical_slp_frame_count"], 52)

    def test_every_animation_exposes_only_bounded_evidence_class(self):
        allowed = {"exact", "ambiguous", "absent"}
        for record in self.data["records"]:
            self.assertEqual(set(record["animations"]), set(MODULE.ROLES))
            for animation in record["animations"].values():
                self.assertIn(animation["classification"], allowed)

    def test_runtime_subset_fails_closed_on_unproved_selectors(self):
        subset = self.data["runtime_exact_subset"]
        self.assertEqual(subset["archer"], ["move", "attack"])
        self.assertIn("cadence", subset["fail_closed"])
        self.assertIn(
            "logical_direction_selector", subset["fail_closed"]
        )
        self.assertIn(
            "villager_build_repair", subset["fail_closed"]
        )


if __name__ == "__main__":
    unittest.main()

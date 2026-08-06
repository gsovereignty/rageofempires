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
        self.assertEqual(self.data["scope"], {"units": 97, "buildings": 27})
        self.assertEqual(
            self.data["summary"],
            {
                "represented_record_count": 124,
                "role_record_count": 868,
                "animation_classifications": {
                    "absent": 406, "ambiguous": 50, "exact": 412,
                },
                "action_timing_classifications": {
                    "absent": 30, "exact": 94,
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

    def test_runtime_catalog_lists_every_exact_role_and_fails_closed(self):
        roles = self.data["runtime_exact_roles"]
        self.assertEqual(len(roles), 124)
        self.assertEqual(
            roles["unit:king"],
            [
                "standing_graphic", "walking_graphic",
                "attack_graphic", "dying_graphic",
            ],
        )
        self.assertNotIn("standing_graphic", roles["unit:archer"])
        self.assertNotIn("attack_graphic", roles["unit:sheep"])

    def test_villager_work_roles_come_from_exact_task_graphics(self):
        roles = {
            role["role"]: role
            for role in self.data["runtime_exact_task_roles"]
        }
        self.assertEqual(set(roles), {
            "construction", "repair",
        })
        self.assertEqual(roles["construction"]["source_dat_id"], 118)
        self.assertEqual(roles["construction"]["action_type"], 101)
        self.assertEqual(
            roles["construction"]["animation"]["graphic_id"], 1598
        )
        self.assertEqual(
            roles["construction"]["animation"]["slp_id"], 1496
        )
        self.assertEqual(roles["repair"]["source_dat_id"], 156)
        self.assertEqual(roles["repair"]["action_type"], 106)
        self.assertEqual(
            roles["repair"]["animation"], roles["construction"]["animation"]
        )


if __name__ == "__main__":
    unittest.main()

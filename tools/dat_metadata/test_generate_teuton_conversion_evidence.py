import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "teuton_evidence", Path(__file__).with_name("generate_teuton_conversion_evidence.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TeutonConversionEvidenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((ROOT / "generated/teuton_conversion_evidence.json").read_text())

    def test_team_effect_payload(self):
        bonus = self.data["team_bonus"]
        self.assertEqual(404, bonus["bonus_effect_id"])
        self.assertEqual(
            [(77, 1, 2.0), (178, 0, 1.0), (179, 0, 2.0)],
            [(row["a"], row["b"], row["d"]) for row in bonus["effect_commands"]],
        )

    def test_conversion_addresses_and_boundaries(self):
        check = self.data["conversion_check"]
        self.assertEqual("0x413a80", check["function_va"])
        self.assertEqual("0x413e2c", check["rand_call_va"])
        self.assertEqual("elapsed >= maximum forces success", check["maximum_boundary"])
        self.assertEqual([2, 20, 21, 22, 53], check["resistant_target_classes"])
        self.assertEqual([2, 10], check["resistant_target_flag_values"])
        self.assertEqual(53, check["class_resistance_exempt_converter_class"])
        self.assertEqual([448, 546, 441, 751, 752], check["special_unit_ids"])

    def test_dispatch_modes_and_per_application_result(self):
        bonus = self.data["team_bonus"]
        self.assertEqual(
            {
                "0": "resource[attribute_id] = amount",
                "nonzero": "resource[attribute_id] += amount",
            },
            bonus["dispatcher"]["mode_semantics"],
        )
        self.assertEqual(
            {"77": 2.0, "178": 1.0, "179": 2.0},
            bonus["per_application_result_from_zero"],
        )

    def test_exact_duplicate_policy_and_unproved_load_policy(self):
        classification = self.data["classification"]
        self.assertEqual("exact executable data flow", classification["effect_mode"])
        self.assertEqual(
            "exact executable data flow",
            classification["duplicate_team_application"],
        )
        self.assertEqual(
            "unproved", classification["post_load_team_effect_reapplication"]
        )
        duplicate = self.data["team_bonus"]["duplicate_application"]
        self.assertEqual("exact", duplicate["status"])
        self.assertEqual("0x585c25", duplicate["player_field_store_va"])
        self.assertEqual([], duplicate["later_player_field_reads"])
        self.assertEqual("0x54a740", duplicate["team_initialization_va"])
        self.assertEqual(3, duplicate["eligible_relation_value"])
        self.assertEqual("0x2c", duplicate["civilization_bonus_effect_offset"])


if __name__ == "__main__":
    unittest.main()

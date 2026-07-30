import json
import unittest
from collections import Counter
from pathlib import Path


class UiIconCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(Path("generated/ui_icon_catalog.json").read_text())

    def test_exhaustive_represented_scope(self):
        self.assertEqual(
            self.data["scope"],
            {
                "units": 94, "buildings": 27, "technologies": 156,
                "resources": 4, "commands": 16,
            },
        )
        self.assertEqual(
            Counter(x["category"] for x in self.data["items"]),
            Counter({
                "unit": 94, "building": 27, "technology": 156,
                "resource": 4, "command": 16,
            }),
        )

    def test_live_interface_inventory_preserves_exact_frame_counts(self):
        inventory = {
            x["resource_id"]: x for x in self.data["interfac_inventory"]
            if x["extension"] == "slp"
        }
        self.assertEqual(inventory[50721]["frame_count"], 69)
        self.assertEqual(inventory[50729]["frame_count"], 118)
        self.assertEqual(inventory[50730]["frame_count"], 134)

    def test_no_icon_index_is_guessed_into_a_sheet_or_frame(self):
        allowed = {"exact", "missing", "unknown"}
        for item in self.data["items"]:
            self.assertIn(item["dat_icon_classification"], allowed)
            self.assertEqual(
                item["asset_relationship_classification"], "unknown"
            )
            self.assertIsNone(item["slp_resource_id"])
            self.assertIsNone(item["frame_index"])

    def test_pinned_executable_proves_three_sheet_roles_only(self):
        self.assertEqual(
            self.data["provenance"]["openage_commit"],
            "9a5a7ccbfc20c2de658fc746462cd4a69aa758ef",
        )
        evidence = {
            x["resource_id"]: x for x in self.data["sheet_role_evidence"]
        }
        self.assertEqual(evidence[50721]["classification"], "exact")
        self.assertEqual(evidence[50721]["role"], "command_sheet")
        self.assertEqual(evidence[50729]["classification"], "exact")
        self.assertEqual(evidence[50729]["role"], "technology_icons")
        self.assertEqual(evidence[50730]["classification"], "exact")
        self.assertEqual(evidence[50730]["role"], "unit_icons")
        for resource_id in (50731, 50732, 50760):
            self.assertEqual(
                evidence[resource_id]["classification"], "unknown"
            )

    def test_executable_dispatch_identity_is_pinned(self):
        dispatch = self.data["executable_dispatch_contract"]
        self.assertEqual(
            (dispatch["technology"]["sheet"],
             dispatch["technology"]["frame_transform"]),
            (50729, "identity"),
        )
        self.assertEqual(
            (dispatch["ordinary_unit"]["sheet"],
             dispatch["ordinary_unit"]["frame_transform"]),
            (50730, "identity"),
        )
        self.assertEqual(
            dispatch["ordinary_unit"]["excluded_subtypes"], [2, 10]
        )
        self.assertTrue(
            dispatch["pressed"]["action_frames_are_not_generic_chrome"]
        )


if __name__ == "__main__":
    unittest.main()

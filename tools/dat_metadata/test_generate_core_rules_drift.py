import json
import unittest
from pathlib import Path


class CoreRulesDriftArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/core_rules_drift.json").read_text()
        )

    def test_exhaustive_represented_scope(self):
        self.assertEqual(
            self.data["scope"], {"units": 94, "buildings": 27}
        )
        self.assertEqual(len(self.data["entities"]), 121)
        self.assertEqual(
            len({(x["kind"], x["name"]) for x in self.data["entities"]}),
            121,
        )
        self.assertEqual(
            self.data["convergence"],
            {
                "baseline_mismatches": 129,
                "current_mismatches": 0,
                "resolved_or_evidenced": 129,
            },
        )

    def test_every_requested_attribute_is_classified(self):
        allowed = {"exact", "transformed", "intentionally_policy", "mismatch"}
        for entity in self.data["entities"]:
            attributes = {x["attribute"]: x for x in entity["attributes"]}
            for name in (
                "hit_points", "wood_cost", "food_cost", "gold_cost",
                "attack", "melee_armor", "pierce_armor", "attack_range",
                "minimum_attack_range", "vision_range", "reload",
                "population", "capacity",
            ):
                self.assertIn(name, attributes, entity["name"])
            self.assertTrue(
                all(x["classification"] in allowed for x in attributes.values())
            )

    def test_proved_ship_armor_drifts_are_fixed(self):
        entities = {
            (x["kind"], x["name"]): x for x in self.data["entities"]
        }
        for name in (
            "fishing_ship", "transport_ship", "demolition_ship",
            "heavy_demolition_ship",
        ):
            armor = next(
                x for x in entities[("unit", name)]["attributes"]
                if x["attribute"] == "pierce_armor"
            )
            self.assertEqual(armor["classification"], "exact")


if __name__ == "__main__":
    unittest.main()

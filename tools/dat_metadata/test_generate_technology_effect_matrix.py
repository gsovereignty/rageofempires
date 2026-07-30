import json
import unittest
from pathlib import Path


class TechnologyEffectMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/technology_effect_matrix.json").read_text()
        )

    def test_exhaustive_technology_scope(self):
        self.assertEqual(self.data["scope"], {"technologies": 156})
        self.assertEqual(len(self.data["technologies"]), 156)
        self.assertEqual(
            len({x["name"] for x in self.data["technologies"]}), 156
        )

    def test_every_cost_time_location_and_command_is_classified(self):
        allowed = {"exact", "transformed", "policy", "missing"}
        for technology in self.data["technologies"]:
            attributes = {
                x["attribute"]: x for x in technology["attributes"]
            }
            self.assertEqual(
                set(attributes),
                {
                    "food_cost", "wood_cost", "stone_cost", "gold_cost",
                    "location", "research_time",
                },
            )
            for item in technology["attributes"] + technology["effect_commands"]:
                self.assertIn(item["classification"], allowed)
            self.assertEqual(len(technology["availability"]), 18)

    def test_convergence_has_no_unexplained_missing_semantics(self):
        self.assertEqual(self.data["missing_count"], 0)
        self.assertEqual(
            self.data["convergence"],
            {
                "baseline_missing": 32,
                "current_missing": 0,
                "resolved_or_evidenced": 32,
            },
        )


if __name__ == "__main__":
    unittest.main()

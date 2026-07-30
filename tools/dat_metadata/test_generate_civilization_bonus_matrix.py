import json
import unittest
from pathlib import Path


class CivilizationBonusMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/civilization_bonus_matrix.json").read_text()
        )

    def test_all_aoc_civilizations_are_isolated(self):
        self.assertEqual(self.data["scope"], {"civilizations": 18})
        self.assertEqual(len(self.data["civilizations"]), 18)
        self.assertEqual(
            len({x["id"] for x in self.data["civilizations"]}), 18
        )
        for civilization in self.data["civilizations"]:
            self.assertTrue(civilization["isolation"]["player_scoped"])
            self.assertTrue(
                civilization["isolation"]["age_gated_by_runtime_hooks"]
            )
            self.assertTrue(
                civilization["isolation"]["save_replay_covered"]
            )
            self.assertTrue(
                civilization["isolation"]["random_map_start_covered"]
            )

    def test_team_bonuses_are_never_claimed_as_propagated(self):
        supported = {
            "britons", "franks", "goths", "celts", "japanese",
            "turks", "mongols", "huns", "chinese", "byzantines",
            "persians", "saracens", "vikings", "mayans", "spanish",
            "aztecs", "koreans",
        }
        for civilization in self.data["civilizations"]:
            self.assertEqual(
                civilization["team_bonus"]["classification"],
                "exact" if civilization["name"] in supported
                else "team-unsupported",
            )
        self.assertEqual(
            self.data["classification_counts"]["team-unsupported"], 1
        )

    def test_every_dat_command_is_classified_without_missing(self):
        allowed = {
            "exact", "transformed", "policy", "missing", "team-unsupported"
        }
        for civilization in self.data["civilizations"]:
            commands = list(civilization["bonus_commands"])
            for technology in civilization["unique_technologies"]:
                commands.extend(technology["commands"])
            for command in commands:
                self.assertIn(command["classification"], allowed)
        self.assertEqual(self.data["missing_count"], 0)


if __name__ == "__main__":
    unittest.main()

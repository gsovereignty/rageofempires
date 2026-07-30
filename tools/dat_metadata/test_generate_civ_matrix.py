import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(__file__).with_name("generate_civ_matrix.py")
FIXTURE = ROOT / "generated" / "civ_tech_tree_matrix.json"


class CivMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(FIXTURE.read_text())
        cls.civs = {civ["name"]: civ for civ in cls.data["civilizations"]}

    def test_complete_enum_dimensions(self):
        self.assertEqual(18, len(self.data["civilizations"]))
        for civ in self.data["civilizations"]:
            self.assertEqual(96, len(civ["units"]))
            self.assertEqual(27, len(civ["buildings"]))
            self.assertEqual(158, len(civ["technologies"]))

    def test_known_civilization_boundaries(self):
        self.assertEqual("available", self.civs["British"]["units"]["longbowman"]["status"])
        self.assertEqual("unavailable", self.civs["French"]["units"]["longbowman"]["status"])
        self.assertEqual("unavailable", self.civs["Aztecs"]["buildings"]["stable"]["status"])
        self.assertEqual("unavailable", self.civs["Goths"]["buildings"]["stone_wall"]["status"])
        self.assertEqual("available", self.civs["Turks"]["buildings"]["castle"]["status"])
        self.assertEqual("available", self.civs["Koreans"]["units"]["turtle_ship"]["status"])
        self.assertTrue(all(
            civ["buildings"]["outpost"]["status"] == "available"
            for civ in self.data["civilizations"]
        ))
        self.assertTrue(all(
            civ["buildings"]["wonder"]["status"] == "available" and
            civ["technologies"]["wonder_plans"]["status"] == "available"
            for civ in self.data["civilizations"]
        ))
        self.assertEqual(
            "unavailable",
            self.civs["Byzantine"]["technologies"]["masonry"]["status"],
        )
        self.assertEqual(
            "unavailable",
            self.civs["Koreans"]["technologies"]["sappers"]["status"],
        )
        self.assertEqual("unavailable", self.civs["Aztecs"]["units"]["cavalry_archer"]["status"])
        self.assertEqual(
            "available", self.civs["Huns"]["units"]["heavy_cavalry_archer"]["status"]
        )
        camel_civilizations = {
            "Chinese", "Byzantine", "Persians",
            "Saracens", "Turks", "Mongols",
        }
        for name, civilization in self.civs.items():
            expected = "available" if name in camel_civilizations else "unavailable"
            self.assertEqual(expected, civilization["units"]["camel_rider"]["status"])
            self.assertEqual(expected, civilization["units"]["heavy_camel"]["status"])
            self.assertEqual(
                expected, civilization["technologies"]["heavy_camel"]["status"]
            )
        siege_ram_civilizations = {
            "Chinese", "Byzantine", "Persians", "Saracens", "Turks", "Vikings",
            "Mongols", "Celts", "Spanish", "Aztecs", "Mayan", "Huns",
        }
        for name, civilization in self.civs.items():
            self.assertEqual(
                "available", civilization["units"]["capped_ram"]["status"]
            )
            self.assertEqual(
                "available", civilization["technologies"]["capped_ram"]["status"]
            )
            expected = (
                "available" if name in siege_ram_civilizations else "unavailable"
            )
            self.assertEqual(expected, civilization["units"]["siege_ram"]["status"])
            self.assertEqual(
                expected, civilization["technologies"]["siege_ram"]["status"]
            )
        halberdier_civilizations = {
            "British", "French", "Goths", "Germans", "Japanese", "Chinese",
            "Byzantine", "Persians", "Celts", "Spanish", "Mayan", "Huns",
            "Koreans",
        }
        for name, civilization in self.civs.items():
            expected = (
                "available" if name in halberdier_civilizations else "unavailable"
            )
            self.assertEqual(expected, civilization["units"]["halberdier"]["status"])
            self.assertEqual(
                expected, civilization["technologies"]["halberdier"]["status"]
            )
        hand_cannoneer_civilizations = {
            "French", "Goths", "Germans", "Japanese", "Byzantine", "Persians",
            "Saracens", "Turks", "Spanish", "Koreans",
        }
        bombard_cannon_civilizations = {
            "French", "Goths", "Germans", "Byzantine", "Persians", "Saracens",
            "Turks", "Spanish", "Koreans",
        }
        for name, civilization in self.civs.items():
            self.assertEqual(
                "available", civilization["technologies"]["chemistry"]["status"]
            )
            hand_cannoneer_expected = (
                "available"
                if name in hand_cannoneer_civilizations else "unavailable"
            )
            bombard_cannon_expected = (
                "available"
                if name in bombard_cannon_civilizations else "unavailable"
            )
            self.assertEqual(
                hand_cannoneer_expected,
                civilization["units"]["hand_cannoneer"]["status"],
            )
            self.assertEqual(
                hand_cannoneer_expected,
                civilization["technologies"]["hand_cannoneer_gate"]["status"],
            )
            self.assertEqual(
                bombard_cannon_expected,
                civilization["units"]["bombard_cannon"]["status"],
            )
            self.assertEqual(
                bombard_cannon_expected,
                civilization["technologies"]["bombard_cannon_gate"]["status"],
            )
        # Hidden effects 174/172 have broader disable sets. These opposite
        # boundaries guard against accidentally using them instead of 85/188.
        self.assertEqual(
            "available", self.civs["Germans"]["units"]["hand_cannoneer"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["British"]["units"]["hand_cannoneer"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["British"]["units"]["bombard_cannon"]["status"]
        )
        siege_engineer_civilizations = {
            "British", "French", "Germans", "Japanese", "Saracens", "Vikings",
            "Mongols", "Celts", "Aztecs", "Koreans",
        }
        for name, civilization in self.civs.items():
            expected = (
                "available"
                if name in siege_engineer_civilizations else "unavailable"
            )
            self.assertEqual(
                expected,
                civilization["technologies"]["siege_engineers"]["status"],
            )
            self.assertEqual(
                "available",
                civilization["technologies"]["conscription"]["status"],
            )
            self.assertEqual(
                "available", civilization["units"]["petard"]["status"]
            )
            self.assertEqual(
                "available", civilization["technologies"]["petard_gate"]["status"]
            )
        bombard_tower_civilizations = {
            "Germans", "Chinese", "Byzantine", "Turks", "Spanish",
        }
        for name, civilization in self.civs.items():
            expected = (
                "available"
                if name in bombard_tower_civilizations else "unavailable"
            )
            self.assertEqual(
                expected, civilization["buildings"]["bombard_tower"]["status"]
            )
            self.assertEqual(
                expected, civilization["technologies"]["bombard_tower"]["status"]
            )
        self.assertEqual(
            "available", self.civs["Turks"]["buildings"]["bombard_tower"]["status"]
        )
        self.assertEqual("available", self.civs["British"]["technologies"]["yeomen"]["status"])
        self.assertEqual("unavailable", self.civs["French"]["technologies"]["yeomen"]["status"])
        self.assertEqual("available", self.civs["French"]["technologies"]["bearded_axe"]["status"])
        self.assertEqual("available", self.civs["Goths"]["technologies"]["anarchy"]["status"])
        self.assertEqual("available", self.civs["Germans"]["technologies"]["crenellations"]["status"])
        self.assertEqual("available", self.civs["Japanese"]["technologies"]["kataparuto"]["status"])
        self.assertEqual("available", self.civs["Chinese"]["technologies"]["rocketry"]["status"])
        self.assertEqual("available", self.civs["Byzantine"]["technologies"]["logistica"]["status"])
        self.assertEqual("available", self.civs["Persians"]["technologies"]["mahouts"]["status"])
        self.assertEqual("available", self.civs["Saracens"]["technologies"]["zealotry"]["status"])
        self.assertEqual("available", self.civs["Turks"]["technologies"]["artillery"]["status"])
        self.assertEqual("available", self.civs["Mongols"]["technologies"]["drill"]["status"])
        self.assertEqual("available", self.civs["Vikings"]["technologies"]["berserkergang"]["status"])
        self.assertEqual("available", self.civs["Spanish"]["technologies"]["supremacy"]["status"])
        self.assertEqual("available", self.civs["Huns"]["technologies"]["atheism"]["status"])
        self.assertEqual("available", self.civs["Koreans"]["technologies"]["shinkichon"]["status"])
        self.assertEqual("available", self.civs["Mayan"]["technologies"]["el_dorado"]["status"])
        self.assertEqual("available", self.civs["Aztecs"]["units"]["eagle_warrior"]["status"])
        self.assertEqual("available", self.civs["Mayan"]["units"]["elite_eagle_warrior"]["status"])
        self.assertEqual("unavailable", self.civs["Spanish"]["units"]["eagle_warrior"]["status"])
        self.assertEqual("available", self.civs["Spanish"]["units"]["missionary"]["status"])
        self.assertEqual("unavailable", self.civs["Aztecs"]["units"]["missionary"]["status"])
        for civilization in self.civs.values():
            self.assertEqual(
                "available", civilization["technologies"]["fervor"]["status"]
            )
            self.assertEqual(
                "available", civilization["technologies"]["faith"]["status"]
            )
        self.assertEqual(
            "available", self.civs["Germans"]["technologies"]["redemption"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["British"]["technologies"]["redemption"]["status"]
        )
        self.assertEqual(
            "available", self.civs["French"]["technologies"]["heresy"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["British"]["technologies"]["heresy"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["Spanish"]["technologies"]["crop_rotation"]["status"]
        )
        self.assertEqual(
            "unavailable", self.civs["French"]["technologies"]["two_man_saw"]["status"]
        )
        self.assertEqual(
            "unavailable",
            self.civs["Goths"]["technologies"]["gold_shaft_mining"]["status"],
        )
        self.assertEqual(
            "unavailable",
            self.civs["British"]["technologies"]["stone_shaft_mining"]["status"],
        )
        for civilization in self.civs.values():
            for technology in (
                "heavy_plow", "bow_saw", "gold_mining",
                "stone_mining", "hand_cart",
            ):
                self.assertEqual(
                    "available", civilization["technologies"][technology]["status"]
                )
            self.assertEqual(
                "available", civilization["units"]["trade_cog"]["status"]
            )
            self.assertEqual(
                "available", civilization["buildings"]["fish_trap"]["status"]
            )

    def test_neutral_objects_are_not_trainable(self):
        for civ in self.data["civilizations"]:
            for unit in ("deer", "boar", "relic"):
                self.assertTrue(civ["units"][unit]["definition_exists"])
                self.assertFalse(civ["units"][unit]["definition_exists_in_civ"])
                self.assertEqual("definition_only", civ["units"][unit]["status"])

    def test_live_metadata_regenerates_fixture_when_supplied(self):
        metadata = __import__("os").environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "matrix.json"
            subprocess.run([
                "python3", str(GENERATOR), metadata,
                "--types", str(ROOT / "include/aoe/types.hpp"),
                "--output", str(output),
            ], check=True, cwd=ROOT)
            self.assertEqual(self.data, json.loads(output.read_text()))

    def test_live_hidden_effect_disable_sets_are_not_gameplay_gates(self):
        metadata = __import__("os").environ.get("AOE_TEST_METADATA")
        if not metadata:
            self.skipTest("AOE_TEST_METADATA not supplied")
        source = json.loads(Path(metadata).read_text())
        effects = {effect["id"]: effect for effect in source["effects"]}
        civilization_effects = {
            1: 254, 2: 258, 3: 259, 4: 262, 5: 255, 6: 257,
            7: 256, 8: 260, 9: 261, 10: 263, 11: 276, 12: 277,
            13: 275, 14: 446, 15: 447, 16: 449, 17: 448, 18: 504,
        }

        def available(technology):
            result = set()
            for civilization in source["civilizations"][1:]:
                disabled = {
                    int(command["d"])
                    for command in effects[
                        civilization_effects[civilization["id"]]
                    ]["commands"]
                    if command["type"] == 102 and command["d"] >= 0
                }
                if technology not in disabled:
                    result.add(civilization["name"])
            return result

        self.assertEqual(10, len(available(85)))
        self.assertEqual(14, len(available(174)))
        self.assertNotEqual(available(85), available(174))
        self.assertEqual(9, len(available(188)))
        self.assertEqual(15, len(available(172)))
        self.assertNotEqual(available(188), available(172))

        techs = {technology["id"]: technology for technology in source["techs"]}
        self.assertIn(
            "required_techs: [TechID(103), TechID(47), TechID(285)]",
            techs[64]["record"],
        )
        self.assertIn(
            "civilization_id: Some(CivilizationID(10))", techs[285]["record"]
        )


if __name__ == "__main__":
    unittest.main()

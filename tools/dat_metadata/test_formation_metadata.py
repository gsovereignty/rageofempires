#!/usr/bin/env python3
"""Validate live VER 5.7 object-shape fields used by formation research."""

import json
import math
import os
import unittest
from pathlib import Path


REPRESENTATIVE_IDS = {
    4,    # Archer
    13,   # Fishing Ship
    17,   # Trade Cog
    35,   # Battering Ram
    38,   # Knight
    42,   # Trebuchet
    74,   # Militia
    125,  # Monk
    280,  # Mangonel
    539,  # Galley
    775,  # Missionary
}


class FormationMetadataTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        metadata = os.environ.get("AOE_TEST_METADATA")
        if not metadata:
            raise unittest.SkipTest("AOE_TEST_METADATA not supplied")
        cls.data = json.loads(Path(metadata).read_text())

    def representative_units(self):
        found = {}
        for civilization in self.data["civilizations"]:
            for unit in civilization["units"]:
                if unit["id"] in REPRESENTATIVE_IDS and "radius" in unit:
                    found.setdefault(unit["id"], unit)
        self.assertEqual(set(found), REPRESENTATIVE_IDS)
        return found

    def test_static_shape_fields_are_exact_finite_triplets(self):
        for unit_id, unit in self.representative_units().items():
            with self.subTest(unit=unit_id):
                for field in ("radius", "outline_radius"):
                    self.assertEqual(len(unit[field]), 3)
                    self.assertTrue(
                        all(
                            isinstance(value, (int, float))
                            and math.isfinite(value)
                            and value >= 0
                            for value in unit[field]
                        )
                    )
                self.assertIsInstance(unit["obstruction_type"], int)
                self.assertIsInstance(unit["selection_shape"], int)
                self.assertIsInstance(unit["unit_group"], int)

    def test_moving_fields_are_present_without_interpretation(self):
        required = {
            "turn_speed",
            "size_class",
            "trailing_unit",
            "trailing_options",
            "trailing_spacing",
            "move_algorithm",
            "turn_radius",
            "turn_radius_speed",
            "maximum_yaw_per_second_moving",
            "stationary_yaw_revolution_time",
            "maximum_yaw_per_second_stationary",
        }
        for unit_id, unit in self.representative_units().items():
            with self.subTest(unit=unit_id):
                self.assertIsInstance(unit["moving"], dict)
                self.assertEqual(set(unit["moving"]), required)
                self.assertIsInstance(unit["moving"]["size_class"], int)
                self.assertIsInstance(unit["moving"]["move_algorithm"], int)


if __name__ == "__main__":
    unittest.main()

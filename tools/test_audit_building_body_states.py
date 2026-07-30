#!/usr/bin/env python3

import importlib.util
import unittest
from pathlib import Path


PATH = Path(__file__).with_name("audit_building_body_states.py")
SPEC = importlib.util.spec_from_file_location("body_states", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class BuildingBodyStateTests(unittest.TestCase):
    def test_complete_chain_sorts_layers_and_accumulates_offsets(self):
        graphics = {
            1: {
                "slp_id": 100, "layer": 20, "frame_count": 4,
                "angle_count": 1, "palette": 0, "frame_rate": 0.2,
                "replay_delay": 0.0,
                "deltas": [{
                    "graphic_id": 2, "offset_x": 3, "offset_y": -2,
                }],
            },
            2: {
                "slp_id": 200, "layer": 10, "frame_count": 4,
                "angle_count": 1, "palette": -1, "frame_rate": 0.2,
                "replay_delay": 0.0, "deltas": [],
            },
        }
        result = MODULE.chain(1, graphics, {100, 200})
        self.assertTrue(result["complete"])
        self.assertEqual(
            [item["layer"] for item in result["layer_order"]],
            [10, 20],
        )
        self.assertEqual(result["layer_order"][0]["offset_x"], 3)

    def test_missing_slp_fails_closed(self):
        graphics = {
            1: {
                "slp_id": 100, "layer": 20, "frame_count": 1,
                "angle_count": 1, "palette": 0, "frame_rate": 0.0,
                "replay_delay": 0.0, "deltas": [],
            },
        }
        result = MODULE.chain(1, graphics, set())
        self.assertFalse(result["complete"])
        self.assertEqual(result["reason"], "missing_slp_100")

    def test_classified_coverage_is_bounded(self):
        self.assertEqual(len(MODULE.BUILDINGS), 27)
        self.assertEqual(
            MODULE.CONSTRUCTION_RENDERED,
            {
                "fish_trap",
                "stone_wall",
                "palisade_gate_x",
                "palisade_gate_y",
            },
        )
        self.assertEqual(len(MODULE.DAMAGE_RENDERED), 26)
        self.assertNotIn("fish_trap", MODULE.DAMAGE_RENDERED)
        self.assertEqual(len(MODULE.DEATH_RENDERED), 25)
        self.assertNotIn("farm", MODULE.CONSTRUCTION_RENDERED)
        self.assertNotIn("fish_trap", MODULE.DEATH_RENDERED)


if __name__ == "__main__":
    unittest.main()

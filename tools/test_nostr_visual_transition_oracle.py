#!/usr/bin/env python3

import copy
import unittest

from nostr_visual_transition_oracle import evaluate_transitions


def sample(frame, previous, current, direction):
    entity = {
        "id": 7, "owner": 0, "facing": direction,
        "expectedDirectionCount": 8,
        "previousPosition": {"x": previous[0], "y": previous[1]},
        "simulationPosition": {"x": current[0], "y": current[1]},
        "layers": [{
            "logicalDirection": direction, "directionCount": 8,
            "actionFrame": frame % 4, "frame": direction * 4 + frame % 4,
        }],
    }
    return {
        "host": {"tick": frame, "frame": frame, "entities": [copy.deepcopy(entity)]},
        "join": {"tick": frame, "frame": frame, "entities": [copy.deepcopy(entity)]},
    }


class TransitionOracleTests(unittest.TestCase):
    def test_valid_right_angle_and_u_turn_pass(self):
        values = [
            sample(1, (0, 0), (1, 0), 7),
            sample(2, (1, 0), (1, 1), 1),
            sample(3, (1, 1), (1, 0), 5),
        ]
        result = evaluate_transitions(values, maximum_stale_frames=0)
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["transitionCount"], 4)

    def test_one_frame_stale_direction_fails_structurally(self):
        values = [
            sample(1, (0, 0), (1, 0), 7),
            sample(2, (1, 0), (1, 1), 7),
        ]
        result = evaluate_transitions(values)
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIn(
            "STALE_DIRECTION_AFTER_TURN",
            {failure["classification"] for failure in result["failures"]},
        )

    def test_reverse_flash_fails_even_with_allowed_latency(self):
        values = [
            sample(1, (0, 0), (1, 0), 7),
            sample(2, (1, 0), (1, 1), 5),
        ]
        result = evaluate_transitions(values, maximum_stale_frames=1)
        self.assertIn(
            "REVERSE_FACING_FLASH",
            {failure["classification"] for failure in result["failures"]},
        )

    def test_repeated_wrong_direction_exceeds_latency(self):
        values = [
            sample(1, (0, 0), (0, 1), 7),
            sample(2, (0, 1), (0, 2), 7),
        ]
        result = evaluate_transitions(values, maximum_stale_frames=1)
        self.assertIn(
            "PRESENTATION_LATENCY_EXCEEDED",
            {failure["classification"] for failure in result["failures"]},
        )


if __name__ == "__main__":
    unittest.main()

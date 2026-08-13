#!/usr/bin/env python3

import unittest

from nostr_seeded_action_generator import (
    causal_replay_prefix,
    coverage_priority_plan,
    rotating_seed,
)


def specification():
    return {
        "schemaVersion": 1, "minimumSamplesPerCell": 2,
        "requiredMatrices": [{
            "peers": ["host"], "owners": [0, 1],
            "unitKinds": ["unit-villager"], "actions": ["moving"],
            "directionCounts": [8], "logicalDirections": [0, 1],
            "mirrored": [True],
            "transitionKinds": ["authoritative-step"],
        }],
    }


class SeededActionGeneratorTests(unittest.TestCase):
    def test_plan_is_seeded_stable_and_prioritizes_lowest_count(self):
        observed = [{
            "peer": "host", "owner": 0, "unitKind": "unit-villager",
            "action": "moving", "directionCount": 8,
            "logicalDirection": 0, "mirroringMode": 6,
            "transitionKind": "authoritative-step",
        }]
        first = coverage_priority_plan(specification(), observed, 42)
        second = coverage_priority_plan(specification(), observed, 42)
        self.assertEqual(first, second)
        self.assertEqual(first["uncoveredCellCount"], 4)
        self.assertNotEqual(first, coverage_priority_plan(
            specification(), observed, 43
        ))
        owner_zero_direction_zero = next(
            index for index, cell in enumerate(first["cells"])
            if cell["owner"] == 0 and cell["logicalDirection"] == 0
        )
        self.assertEqual(owner_zero_direction_zero, 3)

    def test_rotating_seed_comes_from_commit(self):
        self.assertEqual(rotating_seed("abc"), rotating_seed("abc"))
        self.assertNotEqual(rotating_seed("abc"), rotating_seed("abd"))

    def test_causal_prefix_drops_post_failure_actions(self):
        actions = [
            {"kind": "move", "telemetryTick": 3},
            {"kind": "move", "telemetryTick": 8},
            {"kind": "attack", "telemetryTick": 12},
        ]
        failure = {
            "error": "wrong frame",
            "hostRender": {"tick": 9}, "joinRender": {"tick": 10},
        }
        replay = causal_replay_prefix(actions, failure)
        self.assertEqual(replay["retainedActionCount"], 2)
        self.assertEqual(replay["failureTick"], 9)


if __name__ == "__main__":
    unittest.main()

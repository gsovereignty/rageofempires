#!/usr/bin/env python3
"""Regression tests for deterministic runtime telemetry merging."""

from __future__ import annotations

import copy
import unittest

from run_renderer_runtime_coverage import merge_events


def event(key: str, status: str = "renderable") -> dict:
    return {
        "stable_render_state_key": key,
        "render_state": {"object_kind": key},
        "requested_asset": {"slp_id": 1},
        "status": status,
        "reason": "",
        "entity_id": 7,
        "simulation_tick": 3,
        "renderer_call_site": "test",
    }


class MergeEventsTests(unittest.TestCase):
    def test_deduplicates_and_sorts(self) -> None:
        first = event("z")
        duplicate = copy.deepcopy(first)
        duplicate["entity_id"] = 99
        duplicate["simulation_tick"] = 8
        self.assertEqual(
            ["a", "z"],
            [
                item["stable_render_state_key"]
                for item in merge_events(
                    [[first], [duplicate, event("a")]]
                )
            ],
        )

    def test_rejects_conflicting_classification(self) -> None:
        conflicting = event("same", "missing_mapping")
        with self.assertRaisesRegex(RuntimeError, "conflicting telemetry"):
            merge_events([[event("same")], [conflicting]])


if __name__ == "__main__":
    unittest.main()

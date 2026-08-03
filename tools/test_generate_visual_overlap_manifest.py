#!/usr/bin/env python3
"""Tests for visual-overlap corpus manifest generation."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from generate_visual_overlap_manifest import (
    blocked_case,
    case_prefix,
    merge_manifest,
    parse_ticks,
    resolve_scenarios,
)


class GenerateVisualOverlapManifestTests(unittest.TestCase):
    def test_parse_ticks_deduplicates_in_order(self) -> None:
        self.assertEqual(parse_ticks("0, 20,20,60"), [0, 20, 60])
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_ticks("0,-1")

    def test_resolve_scenarios_sorts_and_deduplicates(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "a.scenario"
            second = root / "b.scenario"
            first.touch()
            second.touch()
            self.assertEqual(
                resolve_scenarios([str(root / "*.scenario"), str(first)]),
                [first.resolve(), second.resolve()],
            )

    def test_merge_manifest_prefixes_ids_and_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "manifest.json"
            manifest.write_text(json.dumps({
                "schema_version": 1,
                "cases": [{
                    "id": "unit-7", "actual": "actual.bmp",
                    "terrain": "terrain.bmp", "sprite": "unit-7.tga",
                    "x": 1, "y": 2, "metadata": {"tick": 4},
                }],
            }))
            cases: list[dict[str, object]] = []
            merge_manifest(cases, manifest, Path("captures/demo-t4"), "demo-t4")
            self.assertEqual(cases[0]["id"], "demo-t4-unit-7")
            self.assertEqual(cases[0]["actual"], "captures/demo-t4/actual.bmp")
            self.assertEqual(cases[0]["sprite"], "captures/demo-t4/unit-7.tga")

    def test_case_prefix_is_manifest_safe(self) -> None:
        self.assertEqual(case_prefix(Path("My audit!.scenario"), 12), "My-audit-t12")

    def test_blocked_capture_preserves_scenario_and_tick(self) -> None:
        case = blocked_case(Path("demo.scenario"), 120, "demo-t120", "timed out")
        self.assertEqual(case["id"], "demo-t120-capture")
        self.assertEqual(case["blocked_reason"], "timed out")
        self.assertEqual(case["metadata"], {"scenario": "demo.scenario", "tick": 120})


if __name__ == "__main__":
    unittest.main()

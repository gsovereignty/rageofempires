#!/usr/bin/env python3
"""Focused overlap manifest and baseline tests."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from batch_visual_overlap_audit import run_batch
from capture_visual_overlap import is_game_process_name
from visual_overlap_decisions import compare, update_baseline, validate_decisions


class VisualOverlapToolsTests(unittest.TestCase):
    def test_game_process_name_matches_only_game_executables(self) -> None:
        self.assertTrue(is_game_process_name("aoe_reconstruction"))
        self.assertTrue(is_game_process_name("AoE Archaeology"))
        self.assertTrue(is_game_process_name("  AoE Archaeology  "))
        self.assertFalse(is_game_process_name("capture_visual_overlap.py"))
        self.assertFalse(is_game_process_name("rg aoe_reconstruction"))
        self.assertFalse(is_game_process_name("aoe_reconstruction_helper"))

    def test_blocked_case_is_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({
                "schema_version": 1,
                "cases": [{"id": "unit-7", "blocked_reason": "procedural fallback",
                           "metadata": {"tick": 4}}],
            }))
            report = run_batch(manifest, root / "review")
            self.assertEqual(report["summary"], {
                "total": 1, "flagged": 0, "clean": 0, "blocked": 1,
            })
            self.assertEqual(report["cases"][0]["status"], "blocked")

    def test_baseline_comparison_is_deterministic(self) -> None:
        report = {"schema_version": 1, "cases": [
            {"id": "b", "status": "pass", "metadata": {"tick": 2}},
            {"id": "a", "status": "overlap_detected", "metadata": {"tick": 1}},
        ]}
        baseline = update_baseline(report, {"a": "uncertain", "b": "intentional"})
        self.assertEqual(compare(report, baseline)["unresolved"], ["a"])
        changed = json.loads(json.dumps(report))
        changed["cases"][1]["metadata"]["tick"] = 3
        comparison = compare(changed, baseline)
        self.assertEqual(comparison["changed"], ["a"])
        changed["cases"].pop(0)
        self.assertEqual(compare(changed, baseline)["missing"], ["b"])

    def test_decisions_reject_unreviewed_and_incomplete(self) -> None:
        with self.assertRaisesRegex(ValueError, "must be bug"):
            validate_decisions({"decisions": [{"id": "a", "decision": "unreviewed"}]})
        with self.assertRaisesRegex(ValueError, "missing cases"):
            update_baseline(
                {"schema_version": 1, "cases": [{"id": "a"}]}, {}
            )


if __name__ == "__main__":
    unittest.main()

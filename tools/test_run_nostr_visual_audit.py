#!/usr/bin/env python3

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch


MODULE_PATH = Path(__file__).with_name("run_nostr_visual_audit.py")
SPEC = importlib.util.spec_from_file_location("nostr_audit_runner", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)


def load_runner_without_browser_dependencies():
    """Load pure orchestration helpers without requiring Selenium."""
    selenium_modules = {
        name: MagicMock()
        for name in (
            "PIL",
            "PIL.Image",
            "PIL.ImageChops",
            "websocket",
            "selenium",
            "selenium.common",
            "selenium.common.exceptions",
            "selenium.webdriver",
            "selenium.webdriver.common",
            "selenium.webdriver.common.action_chains",
            "selenium.webdriver.common.actions",
            "selenium.webdriver.common.actions.action_builder",
            "selenium.webdriver.common.actions.pointer_input",
            "selenium.webdriver.common.by",
            "selenium.webdriver.common.keys",
            "selenium.webdriver.support",
            "selenium.webdriver.support.ui",
        )
    }
    with patch.dict(sys.modules, selenium_modules):
        SPEC.loader.exec_module(MODULE)


load_runner_without_browser_dependencies()


class RelayRotationTests(unittest.TestCase):
    def test_exact_destination_id_reserves_declared_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination = MODULE.allocate_audit_destination(
                root / "artifacts", root / "reports", "same-seed-checkpoint"
            )
            self.assertEqual(destination.artifacts.name,
                             "same-seed-checkpoint")
            self.assertTrue(destination.report.name.endswith(
                "-NOSTR-E2E-VISUAL-GAMEPLAY-same-seed-checkpoint.md"
            ))
            with self.assertRaises(FileExistsError):
                MODULE.allocate_audit_destination(
                    root / "artifacts", root / "reports",
                    "same-seed-checkpoint",
                )

    def test_active_artifact_progress_extends_inactivity_deadline(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = (
                "import pathlib,time; p=pathlib.Path(r'%s'); "
                "[(p.write_text(str(i)), time.sleep(0.08)) for i in range(3)]"
            ) % (root / "progress.txt")
            return_code, timeout_kind = MODULE.run_with_progress_deadline(
                [sys.executable, "-c", script], cwd=root,
                progress_root=root, hard_timeout_seconds=2.0,
                progress_timeout_seconds=0.15, poll_seconds=0.02,
            )
            self.assertEqual((return_code, timeout_kind), (0, None))

    def test_inactive_child_hits_progress_deadline_before_hard_cap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            return_code, timeout_kind = MODULE.run_with_progress_deadline(
                [sys.executable, "-c", "import time; time.sleep(2)"],
                cwd=root, progress_root=root, hard_timeout_seconds=1.0,
                progress_timeout_seconds=0.1, poll_seconds=0.02,
            )
            self.assertEqual((return_code, timeout_kind), (124, "progress"))

    def test_timeout_retains_complete_blocked_attempt_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            root.mkdir(exist_ok=True)
            attempt = MODULE.retain_attempt_timeout(
                root, 0, ["wss://one", "wss://two"], 90.0, None
            )
            self.assertEqual(
                json.loads((attempt / "verdict.json").read_text())["status"],
                "BLOCKED",
            )
            self.assertEqual(
                json.loads((attempt / "first-failure.json").read_text())[
                    "classification"
                ],
                "attempt-deadline",
            )
            for name in (
                "run.json", "actions.jsonl", "correlated-frames.jsonl",
                "visual-oracles.jsonl", "coverage.json", "report.md",
            ):
                self.assertTrue((attempt / name).is_file(), name)

    def test_extracts_rejected_relays_from_retained_acknowledgements(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "first-failure.json").write_text(json.dumps({
                "infrastructureBlocker": {
                    "rejectedPublications": {"host": [{
                        "intentId": "lobby-2", "results": [
                            {"relay": "wss://reject/", "ok": False},
                            {"relay": "wss://accept/", "ok": True},
                        ],
                    }]},
                },
            }))
            self.assertEqual(MODULE.rejected_relays(root), {
                "wss://reject": ["lobby-2"],
            })

    def test_rotates_deterministic_distinct_quorums(self):
        self.assertEqual(MODULE.rotating_quorums(["a", "b", "c", "d"]), [
            ["a", "b", "c"], ["b", "c", "d"],
            ["c", "d", "a"], ["d", "a", "b"],
        ])

    def test_deduplicates_pool_and_requires_two_relays(self):
        self.assertEqual(
            MODULE.rotating_quorums(["a", "a", "b"]),
            [["a", "b"], ["b", "a"]],
        )
        self.assertEqual(MODULE.rotating_quorums(["a"]), [])
        with self.assertRaisesRegex(ValueError, "at least two"):
            MODULE.rotating_quorums(["a", "b"], size=1)

    def test_binary_prefix_minimizer_finds_first_matching_failure(self):
        def candidate(limit):
            return {
                "actionLimit": limit,
                "status": "FAIL" if limit >= 6 else "NOT_REPRODUCED",
                "failureIdentity": {"sha256": "target"}
                    if limit >= 6 else None,
            }

        minimum, attempts = MODULE.minimize_prefix(12, "target", candidate)
        self.assertEqual(minimum, 6)
        self.assertLessEqual(len(attempts), 4)

    def test_prefix_minimizer_aborts_on_blocked_candidate(self):
        with self.assertRaisesRegex(MODULE.MinimizationBlocked,
                                    "infrastructure-blocked"):
            MODULE.minimize_prefix(4, "target", lambda limit: {
                "actionLimit": limit, "status": "BLOCKED",
            })

    def test_failure_identity_prefers_first_thrown_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "visual-failures.json").write_text(json.dumps([
                {"phase": "turn", "verdict": "FAIL",
                 "classification": "STALE_DIRECTION_AFTER_TURN"},
            ]))
            (root / "first-failure.json").write_text(json.dumps({
                "error": "Failure: wrapper",
            }))
            identity = MODULE.canonical_failure_identity(root)
            self.assertEqual(identity["kind"], "exception")
            self.assertEqual(identity["value"], "Failure: wrapper")
            self.assertEqual(identity,
                             MODULE.canonical_failure_identity(root))

    def test_failure_identity_ignores_dead_browser_and_uses_screenshot_oracle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "first-failure.json").write_text(json.dumps({
                "error": "InvalidSessionIdException: browser closed",
            }))
            (root / "screenshot-audit.json").write_text(json.dumps({
                "findings": [{
                    "case": "host-house-45", "kind": "sprite_overlap",
                    "overlapPixels": 17, "status": "FAIL",
                }],
            }))
            identity = MODULE.canonical_failure_identity(root)
            self.assertEqual(identity["kind"], "screenshot-oracle")
            self.assertEqual(identity["value"]["case"], "host-house-45")

    def test_dead_browser_without_retained_oracle_has_no_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "first-failure.json").write_text(json.dumps({
                "error": "InvalidSessionIdException: browser closed",
            }))
            self.assertIsNone(MODULE.canonical_failure_identity(root))


if __name__ == "__main__":
    unittest.main()

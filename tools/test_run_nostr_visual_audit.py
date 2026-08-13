#!/usr/bin/env python3

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("run_nostr_visual_audit.py")
SPEC = importlib.util.spec_from_file_location("nostr_audit_runner", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RelayRotationTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()

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

    def test_failure_identity_prefers_structured_visual_record(self):
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
            self.assertEqual(identity["kind"], "visual-oracle")
            self.assertEqual(identity,
                             MODULE.canonical_failure_identity(root))


if __name__ == "__main__":
    unittest.main()

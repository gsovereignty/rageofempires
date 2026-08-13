#!/usr/bin/env python3

import importlib.util
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


if __name__ == "__main__":
    unittest.main()

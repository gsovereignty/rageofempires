#!/usr/bin/env python3

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("run_nostr_visual_display_matrix.py")
SPEC = importlib.util.spec_from_file_location("nostr_display_matrix", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DisplayMatrixTests(unittest.TestCase):
    def test_required_matrix_covers_aspects_dpr_zoom_and_renderer(self):
        cases = [case for case in MODULE.display_cases(False)
                 if case["required"]]
        self.assertGreaterEqual(len({case["viewport"] for case in cases}), 3)
        self.assertEqual({case["dpr"] for case in cases}, {1.0, 2.0})
        self.assertEqual({case["zoom"] for case in cases}, {1.0, 2.0})
        self.assertTrue(any(case["browserArguments"] for case in cases))

    def test_headed_cell_is_explicitly_blocked_when_not_enabled(self):
        headed = MODULE.display_cases(False)[-1]
        self.assertEqual(headed["status"], "BLOCKED")
        self.assertFalse(headed["executed"])
        self.assertFalse(headed["required"])

    def test_required_fail_and_block_propagate(self):
        cases = [{"required": True, "status": "PASS"},
                 {"required": False, "status": "BLOCKED"}]
        self.assertEqual(MODULE.matrix_status(cases), "PASS")
        cases.append({"required": True, "status": "BLOCKED"})
        self.assertEqual(MODULE.matrix_status(cases), "BLOCKED")
        cases.append({"required": True, "status": "FAIL"})
        self.assertEqual(MODULE.matrix_status(cases), "FAIL")
        self.assertEqual(MODULE.matrix_status([
            {"required": True, "status": "PASS"},
            {"required": False, "executed": True, "status": "FAIL"},
        ]), "FAIL")

    def test_reads_renderer_identity_from_retained_child_attempt(self):
        with tempfile.TemporaryDirectory(dir=MODULE.ROOT) as directory:
            root = Path(directory)
            attempt = root / "attempt"
            attempt.mkdir()
            (attempt / "evidence.json").write_text(json.dumps({
                "browser": {
                    "hostRenderer": {"unmaskedRenderer": "SwiftShader"},
                    "joinRenderer": {"unmaskedRenderer": "SwiftShader"},
                    "arguments": ["--use-angle=swiftshader"],
                },
            }))
            child = root / "child"
            child.mkdir()
            (child / "attempts.json").write_text(json.dumps({
                "attempts": [{
                    "artifactPath": str(attempt.relative_to(MODULE.ROOT)),
                }],
            }))
            evidence = MODULE.renderer_evidence(child)
            self.assertEqual(
                evidence["host"]["unmaskedRenderer"], "SwiftShader"
            )


if __name__ == "__main__":
    unittest.main()

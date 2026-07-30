import unittest

from compare_renderer_asset_coverage import compare


def report(rows):
    return {
        "schema": "aoe-renderer-asset-coverage-v2",
        "rows": rows,
    }


def row(kind, frame, status, reason=""):
    return {
        "object_kind": kind,
        "state_dimensions": {"action": "idle", "animation_frame": frame},
        "status": status,
        "failure_reason": reason,
    }


class CoverageComparisonTests(unittest.TestCase):
    def test_same_reviewed_gap_passes(self):
        baseline = report([row("sheep", 0, "missing_archive_resource", "x")])
        self.assertEqual(compare(baseline, baseline), [])

    def test_new_unresolved_state_fails(self):
        baseline = report([row("sheep", 0, "renderable")])
        current = report([row("sheep", 0, "missing_archive_resource", "x")])
        findings = compare(baseline, current)
        self.assertEqual(len(findings), 1)
        self.assertIn("new unresolved state", findings[0])

    def test_reviewed_gap_becoming_renderable_passes(self):
        baseline = report([row("sheep", 0, "missing_archive_resource", "x")])
        current = report([row("sheep", 0, "renderable")])
        self.assertEqual(compare(baseline, current), [])

    def test_changed_failure_cannot_hide_regression(self):
        baseline = report([row("farm", 0, "missing_mapping", "old")])
        current = report([row("farm", 0, "decode_failure", "new")])
        findings = compare(baseline, current)
        self.assertEqual(len(findings), 1)
        self.assertIn("changed unresolved state", findings[0])


if __name__ == "__main__":
    unittest.main()

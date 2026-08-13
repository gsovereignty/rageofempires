#!/usr/bin/env python3

import unittest
import json
from pathlib import Path

from nostr_visual_route_coverage import (
    REQUIRED_ROUTE_IDS,
    evaluate_route_coverage,
    validate_route_catalog,
)


def specification():
    return {"routeCatalog": [
        {"id": identifier, "status": "required", "evidence": "fixture"}
        for identifier in sorted(REQUIRED_ROUTE_IDS)
    ]}


class RouteCoverageTests(unittest.TestCase):
    def test_tracked_catalog_covers_every_route(self):
        value = json.loads((
            Path(__file__).resolve().parents[1] / "resources" /
            "nostr-visual-gameplay-coverage.json"
        ).read_text())
        catalog = validate_route_catalog(value)
        self.assertEqual({entry["id"] for entry in catalog},
                         REQUIRED_ROUTE_IDS)

    def test_missing_route_blocks(self):
        result = evaluate_route_coverage(specification(), [])
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(len(result["missingRequiredRoutes"]),
                         len(REQUIRED_ROUTE_IDS))

    def test_every_required_route_passes(self):
        observed = [{"id": identifier, "verdict": "PASS"}
                    for identifier in REQUIRED_ROUTE_IDS]
        self.assertEqual(
            evaluate_route_coverage(specification(), observed)["status"],
            "PASS",
        )

    def test_route_failure_wins(self):
        observed = [{"id": identifier, "verdict": (
            "FAIL" if identifier == "zigzag" else "PASS"
        )} for identifier in REQUIRED_ROUTE_IDS]
        self.assertEqual(
            evaluate_route_coverage(specification(), observed)["status"],
            "FAIL",
        )

    def test_observed_blocked_route_does_not_count_as_pass(self):
        observed = [{"id": identifier, "verdict": (
            "BLOCKED" if identifier == "zigzag" else "PASS"
        )} for identifier in REQUIRED_ROUTE_IDS]
        self.assertEqual(
            evaluate_route_coverage(specification(), observed)["status"],
            "BLOCKED",
        )

    def test_catalog_omission_fails_closed(self):
        value = specification()
        value["routeCatalog"].pop()
        with self.assertRaisesRegex(ValueError, "silently omits"):
            validate_route_catalog(value)


if __name__ == "__main__":
    unittest.main()

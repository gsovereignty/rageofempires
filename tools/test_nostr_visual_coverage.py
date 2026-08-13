#!/usr/bin/env python3

import json
import tempfile
import unittest
from pathlib import Path

from nostr_visual_coverage import (
    REQUIRED_CATALOG_IDS,
    evaluate_coverage,
    load_specification,
    required_cells,
)


def specification():
    return {
        "schemaVersion": 1, "minimumSamplesPerCell": 3,
        "requiredMatrices": [{
            "peers": ["host", "join"], "owners": [0],
            "unitKinds": ["unit-villager"], "actions": ["moving"],
            "directionCounts": [8], "logicalDirections": list(range(8)),
            "mirrored": [True],
            "transitionKinds": ["authoritative-step"],
        }],
    }


def record(peer, direction, tick, verdict="PASS"):
    return {
        "peer": peer, "owner": 0, "unitKind": "unit-villager",
        "action": "moving", "directionCount": 8,
        "logicalDirection": direction, "mirroringMode": 6,
        "transitionKind": "authoritative-step", "tick": tick,
        "entity": 7, "verdict": verdict,
        "screenshot": {"path": f"{peer}-{direction}-{tick}.png"},
    }


def catalog_specification():
    value = specification()
    value["requiredCatalogAssertions"] = [
        "movement-direction", "resolved-frame", "mirror",
        "animation-progress", "pixel-direction",
    ]
    value["unitActionCatalog"] = [{
        "id": "villager-empty-moving", "status": "required",
        "evidence": "fixture",
    }]
    return value


class VisualCoverageTests(unittest.TestCase):
    def test_real_catalog_covers_every_required_unit_action_category(self):
        value = load_specification(
            Path(__file__).resolve().parents[1] / "resources" /
            "nostr-visual-gameplay-coverage.json"
        )
        self.assertEqual(
            {entry["id"] for entry in value["unitActionCatalog"]},
            REQUIRED_CATALOG_IDS,
        )

    def test_catalog_omission_fails_closed(self):
        value = json.loads((
            Path(__file__).resolve().parents[1] / "resources" /
            "nostr-visual-gameplay-coverage.json"
        ).read_text())
        value["unitActionCatalog"].pop()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "coverage.json"
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError, "silently omits"):
                load_specification(path)

    def test_expands_every_matrix_dimension(self):
        self.assertEqual(len(required_cells(specification())), 16)

    def test_missing_samples_block(self):
        result = evaluate_coverage(specification(), [])
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(len(result["missingRequiredCells"]), 16)

    def test_three_samples_for_every_cell_pass(self):
        records = [
            record(peer, direction, tick)
            for peer in ("host", "join")
            for direction in range(8)
            for tick in range(3)
        ]
        result = evaluate_coverage(specification(), records)
        self.assertEqual(result["status"], "PASS")
        self.assertFalse(result["missingRequiredCells"])

    def test_failed_oracle_forces_fail_even_with_complete_coverage(self):
        records = [
            record(peer, direction, tick,
                   "FAIL" if peer == "join" and direction == 6 and tick == 1
                   else "PASS")
            for peer in ("host", "join")
            for direction in range(8)
            for tick in range(3)
        ]
        result = evaluate_coverage(specification(), records)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(len(result["failedOracleRecordIndexes"]), 1)

    def test_missing_catalog_assertions_block_even_when_cells_complete(self):
        records = [
            record(peer, direction, tick)
            for peer in ("host", "join")
            for direction in range(8)
            for tick in range(3)
        ]
        result = evaluate_coverage(catalog_specification(), records)
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["missingCatalogAssertions"][0]["id"],
                         "villager-empty-moving")

    def test_catalog_pass_requires_every_assertion(self):
        records = [
            record(peer, direction, tick)
            for peer in ("host", "join")
            for direction in range(8)
            for tick in range(3)
        ]
        records.append({
            "verdict": "PASS",
            "catalogIds": ["villager-empty-moving"],
            "assertions": [
                "movement-direction", "resolved-frame", "mirror",
                "animation-progress", "pixel-direction",
            ],
        })
        result = evaluate_coverage(catalog_specification(), records)
        self.assertEqual(result["status"], "PASS")
        self.assertFalse(result["missingCatalogAssertions"])


if __name__ == "__main__":
    unittest.main()

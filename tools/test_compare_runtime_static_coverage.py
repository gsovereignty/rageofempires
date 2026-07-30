#!/usr/bin/env python3

import unittest

from compare_runtime_static_coverage import compare


def static_row(status: str = "missing_composite_part"):
    return {
        "schema": "aoe-renderer-asset-coverage-v2",
        "rows": [{
            "object_kind": "town_center",
            "state_dimensions": {
                "building_state": "completed",
                "owner": 0,
                "age": "dark",
                "civilization": "generic",
                "architecture_family": 0,
                "damage_stage": 0,
                "construction_stage": 4,
            },
            "expected_asset_ids": {
                "graphic_id": 3241,
                "slp_id": None,
                "overlay_graphic_ids": [],
            },
            "status": status,
        }],
    }


def runtime_event(status: str = "missing_composite_part"):
    return {
        "schema": "aoe-runtime-render-fallback-v1",
        "events": [{
            "render_state": {
                "category": "building",
                "object_kind": "town_center",
                "building_state": "completed",
                "owner": 0,
                "age": "dark",
                "civilization": "generic",
                "architecture_family": 0,
                "damage_stage": 0,
                "construction_stage": 4,
            },
            "requested_asset": {
                "graphic_id": 3241,
                "slp_id": None,
                "overlay_graphic_ids": [],
            },
            "status": status,
        }],
    }


def static_unit_row(status: str = "missing_archive_resource"):
    return {
        "schema": "aoe-renderer-asset-coverage-v2",
        "rows": [{
            "object_kind": "sheep",
            "state_dimensions": {
                "action": "attacking",
                "action_detail": "none",
                "moving": False,
                "owner": 0,
                "architecture_family": 0,
                "direction": 3,
                "animation_frame": 0,
            },
            "expected_asset_ids": {
                "graphic_id": None,
                "slp_id": 3623,
                "overlay_graphic_ids": [],
            },
            "status": status,
            "failure_reason": "SLP 3623 absent from graphics.drs",
        }],
    }


def runtime_unit_event(status: str = "missing_archive_resource"):
    return {
        "schema": "aoe-runtime-render-fallback-v1",
        "events": [{
            "render_state": {
                "category": "unit",
                "object_kind": "sheep",
                "action": "attacking",
                "action_detail": "none",
                "moving": False,
                "owner": 0,
                "architecture_family": 0,
                "direction": 3,
                "animation_frame": 0,
            },
            "requested_asset": {
                "graphic_id": None,
                "slp_id": 3623,
                "overlay_graphic_ids": [],
            },
            "status": status,
            "reason": "SLP 3623 absent from graphics.drs",
        }],
    }


class RuntimeStaticAgreementTests(unittest.TestCase):
    def test_matching_event_passes(self):
        self.assertEqual(compare(static_row(), runtime_event()), [])

    def test_status_drift_fails(self):
        problems = compare(static_row(), runtime_event("renderer_failure"))
        self.assertTrue(any("status mismatch" in item for item in problems))

    def test_missing_static_state_fails(self):
        report = static_row()
        report["rows"] = []
        problems = compare(report, runtime_event())
        self.assertTrue(any("absent from static audit" in item for item in problems))

    def test_asset_drift_fails(self):
        event = runtime_event()
        event["events"][0]["requested_asset"]["graphic_id"] = 999
        problems = compare(static_row(), event)
        self.assertTrue(any("graphic_id mismatch" in item for item in problems))

    def test_matching_unit_event_passes(self):
        self.assertEqual(
            compare(static_unit_row(), runtime_unit_event()), []
        )

    def test_unit_reason_drift_fails(self):
        event = runtime_unit_event()
        event["events"][0]["reason"] = "generic renderer failure"
        problems = compare(static_unit_row(), event)
        self.assertTrue(
            any("failure reason mismatch" in item for item in problems)
        )

    def test_intentional_projectile_fallback_matches(self):
        static = {
            "schema": "aoe-renderer-asset-coverage-v2",
            "rows": [{
                "object_kind": "arrow",
                "state_dimensions": {
                    "object_category": "projectile",
                    "shadow": False,
                    "direction": 7,
                    "animation_frame": 2,
                },
                "expected_asset_ids": {
                    "graphic_id": 3378,
                    "slp_id": 3799,
                    "shadow_slp_id": 3800,
                    "overlay_graphic_ids": [],
                },
                "status": "intentional_procedural",
                "failure_reason":
                    "projectile directional transform is explicitly unproved",
            }],
        }
        runtime = {
            "schema": "aoe-runtime-render-fallback-v1",
            "events": [{
                "render_state": {
                    "category": "projectile",
                    "object_kind": "arrow",
                    "shadow": False,
                    "direction": 7,
                    "animation_frame": 2,
                },
                "requested_asset": {
                    "graphic_id": 3378,
                    "slp_id": 3799,
                    "shadow_slp_id": 3800,
                    "overlay_graphic_ids": [],
                },
                "status": "intentional_procedural",
                "reason":
                    "projectile directional transform is explicitly unproved",
            }],
        }
        self.assertEqual(compare(static, runtime), [])


if __name__ == "__main__":
    unittest.main()

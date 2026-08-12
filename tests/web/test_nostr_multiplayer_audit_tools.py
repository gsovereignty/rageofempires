#!/usr/bin/env python3

import copy
import tempfile
import unittest
from pathlib import Path

from nostr_multiplayer_smoke_test import (
    Failure,
    analyze_render_samples,
    analyze_render_samples_for_audit,
    audited_key,
    capture_failure_value,
    collapse_match_details,
    diagnostics,
    render_diagnostics,
    visual_failures,
    visual_findings,
    write_audit_bundle,
)


def sample(frame: int, x: float, source: str = "legacy",
           host_camera: float = 0.0, join_camera: float = 0.0):
    entity = {
        "id": 7,
        "category": "unit-villager",
        "renderPosition": {"x": x, "y": 20.0},
        "source": source,
        "expectedAssetStatus": "renderable",
        "expectedResourceIds": [1479],
        "expectedRequiredFrameCount": 10,
        "moving": False,
        "animationState": 0,
        "layers": ([{"resourceId": 1479, "frame": frame}]
                   if source == "legacy" else []),
    }
    host_state = {
        "frame": frame, "tick": frame, "entities": [entity],
        "camera": {"x": host_camera, "y": 0.0},
    }
    join_entity = copy.deepcopy(entity)
    join_entity["renderPosition"] = {
        "x": x + host_camera - join_camera, "y": 20.0
    }
    join_state = {
        "frame": frame, "tick": frame, "entities": [join_entity],
        "camera": {"x": join_camera, "y": 0.0},
    }
    return {"host": host_state, "join": join_state}


def moving_sample(dx: int, dy: int, facing: int):
    value = sample(1, 10.0)
    for peer in ("host", "join"):
        entity = value[peer]["entities"][0]
        entity["moving"] = True
        entity["previousPosition"] = {"x": 10, "y": 10}
        entity["simulationPosition"] = {"x": 10 + dx, "y": 10 + dy}
        entity["facing"] = facing
    return value


def gathering_sample(*, amount: int = 100, include_resource: bool = True):
    value = sample(1, 10.0)
    for peer in ("host", "join"):
        unit = value[peer]["entities"][0]
        unit.update({
            "action": "gathering",
            "hasResourceTarget": True,
            "returningResource": False,
            "resourceTarget": {"x": 4, "y": 3},
            "resourceTargetInMap": True,
            "resourceTargetKind": "tile",
            "resourceTargetExists": True,
            "resourceTargetAmount": amount,
            "resourceTargetVisible": True,
            "resourceTargetEntityId": 28,
            "resourceBuildingId": 0,
            "resourceUnitId": 0,
        })
        if include_resource:
            value[peer]["entities"].append({
                "id": 28,
                "category": "resource-17",
                "renderPosition": {"x": 30.0, "y": 40.0},
                "source": "legacy",
                "expectedAssetStatus": "renderable",
                "expectedResourceIds": [1503],
                "expectedRequiredFrameCount": 7,
                "facing": 0,
                "layers": [{"resourceId": 1503, "frame": 0}],
            })
    return value


class AuditedInputTests(unittest.TestCase):
    def test_audited_key_preserves_selection_without_canvas_click(self):
        class Canvas:
            def __init__(self):
                self.keys = []

            def send_keys(self, key):
                self.keys.append(key)

        class Driver:
            def __init__(self):
                self.canvas = Canvas()

            def find_element(self, *_):
                return self.canvas

        driver = Driver()
        actions = []
        audited_key(driver, actions, "host", "h")
        self.assertEqual(driver.canvas.keys, ["h"])
        self.assertEqual(actions[0]["key"], "h")

    def test_collapses_match_details_through_visible_button(self):
        class Element:
            def __init__(self, driver, name):
                self.driver = driver
                self.name = name

            def get_attribute(self, name):
                if self.name == "toggle" and name == "aria-expanded":
                    return "false" if self.driver.hidden else "true"
                if self.name == "details" and name == "hidden":
                    return "" if self.driver.hidden else None
                return None

            def click(self):
                self.driver.hidden = True

        class Driver:
            def __init__(self):
                self.hidden = False

            def find_element(self, _, identifier):
                return Element(
                    self,
                    "toggle" if identifier ==
                    "toggle-nostr-session-details" else "details",
                )

        driver = Driver()
        actions = []
        collapse_match_details(driver, actions, "host")
        self.assertTrue(driver.hidden)
        self.assertEqual(actions[0]["target"], "toggle-nostr-session-details")


class FailureEvidenceTests(unittest.TestCase):
    def test_diagnostics_tolerates_missing_module(self):
        class Driver:
            def execute_script(self, source):
                self.source = source
                return None

        driver = Driver()
        self.assertIsNone(diagnostics(driver))
        self.assertIn("typeof Module", driver.source)

    def test_render_diagnostics_tolerates_missing_module(self):
        class Driver:
            def execute_script(self, source):
                self.source = source
                return None

        driver = Driver()
        self.assertIsNone(render_diagnostics(driver))
        self.assertIn("typeof Module", driver.source)

    def test_secondary_capture_error_becomes_evidence(self):
        def fail():
            raise RuntimeError("browser gone")

        self.assertEqual(
            capture_failure_value("join diagnostics", fail),
            {"captureError": "join diagnostics: RuntimeError: browser gone"},
        )


class RenderOracleTests(unittest.TestCase):
    def test_accepts_monotonic_legacy_motion(self):
        result = analyze_render_samples([sample(1, 10.0), sample(2, 14.0)])
        self.assertEqual(result["frames"], 2)
        self.assertEqual(result["legacy"], 4)
        self.assertEqual(result["maximumFrameDisplacement"], 4.0)

    def test_rejects_non_monotonic_frames(self):
        with self.assertRaisesRegex(Failure, "non-monotonic"):
            analyze_render_samples([sample(2, 10.0), sample(2, 11.0)])

    def test_rejects_teleport_candidate(self):
        with self.assertRaisesRegex(Failure, "teleport candidate"):
            analyze_render_samples([sample(1, 10.0), sample(2, 250.0)])

    def test_accepts_all_canonical_movement_facings(self):
        cases = (
            (1, 1, 0), (0, 1, 1), (-1, 1, 2), (-1, 0, 3),
            (-1, -1, 4), (0, -1, 5), (1, -1, 6), (1, 0, 7),
        )
        for dx, dy, facing in cases:
            with self.subTest(dx=dx, dy=dy, facing=facing):
                analyze_render_samples([moving_sample(dx, dy, facing)])

    def test_rejects_movement_facing_mismatch(self):
        with self.assertRaisesRegex(Failure, "movement facing mismatch"):
            analyze_render_samples([moving_sample(1, 0, 3)])

    def test_stationary_action_may_face_away_from_previous_movement(self):
        value = moving_sample(1, 0, 3)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["moving"] = False
        analyze_render_samples([value])

    def test_accepts_sixteen_direction_unit_facing(self):
        value = moving_sample(1, 0, 14)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["expectedDirectionCount"] = 16
        analyze_render_samples([value])

    def test_accepts_visible_gather_target_with_resource_sprite(self):
        result = analyze_render_samples([gathering_sample()])
        self.assertEqual(result["legacy"], 4)

    def test_rejects_gathering_depleted_tile(self):
        with self.assertRaisesRegex(Failure, "depleted resource"):
            analyze_render_samples([gathering_sample(amount=0)])

    def test_rejects_missing_resource_target(self):
        value = gathering_sample()
        for peer in ("host", "join"):
            value[peer]["entities"][0]["resourceTargetExists"] = False
        with self.assertRaisesRegex(Failure, "target does not exist"):
            analyze_render_samples([value])

    def test_rejects_unrendered_visible_gather_target(self):
        with self.assertRaisesRegex(Failure, "resource is not rendered"):
            analyze_render_samples([
                gathering_sample(include_resource=False),
            ])

    def test_rejects_unproved_render_source(self):
        with self.assertRaisesRegex(Failure, "unproved production"):
            analyze_render_samples([sample(1, 10.0, "procedural_or_unproven")])

    def test_rejects_non_renderable_expected_mapping(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["expectedAssetStatus"] = (
                "missing_composite_part"
            )
        with self.assertRaisesRegex(Failure, "non-renderable asset mapping"):
            analyze_render_samples([value])

    def test_retains_visual_failure_without_aborting_match_journey(self):
        result = analyze_render_samples_for_audit(
            [sample(1, 10.0, "intentional_procedural")], "combat"
        )
        self.assertEqual(result["verdict"], "FAIL")
        self.assertEqual(result["phase"], "combat")
        self.assertEqual(visual_failures({"oracle": result}), [result])

    def test_rejects_contractual_procedural_effect(self):
        value = sample(1, 10.0, "intentional_procedural")
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["expectedAssetStatus"] = "intentional_procedural"
            entity["expectedSourceMapping"] = "generic-impact-contract"
            entity["expectedResourceIds"] = []
            entity["primitives"] = [{
                "operation": "line",
                "rgba": [225, 190, 105, 255],
                "x1": -5.0, "y1": -5.0, "x2": 5.0, "y2": 5.0,
            }]
        with self.assertRaisesRegex(Failure, "procedural production visual"):
            analyze_render_samples([value])

    def test_rejects_procedural_visual_in_every_production_category(self):
        categories = (
            "terrain", "resource-gold", "unit-villager", "building-house",
            "shadow", "damage-overlay", "projectile", "impact", "effect",
            "hud", "menu", "minimap", "terminal",
        )
        for category in categories:
            with self.subTest(category=category):
                value = sample(1, 10.0, "intentional_procedural")
                for peer in ("host", "join"):
                    value[peer]["entities"][0]["category"] = category
                with self.assertRaisesRegex(
                    Failure, "procedural production visual"
                ):
                    analyze_render_samples([value])

    def test_rejects_procedural_without_geometry(self):
        value = sample(1, 10.0, "intentional_procedural")
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["expectedAssetStatus"] = "intentional_procedural"
            entity["expectedSourceMapping"] = "generic-impact-contract"
        with self.assertRaisesRegex(Failure, "procedural production visual"):
            analyze_render_samples([value])

    def test_rejects_procedural_before_peer_primitive_comparison(self):
        value = sample(1, 10.0, "intentional_procedural")
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["expectedAssetStatus"] = "intentional_procedural"
            entity["expectedSourceMapping"] = "generic-impact-contract"
            entity["primitives"] = [{
                "operation": "fill_rect", "rgba": [1, 2, 3, 255],
                "x": 0.0, "y": 0.0, "width": 4.0, "height": 4.0,
            }]
        value["join"]["entities"][0]["primitives"][0]["width"] = 5.0
        with self.assertRaisesRegex(Failure, "procedural production visual"):
            analyze_render_samples([value])

    def test_normalizes_different_client_cameras(self):
        result = analyze_render_samples([
            sample(1, 10.0, host_camera=100.0, join_camera=40.0),
            sample(2, 14.0, host_camera=100.0, join_camera=40.0),
        ])
        self.assertEqual(result["maximumFrameDisplacement"], 4.0)

    def test_animation_sequence_restarts_when_facing_changes(self):
        first = sample(1, 10.0)
        second = sample(2, 11.0)
        for peer in ("host", "join"):
            first[peer]["entities"][0]["layers"][0]["frame"] = 35
            second[peer]["entities"][0]["layers"][0]["frame"] = 49
            second[peer]["entities"][0]["facing"] = 7
        result = analyze_render_samples([first, second])
        self.assertEqual(result["frames"], 2)

    def test_rejects_client_asset_divergence(self):
        value = sample(1, 10.0)
        value["join"]["entities"][0]["layers"][0]["resourceId"] = 999
        value["join"]["entities"][0]["expectedResourceIds"].append(999)
        with self.assertRaisesRegex(Failure, "client asset divergence"):
            analyze_render_samples([value])

    def test_records_non_renderable_mapping_without_stopping_match(self):
        value = sample(1, 10.0)
        value["host"]["entities"][0]["expectedAssetStatus"] = "missing_mapping"
        result = analyze_render_samples_for_audit([value], "movement")
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIn(
            "non-renderable asset mapping", result["failure"]
        )

    def test_records_empty_renderable_mapping_without_stopping_gameplay(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["expectedResourceIds"] = []
        result = analyze_render_samples([value])
        self.assertEqual(len(result["unresolvedExpectedMappings"]), 2)
        classified = analyze_render_samples_for_audit([value], "movement")
        self.assertEqual(classified["verdict"], "BLOCKED")
        self.assertEqual(visual_failures({"oracle": classified}), [])
        self.assertEqual(visual_findings({"oracle": classified}), [classified])

    def test_rejects_missing_entity_at_shared_camera(self):
        value = sample(1, 10.0)
        value["join"]["entities"] = []
        with self.assertRaisesRegex(Failure, "entity-set divergence"):
            analyze_render_samples([value])

    def test_rejects_duplicate_transient_identity(self):
        value = sample(1, 10.0)
        value["host"]["entities"].append(
            copy.deepcopy(value["host"]["entities"][0])
        )
        with self.assertRaisesRegex(Failure, "duplicate host"):
            analyze_render_samples([value])

    def test_rejects_animation_state_divergence(self):
        value = sample(1, 10.0)
        value["join"]["entities"][0]["animationState"] = 2
        with self.assertRaisesRegex(Failure, "animationState"):
            analyze_render_samples([value])

    def test_rejects_layer_frame_divergence(self):
        value = sample(1, 10.0)
        value["join"]["entities"][0]["layers"][0]["frame"] = 2
        with self.assertRaisesRegex(Failure, "client asset divergence"):
            analyze_render_samples([value])

    def test_marks_raw_animation_sequence_order_blocked(self):
        first = sample(3, 10.0)
        second = sample(4, 11.0)
        for peer in ("host", "join"):
            first[peer]["entities"][0]["layers"][0]["frame"] = 3
            second[peer]["entities"][0]["layers"][0]["frame"] = 2
        result = analyze_render_samples([first, second])
        self.assertEqual(result["animationSequenceBlocked"], 2)

    def test_rejects_frozen_moving_animation(self):
        values = [sample(frame, 10.0 + frame) for frame in range(1, 5)]
        for value in values:
            for peer in ("host", "join"):
                entity = value[peer]["entities"][0]
                entity["moving"] = True
                entity["layers"][0]["frame"] = 1
        with self.assertRaisesRegex(Failure, "frozen moving animation"):
            analyze_render_samples(values)

    def test_failure_evidence_bundle_has_required_ledgers(self):
        evidence = {
            "relays": ["wss://example.invalid"],
            "host": {"publicKey": "a", "game": {"currentTick": 2}},
            "join": {"publicKey": "b", "game": {"currentTick": 2}},
            "actions": [],
            "recovery": {},
            "hostConsole": [],
            "joinConsole": [],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_audit_bundle(root, evidence)
            for relative in (
                "run.json", "actions.jsonl", "transport.jsonl",
                "states/host.jsonl", "states/join.jsonl", "motion.json",
                "sprite-provenance.jsonl", "console-host.json",
                "console-join.json", "visual-failures.json",
                "visual-findings.json",
            ):
                self.assertTrue((root / relative).exists(), relative)
            self.assertFalse((root / "first-failure.json").exists())


if __name__ == "__main__":
    unittest.main()

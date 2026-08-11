#!/usr/bin/env python3

import copy
import tempfile
import unittest
from pathlib import Path

from nostr_multiplayer_smoke_test import (
    Failure,
    analyze_render_samples,
    audited_key,
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

    def test_rejects_unproved_render_source(self):
        with self.assertRaisesRegex(Failure, "unproved production"):
            analyze_render_samples([sample(1, 10.0, "procedural_or_unproven")])

    def test_accepts_contractual_procedural_effect(self):
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
        result = analyze_render_samples([value])
        self.assertEqual(result["intentionalProcedural"], 2)

    def test_rejects_procedural_without_geometry(self):
        value = sample(1, 10.0, "intentional_procedural")
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["expectedAssetStatus"] = "intentional_procedural"
            entity["expectedSourceMapping"] = "generic-impact-contract"
        with self.assertRaisesRegex(Failure, "lacks provenance"):
            analyze_render_samples([value])

    def test_rejects_peer_primitive_divergence(self):
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
        with self.assertRaisesRegex(Failure, "primitive divergence"):
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

    def test_records_unresolved_expected_mapping_without_stopping_gameplay(self):
        value = sample(1, 10.0)
        value["host"]["entities"][0]["expectedAssetStatus"] = "missing_mapping"
        result = analyze_render_samples([value])
        self.assertEqual(len(result["unresolvedExpectedMappings"]), 1)
        self.assertEqual(
            result["unresolvedExpectedMappings"][0]["reason"],
            "expected asset is not renderable",
        )

    def test_records_empty_renderable_mapping_without_stopping_gameplay(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["expectedResourceIds"] = []
        result = analyze_render_samples([value])
        self.assertEqual(len(result["unresolvedExpectedMappings"]), 2)

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
                "console-join.json",
            ):
                self.assertTrue((root / relative).exists(), relative)
            self.assertFalse((root / "first-failure.json").exists())


if __name__ == "__main__":
    unittest.main()

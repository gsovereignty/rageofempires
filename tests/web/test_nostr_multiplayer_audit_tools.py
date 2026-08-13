#!/usr/bin/env python3

import copy
import base64
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image

from nostr_multiplayer_smoke_test import (
    CDP_SHIFT_MODIFIER,
    ActionLimitReached,
    BoundedActionLog,
    Failure,
    allocate_audit_destination,
    analyze_render_samples,
    analyze_render_samples_for_audit,
    audited_key,
    audited_zoom,
    banked_resource_increased,
    capture_failure_value,
    capture_browser_overlap,
    capture_catalog_semantic_pixels,
    click_canvas_logical,
    canonical_direction_route,
    deterministic_replacement_destination,
    failure_bundle_evidence,
    canonical_transition_routes,
    catalog_ids_for_entity,
    collapse_match_details,
    diagnostics,
    initialize_run_ledger,
    negotiate_game_speed,
    parse_viewport,
    probe_relay_pool,
    render_diagnostics,
    request_correlated_pixel_capture,
    replayable_action_stream,
    relay_blocker_from_diagnostics,
    selectable_military_id,
    visual_failures,
    visual_findings,
    wait_for_drawable_direction,
    write_audit_bundle,
)


class VirtualFsDriver:
    def __init__(self, files):
        self.files = files

    def execute_script(self, script, path):
        value = self.files[path]
        if "encoding: 'utf8'" in script:
            return value.decode()
        return base64.b64encode(value).decode()


class PixelCaptureDriver(VirtualFsDriver):
    def __init__(self, files):
        super().__init__(files)
        self.complete = None

    def execute_script(self, script, *arguments):
        if not arguments and "browserRenderTelemetry" in script:
            return {
                "tick": 12, "frame": 34,
                "entities": [{
                    "id": 7, "facing": 1,
                    "previousPosition": {"x": 2, "y": 2},
                    "simulationPosition": {"x": 2, "y": 3},
                    "destination": {"x": 2, "y": 6},
                }],
            }
        if not arguments and "browserNostrDiagnostics" in script:
            return {"game": {
                "currentTick": 12, "stateHash": "hash-12",
            }}
        path = arguments[0]
        if "browserPixelCaptureRequest =" in script:
            self.complete = path
            return None
        if "browserPixelCaptureComplete ===" in script:
            return self.complete == path
        return super().execute_script(script, path)


def encoded_image(format_name, mode="RGB"):
    output = io.BytesIO()
    Image.new(mode, (4, 4), ((20, 50, 10, 255) if mode == "RGBA"
                             else (20, 50, 10))).save(output, format=format_name)
    return output.getvalue()


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
    def test_non_active_reliability_is_infrastructure_blocker(self):
        blocker = relay_blocker_from_diagnostics(
            {"game": {"reliabilityStatus": 1, "reliabilityReason": 7}},
            {"game": {"reliabilityStatus": 2, "reliabilityReason": 1}},
        )
        self.assertEqual(
            blocker["classification"], "public-relay-infrastructure"
        )
        self.assertEqual(blocker["peers"]["join"], {
            "status": 2, "reason": 1,
        })

    def test_active_reliability_is_not_infrastructure_blocker(self):
        state = {"game": {
            "reliabilityStatus": 0, "reliabilityReason": 0,
        }}
        self.assertIsNone(relay_blocker_from_diagnostics(state, state))

    def test_rejected_publish_quorum_blocks_even_while_active(self):
        state = {"game": {
            "reliabilityStatus": 0, "reliabilityReason": 0,
        }, "recentPublications": [{
            "intentId": "lobby-2", "results": [
                {"relay": "one", "ok": False, "message": "rejected"},
                {"relay": "two", "ok": False, "message": "unsupported"},
                {"relay": "three", "ok": True, "message": ""},
            ],
        }]}
        blocker = relay_blocker_from_diagnostics(state, state)
        self.assertEqual(
            blocker["rejectedPublications"]["host"][0]
            ["acceptedRelayCount"], 1,
        )

    def test_game_speed_cycles_until_exact_shared_target(self):
        host = object()
        join = object()
        speed = {"value": 0}

        def diagnostics(_driver):
            return {"gameSpeed": speed["value"]}

        def chord(_driver, _key):
            speed["value"] = (speed["value"] + 1) % 3

        with patch(
            "nostr_multiplayer_smoke_test.game_diagnostics", diagnostics,
        ), patch(
            "nostr_multiplayer_smoke_test.key_chord", side_effect=chord,
        ) as key:
            negotiate_game_speed(host, join, 2)

        self.assertEqual(speed["value"], 2)
        self.assertEqual(key.call_count, 2)

    def test_zoom_dispatches_wheel_at_canvas_center(self):
        class Journey:
            def __init__(self):
                self.calls = 0

            def telemetry(self):
                self.calls += 1
                return {
                    "tick": 4,
                    "camera": {"zoom": 1.25 if self.calls < 3 else 1.0},
                }

        class Canvas:
            rect = {"x": 10, "y": 20, "width": 100, "height": 60}

        class Driver:
            def __init__(self):
                self.events = []

            def find_element(self, *_):
                return Canvas()

            def execute_cdp_cmd(self, name, event):
                self.events.append((name, event))

        driver = Driver()
        actions = []
        with patch("nostr_multiplayer_smoke_test.time.sleep"):
            audited_zoom(Journey(), driver, actions, "host", 1.0)

        wheel = driver.events[1]
        self.assertEqual(wheel[0], "Input.dispatchMouseEvent")
        self.assertEqual(wheel[1]["type"], "mouseWheel")
        self.assertEqual((wheel[1]["x"], wheel[1]["y"]), (60.0, 50.0))
        self.assertEqual(wheel[1]["deltaY"], 100)

    def test_bounded_action_log_stops_before_post_prefix_action(self):
        actions = BoundedActionLog(2)
        actions.append({"kind": "first"})
        actions.append({"kind": "second"})
        with self.assertRaisesRegex(ActionLimitReached, "action limit 2"):
            actions.append({"kind": "third"})
        self.assertEqual([value["kind"] for value in actions],
                         ["first", "second"])

    def test_shift_click_uses_cdp_shift_bit(self):
        class Canvas:
            rect = {"x": 0, "y": 0, "width": 1280, "height": 720}

        class Driver:
            def __init__(self):
                self.events = []

            def find_element(self, *_):
                return Canvas()

            def execute_cdp_cmd(self, name, event):
                self.events.append((name, event))

        driver = Driver()
        click_canvas_logical(
            driver, 100, 200, modifiers=CDP_SHIFT_MODIFIER
        )
        self.assertEqual([event[1]["modifiers"] for event in driver.events],
                         [8, 8])

    def test_replayable_action_stream_retains_order_and_relative_timing(self):
        actions = [
            {"monotonic": 10.0, "kind": "key", "telemetryTick": 4},
            {"monotonic": 10.125, "kind": "world-pointer",
             "telemetryTick": 5},
            {"kind": "terminal"},
        ]
        result = replayable_action_stream(actions)
        self.assertEqual([item["sequence"] for item in result], [0, 1, 2])
        self.assertEqual(result[0]["elapsedFromStartMs"], 0.0)
        self.assertEqual(result[1]["elapsedFromStartMs"], 125.0)
        self.assertEqual(result[1]["elapsedFromPreviousMs"], 125.0)
        self.assertIsNone(result[2]["elapsedFromStartMs"])
        self.assertEqual(result[1]["telemetryTick"], 5)

    def test_drawable_direction_uses_interpolation_after_new_step(self):
        host = object()
        join = object()

        def state(_driver):
            return {"entities": [{
                "id": 7, "owner": 0, "moving": False,
                "interpolating": True,
                "previousPosition": {"x": 10, "y": 10},
                "simulationPosition": {"x": 11, "y": 11},
            }]}

        with patch("nostr_multiplayer_smoke_test.render_diagnostics", state):
            wait_for_drawable_direction(
                host, join, owner=0, entity_id=7, direction=0,
                baseline_position=(10, 10),
            )

    def test_exports_live_renderer_overlap_inputs_as_png_manifest(self):
        manifest = {"cases": [{
            "id": "unit-7", "actual": "actual.bmp", "terrain": "terrain.bmp",
            "sprite": "unit-7.tga", "x": 12, "y": 23,
            "metadata": {"entity_id": 7},
        }]}
        driver = VirtualFsDriver({
            "/audit-overlap/manifest.json": json.dumps(manifest).encode(),
            "/audit-overlap/actual.bmp": encoded_image("BMP"),
            "/audit-overlap/terrain.bmp": encoded_image("BMP"),
            "/audit-overlap/unit-7.tga": encoded_image("TGA", "RGBA"),
        })
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            count = capture_browser_overlap(driver, root, "host")

            self.assertEqual(count, 1)
            case = json.loads(
                (root / "overlap" / "manifest.json").read_text()
            )["cases"][0]
            self.assertEqual((case["x"], case["y"]), (12, 23))
            self.assertEqual(Image.open(root / "overlap" /
                                        case["sprite"]).mode, "RGBA")

    def test_requests_correlated_exact_capture_and_filters_entity(self):
        manifest = {"cases": [
            {
                "id": "unit-7", "sprite": "unit-7.tga", "x": 12,
                "y": 23, "metadata": {"entity_id": 7},
            },
            {
                "id": "unit-8", "sprite": "unit-8.tga", "x": 20,
                "y": 30, "metadata": {"entity_id": 8},
            },
        ]}
        root_path = "/audit-pixels/host-lap-0-direction-1"
        files = {
            f"{root_path}/manifest.json": json.dumps(manifest).encode(),
            f"{root_path}/actual.bmp": encoded_image("BMP"),
            f"{root_path}/terrain.bmp": encoded_image("BMP"),
            f"{root_path}/unit-7.tga": encoded_image("TGA", "RGBA"),
            f"{root_path}/unit-8.tga": encoded_image("TGA", "RGBA"),
        }
        with tempfile.TemporaryDirectory() as directory:
            result = request_correlated_pixel_capture(
                PixelCaptureDriver(files), PixelCaptureDriver(files),
                Path(directory), "host-lap-0-direction-1", 7,
            )
            self.assertEqual(result["entityId"], 7)
            for peer in ("host", "join"):
                manifest_path = Path(directory) / result["peers"][peer][
                    "manifest"
                ]
                cases = json.loads(manifest_path.read_text())["cases"]
                self.assertEqual(len(cases), 1)
                self.assertEqual(cases[0]["metadata"]["entity_id"], 7)

    def test_catalog_pixel_direction_comes_from_captured_motion(self):
        capture = {
            "peers": {
                peer: {
                    "manifest": f"{peer}/manifest.json",
                    "previousPosition": {"x": 2, "y": 2},
                    "currentPosition": {"x": 2, "y": 3},
                    "actualLogicalDirection": 7,
                }
                for peer in ("host", "join")
            }
        }
        manifest = {"cases": [{"metadata": {"sprite_frames": [{
            "direction_count": 8,
        }]}}]}
        retained = {
            "verdict": "PASS", "images": {"actual": "actual.png"},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for peer in ("host", "join"):
                (root / peer).mkdir()
                (root / peer / "manifest.json").write_text(
                    json.dumps(manifest)
                )
            with patch(
                "nostr_multiplayer_smoke_test.request_correlated_pixel_capture",
                return_value=capture,
            ), patch(
                "nostr_multiplayer_smoke_test.evaluate_packaged_capture",
                return_value=retained,
            ) as evaluate:
                result = capture_catalog_semantic_pixels(
                    object(), object(), root, "formation", 7, owner=0,
                    unit_kind="unit-villager", action="formation",
                    catalog_ids=["formation"], phase="formation",
                )

        self.assertIsNotNone(result)
        self.assertEqual(
            [call.kwargs["expected_logical_direction"]
             for call in evaluate.call_args_list],
            [1, 1],
        )
        self.assertEqual(
            [oracle["actualLogicalDirection"]
             for oracle in result["visualOracles"]],
            [7, 7],
        )

    def test_gold_deposit_oracle_waits_for_banked_resource(self):
        carrying = {"resources": {"gold": 200}}
        deposited = {"resources": {"gold": 209}}

        self.assertIsNone(
            banked_resource_increased(carrying, "gold", 200)
        )
        self.assertEqual(
            banked_resource_increased(deposited, "gold", 200), 209
        )

    def test_commanded_military_can_be_selected_by_direct_fallback(self):
        military_ids = {8, 27}

        self.assertEqual(
            selectable_military_id(27, military_ids, set()), 27
        )
        self.assertIsNone(
            selectable_military_id(27, military_ids, {27})
        )
        self.assertIsNone(
            selectable_military_id(5, military_ids, set())
        )

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
    def test_failure_bundle_preserves_actual_completed_ui_actions(self):
        actions = [{"kind": "world-pointer", "tileX": 24, "tileY": 20}]
        merged = failure_bundle_evidence({
            "error": "Failure: drawable direction",
            "completedEvidence": {"actions": actions, "host": {"old": True}},
            "host": {"game": {"currentTick": 644}},
            "hostRender": {"frame": 12810},
        })
        self.assertEqual(merged["actions"], actions)
        self.assertEqual(merged["host"]["game"]["currentTick"], 644)
        self.assertEqual(merged["failureError"],
                         "Failure: drawable direction")

    def test_parses_supported_viewport(self):
        self.assertEqual(parse_viewport("1280x900"), (1280, 900))
        with self.assertRaisesRegex(Exception, "WIDTHxHEIGHT"):
            parse_viewport("wide")
        with self.assertRaisesRegex(Exception, "supported minimum"):
            parse_viewport("320x200")

    def test_relay_probe_preserves_configured_order_for_quorum(self):
        class Socket:
            def close(self):
                pass

        def connector(relay, timeout):
            self.assertEqual(timeout, 0.25)
            if relay.endswith("bad"):
                raise OSError("unreachable")
            return Socket()

        report = probe_relay_pool(
            ["wss://one/", "wss://bad", "wss://two", "wss://three",
             "wss://four", "wss://one"],
            timeout=0.25, connector=connector,
        )
        self.assertEqual(report["selectedQuorum"], [
            "wss://one", "wss://two", "wss://three",
        ])
        self.assertEqual(len(report["results"]), 5)
        failed = next(
            result for result in report["results"]
            if result["relay"] == "wss://bad"
        )
        self.assertFalse(failed["healthy"])
        self.assertIn("OSError", failed["error"])

    def test_allocates_durable_contract_before_browser_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination = allocate_audit_destination(
                root / "artifacts", root / "reports"
            )
            initialize_run_ledger(
                destination, relays="wss://one.example,wss://two.example",
                headed=False, port=8888, seed=42, retry_budget=3,
            )
            self.assertRegex(
                destination.artifacts.name,
                r"^\d{8}T\d{6}Z-[0-9a-f]{12}$",
            )
            self.assertTrue(destination.report.is_file())
            for name in (
                "run.json", "actions.jsonl", "correlated-frames.jsonl",
                "visual-oracles.jsonl", "coverage.json", "verdict.json",
            ):
                self.assertTrue((destination.artifacts / name).is_file(), name)
            ledger = json.loads(
                (destination.artifacts / "run.json").read_text()
            )
            self.assertEqual(ledger["status"], "RUNNING")
            self.assertFalse(ledger["privateKeysRetained"])

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
    def test_stuck_action_replacement_is_seeded_and_changes_target(self):
        first = deterministic_replacement_destination(
            (20, 16), (21, 16), 42, owner=0,
        )
        self.assertNotEqual(first, (21, 16))
        self.assertEqual(
            first,
            deterministic_replacement_destination(
                (20, 16), (21, 16), 42, owner=0,
            ),
        )
        self.assertLessEqual(abs(first[0] - 20), 1)
        self.assertLessEqual(abs(first[1] - 16), 1)

    def test_formation_and_patrol_catalog_use_authoritative_fields(self):
        self.assertEqual(
            catalog_ids_for_entity({
                "category": "unit-militia", "action": "moving",
                "formationGroupId": 9, "patrolling": True,
                "attackMoving": True, "chasing": True,
            }),
            [
                "attack-movement", "chase", "formation",
                "infantry-before-upgrade", "patrol",
            ],
        )

    def test_canonical_route_covers_all_directions_and_closes(self):
        route = canonical_direction_route((20, 16), radius=4)
        self.assertEqual(route[0], route[-1])
        self.assertEqual(len(route), 9)
        vectors = [
            (current[0] - previous[0], current[1] - previous[1])
            for previous, current in zip(route, route[1:])
        ]
        self.assertEqual(vectors, [
            (4, 4), (0, 4), (-4, 4), (-4, 0),
            (-4, -4), (0, -4), (4, -4), (4, 0),
        ])

    def test_canonical_route_honors_seeded_direction_order_and_closes(self):
        order = [3, 2, 1, 0, 7, 6, 5, 4]
        route = canonical_direction_route(
            (20, 16), radius=4, direction_order=order,
        )
        vectors = [
            (current[0] - previous[0], current[1] - previous[1])
            for previous, current in zip(route, route[1:])
        ]
        canonical = [
            (4, 4), (0, 4), (-4, 4), (-4, 0),
            (-4, -4), (0, -4), (4, -4), (4, 0),
        ]
        self.assertEqual(vectors, [canonical[direction] for direction in order])
        self.assertEqual(route[0], route[-1])

    def test_transition_routes_cover_turn_shapes_and_close_loops(self):
        routes = canonical_transition_routes((20, 16), radius=2)
        self.assertEqual(set(routes), {
            "right-angle", "u-turn", "zigzag",
            "clockwise-loop", "counter-clockwise-loop",
            "queued-waypoints",
        })
        self.assertEqual(routes["u-turn"][0], routes["u-turn"][-1])
        self.assertEqual(
            routes["clockwise-loop"][0],
            routes["clockwise-loop"][-1],
        )
        self.assertEqual(
            routes["counter-clockwise-loop"][0],
            routes["counter-clockwise-loop"][-1],
        )
        zigzag_vectors = [
            (right[0] - left[0], right[1] - left[1])
            for left, right in zip(
                routes["zigzag"], routes["zigzag"][1:]
            )
        ]
        self.assertEqual(zigzag_vectors, [(2, 2), (2, -2), (2, 2)])

    def test_accepts_monotonic_legacy_motion(self):
        result = analyze_render_samples([sample(1, 10.0), sample(2, 14.0)])
        self.assertEqual(result["frames"], 2)
        self.assertEqual(result["legacy"], 4)
        self.assertEqual(result["maximumFrameDisplacement"], 4.0)

    def test_ignores_expected_asset_wholly_outside_viewport(self):
        value = sample(1, 10.0)
        offscreen = {
            "id": 99,
            "owner": 1,
            "category": "unit-scout_cavalry",
            "source": "procedural_or_unproven",
            "expectedAssetStatus": "renderable",
            "expectedResourceIds": [2085],
            "layers": [],
            "renderPosition": None,
        }
        for peer in ("host", "join"):
            value[peer]["entities"].append(dict(offscreen))
        result = analyze_render_samples([value])
        self.assertEqual(result["offscreenEntities"], 2)
        self.assertEqual(result["unprovenSources"], 0)

    def test_accepts_actual_selection_overlay_submission(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["layers"][0]["drawOrder"] = 3
            entity["selectionOverlay"] = {
                "center": {"x": 10.0, "y": 21.0},
                "halfWidth": 15.0, "halfHeight": 6.3,
                "color": {"r": 250, "g": 220, "b": 65, "a": 255},
                "shadowDrawOrder": 4, "markerDrawOrder": 5,
            }
        result = analyze_render_samples([value])
        self.assertEqual(result["selectionOverlayAssertions"], 2)

    def test_rejects_selection_overlay_at_wrong_position(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["layers"][0]["drawOrder"] = 3
            entity["selectionOverlay"] = {
                "center": {"x": 14.0, "y": 21.0},
                "halfWidth": 15.0, "halfHeight": 6.3,
                "color": {"r": 250, "g": 220, "b": 65, "a": 255},
                "shadowDrawOrder": 4, "markerDrawOrder": 5,
            }
        with self.assertRaisesRegex(Failure, "selection overlay draw mismatch"):
            analyze_render_samples([value])

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

    def test_rejects_correct_facing_with_wrong_physical_frame(self):
        value = moving_sample(1, 1, 0)
        for peer in ("host", "join"):
            layer = value[peer]["entities"][0]["layers"][0]
            layer.update({
                "framesPerDirection": 2,
                "physicalFrameCount": 10,
                "mirroringMode": 6,
                "actionFrame": 1,
                "frame": 7,
                "flipHorizontal": True,
            })
        with self.assertRaisesRegex(Failure, "physical frame/flip mismatch"):
            analyze_render_samples([value])

    def test_rejects_correct_facing_with_wrong_horizontal_flip(self):
        value = moving_sample(1, 1, 0)
        for peer in ("host", "join"):
            layer = value[peer]["entities"][0]["layers"][0]
            layer.update({
                "framesPerDirection": 2,
                "physicalFrameCount": 10,
                "mirroringMode": 6,
                "actionFrame": 1,
                "frame": 5,
                "flipHorizontal": False,
            })
        with self.assertRaisesRegex(Failure, "physical frame/flip mismatch"):
            analyze_render_samples([value])

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

    def test_requires_actual_draw_submission_rectangles(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            value[peer]["schemaVersion"] = 1
            layer = value[peer]["entities"][0]["layers"][0]
            layer.update({
                "drawOrder": 3,
                "width": 12,
                "height": 18,
                "sourceRectangle": {"x": 0, "y": 0, "w": 12, "h": 18},
                "destination": {"x": 4, "y": 5, "w": 12, "h": 18},
                "clippedDestination": {
                    "x": 4, "y": 5, "w": 10, "h": 16,
                },
            })
        self.assertEqual(analyze_render_samples([value])["frames"], 1)
        value["join"]["entities"][0]["layers"][0][
            "clippedDestination"
        ]["w"] = 13
        with self.assertRaisesRegex(Failure, "draw submission telemetry"):
            analyze_render_samples([value])

    def test_allows_frame_phase_difference_at_distinct_interpolation_points(self):
        value = sample(1, 10.0)
        value["host"]["movementAlpha"] = 0.2
        value["join"]["movementAlpha"] = 0.7
        value["host"]["entities"][0]["layers"][0]["actionFrame"] = 1
        value["join"]["entities"][0]["layers"][0]["actionFrame"] = 2
        value["join"]["entities"][0]["layers"][0]["frame"] = 2
        result = analyze_render_samples([value])
        self.assertEqual(result["frames"], 1)

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
                "correlated-frames.jsonl", "visual-oracles.jsonl",
                "coverage.json", "verdict.json",
                "states/host.jsonl", "states/join.jsonl", "motion.json",
                "sprite-provenance.jsonl", "console-host.json",
                "console-join.json", "visual-failures.json",
                "visual-findings.json", "screenshot-audit.json",
            ):
                self.assertTrue((root / relative).exists(), relative)
            self.assertFalse((root / "first-failure.json").exists())


if __name__ == "__main__":
    unittest.main()

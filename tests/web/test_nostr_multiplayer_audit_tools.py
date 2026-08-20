#!/usr/bin/env python3

import copy
import base64
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import call, patch

from PIL import Image
from selenium.webdriver.common.keys import Keys

from nostr_multiplayer_smoke_test import (
    CDP_SHIFT_MODIFIER,
    ActionLimitReached,
    BoundedActionLog,
    EmptyPixelCapture,
    Failure,
    InfrastructureBlocked,
    allocate_audit_destination,
    analyze_render_samples,
    analyze_render_samples_for_audit,
    audited_held_key,
    audited_key,
    audited_zoom,
    banked_resource_increased,
    capture_failure_value,
    complete_pixel_oracle_pair,
    capture_initial_gameplay_overlap,
    capture_browser_overlap,
    capture_catalog_semantic_pixels,
    capture_owner_then_visible_observer_pixels,
    capture_route_pixel_or_unsampled,
    capture_until_arrival,
    center_camera_for_tile,
    center_peer_cameras_for_tile,
    click_canvas_logical,
    canonical_direction_route,
    deterministic_replacement_destination,
    failure_bundle_evidence,
    gameplay_launch_url,
    canonical_transition_routes,
    catalog_ids_for_entity,
    command_acceptance_status,
    collapse_match_details,
    diagnostics,
    evaluate_packaged_capture,
    entity_arrived_on_both_peers,
    initialize_run_ledger,
    load_default_relays,
    relay_pool_digest,
    negotiate_game_speed,
    narrow_passage_egress,
    validate_cand003_route_state,
    parse_viewport,
    PostCameraDirectionExpired,
    probe_relay_pool,
    prepare_correlated_entity_capture,
    render_diagnostics,
    request_correlated_pixel_capture,
    request_browser_overlap,
    request_prepared_correlated_pixel_capture,
    replayable_action_stream,
    relay_blocker_from_diagnostics,
    recapture_attempt_record,
    select_route_unit_at_current_position,
    selectable_military_id,
    visual_failures,
    visual_findings,
    wait_for_drawable_direction,
    write_audit_bundle,
    write_overlap_checkpoint,
)


class NarrowPassageTests(unittest.TestCase):
    def test_each_owner_exits_shared_destination_on_own_side(self):
        self.assertEqual(narrow_passage_egress(0), (19, 8))
        self.assertEqual(narrow_passage_egress(1), (30, 8))
        self.assertNotEqual(narrow_passage_egress(0), (24, 8))
        self.assertNotEqual(narrow_passage_egress(1), (24, 8))

    def test_unknown_owner_has_no_unsafe_default_egress(self):
        with self.assertRaisesRegex(ValueError, "owner must be 0 or 1"):
            narrow_passage_egress(2)

    def test_post_egress_requires_converged_alive_idle_units(self):
        units = [
            {"id": 10, "x": 30, "y": 8, "hitPoints": 25,
             "attackTargetId": 0},
            {"id": 1, "x": 19, "y": 8, "hitPoints": 25,
             "attackTargetId": 0},
        ]
        game = {
            "currentTick": 80, "stateHash": "same",
            "blueContiguous": 12, "redContiguous": 12,
            "blueMissing": [], "redMissing": [], "units": units,
        }
        result = validate_cand003_route_state(
            [copy.deepcopy(game), copy.deepcopy(game)], {1: 10, 0: 1}, 0,
        )
        self.assertEqual(result["currentPosition"], {"x": 19, "y": 8})
        attacking = copy.deepcopy(game)
        attacking["units"][0]["attackTargetId"] = 1
        with self.assertRaisesRegex(Failure, "retained attack"):
            validate_cand003_route_state(
                [attacking, copy.deepcopy(attacking)], {1: 10, 0: 1}, 0,
            )

    def test_post_egress_rejects_peer_divergence_and_death(self):
        game = {
            "currentTick": 80, "stateHash": "same",
            "blueContiguous": 12, "redContiguous": 12,
            "blueMissing": [], "redMissing": [],
            "units": [{"id": 10, "x": 30, "y": 8, "hitPoints": 25,
                       "attackTargetId": 0}],
        }
        divergent = copy.deepcopy(game)
        divergent["stateHash"] = "different"
        with self.assertRaisesRegex(Failure, "peer divergence"):
            validate_cand003_route_state([game, divergent], {1: 10}, 1)
        dead = copy.deepcopy(game)
        dead["units"][0]["hitPoints"] = 0
        with self.assertRaisesRegex(Failure, "died"):
            validate_cand003_route_state([dead, copy.deepcopy(dead)], {1: 10}, 1)


class PixelOraclePairTests(unittest.TestCase):
    def test_hidden_peer_does_not_form_mutation_pair(self):
        self.assertIsNone(complete_pixel_oracle_pair([
            {"peer": "host", "verdict": "PASS"},
        ]))

    def test_visible_peers_are_keyed_independent_of_capture_order(self):
        join = {"peer": "join", "verdict": "PASS"}
        host = {"peer": "host", "verdict": "PASS"}

        self.assertEqual(
            complete_pixel_oracle_pair([join, host]),
            {"host": host, "join": join},
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
    def test_direct_relay_reliability_is_infrastructure_blocker(self):
        blocker = relay_blocker_from_diagnostics(
            {"game": {"reliabilityStatus": 1, "reliabilityReason": 7}},
            {"game": {"reliabilityStatus": 2, "reliabilityReason": 5}},
        )
        self.assertEqual(
            blocker["classification"], "public-relay-infrastructure"
        )
        self.assertEqual(blocker["peers"]["join"], {
            "status": 2, "reason": 5,
        })

    def test_peer_silence_alone_is_not_attributed_to_relays(self):
        host = {"game": {
            "reliabilityStatus": 0, "reliabilityReason": 0,
        }}
        join = {"game": {
            "reliabilityStatus": 2, "reliabilityReason": 1,
        }}
        self.assertIsNone(relay_blocker_from_diagnostics(host, join))

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

    def test_shift_click_holds_real_shift_across_mouse_press(self):
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
        with patch("nostr_multiplayer_smoke_test.time.sleep") as sleep:
            click_canvas_logical(
                driver, 100, 200, modifiers=CDP_SHIFT_MODIFIER
            )
        self.assertEqual([
            (name, event["type"])
            for name, event in driver.events
        ], [
            ("Input.dispatchKeyEvent", "keyDown"),
            ("Input.dispatchMouseEvent", "mousePressed"),
            ("Input.dispatchMouseEvent", "mouseReleased"),
            ("Input.dispatchKeyEvent", "keyUp"),
        ])
        self.assertEqual(
            [event["modifiers"] for _, event in driver.events[:3]],
            [CDP_SHIFT_MODIFIER] * 3,
        )
        self.assertEqual(sleep.call_args_list, [
            call(0.05), call(0.05), call(0.05),
        ])

    def test_shift_right_click_preserves_requested_mouse_button(self):
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
        with patch("nostr_multiplayer_smoke_test.time.sleep"):
            click_canvas_logical(
                driver, 100, 200, button=2,
                modifiers=CDP_SHIFT_MODIFIER,
            )
        mouse_events = [
            event for name, event in driver.events
            if name == "Input.dispatchMouseEvent"
        ]
        self.assertEqual(
            [(event["button"], event["buttons"])
             for event in mouse_events],
            [("right", 2), ("right", 0)],
        )

    def test_shift_click_releases_shift_after_mouse_failure(self):
        class Canvas:
            rect = {"x": 0, "y": 0, "width": 1280, "height": 720}

        class Driver:
            def __init__(self):
                self.events = []

            def find_element(self, *_):
                return Canvas()

            def execute_cdp_cmd(self, name, event):
                self.events.append((name, event))
                if event["type"] == "mousePressed":
                    raise RuntimeError("mouse dispatch failed")

        driver = Driver()
        with self.assertRaisesRegex(RuntimeError, "mouse dispatch failed"):
            click_canvas_logical(
                driver, 100, 200, modifiers=CDP_SHIFT_MODIFIER
            )
        self.assertEqual(driver.events[-1][0], "Input.dispatchKeyEvent")
        self.assertEqual(driver.events[-1][1]["type"], "keyUp")

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

    def test_drawable_direction_accepts_settled_command_endpoint(self):
        host = object()
        join = object()

        def state(_driver):
            return {"entities": [{
                "id": 9, "owner": 1, "moving": False,
                "interpolating": False,
                "previousPosition": {"x": 37, "y": 5},
                "simulationPosition": {"x": 36, "y": 4},
            }]}

        with patch("nostr_multiplayer_smoke_test.render_diagnostics", state):
            wait_for_drawable_direction(
                host, join, owner=1, entity_id=9, direction=4,
                baseline_position=(40, 8),
            )

    def test_drawable_direction_ignores_opponent_fog_omission(self):
        host = object()
        join = object()

        def state(driver):
            if driver is host:
                return {"entities": []}
            return {"entities": [{
                "id": 9, "owner": 1, "moving": False,
                "interpolating": False,
                "previousPosition": {"x": 37, "y": 5},
                "simulationPosition": {"x": 36, "y": 4},
            }]}

        with patch("nostr_multiplayer_smoke_test.render_diagnostics", state):
            wait_for_drawable_direction(
                host, join, owner=1, entity_id=9, direction=4,
                baseline_position=(40, 8),
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

    def test_requests_overlap_only_after_explicit_gameplay_capture(self):
        manifest = {"cases": [{
            "id": "unit-7", "sprite": "unit-7.tga", "x": 12,
            "y": 23, "metadata": {"entity_id": 7},
        }]}
        files = {
            "/audit-overlap/manifest.json": json.dumps(manifest).encode(),
            "/audit-overlap/actual.bmp": encoded_image("BMP"),
            "/audit-overlap/terrain.bmp": encoded_image("BMP"),
            "/audit-overlap/unit-7.tga": encoded_image("TGA", "RGBA"),
        }
        driver = PixelCaptureDriver(files)
        with tempfile.TemporaryDirectory() as directory:
            count = request_browser_overlap(
                driver, Path(directory), "host"
            )

        self.assertEqual(count, 1)
        self.assertEqual(driver.complete, "/audit-overlap")

    def test_launch_url_never_requests_tick_zero_overlap_capture(self):
        url = gameplay_launch_url("http://127.0.0.1:8892")

        self.assertEqual(
            url,
            "http://127.0.0.1:8892/aoe_web.html?scenario=nostr-visual",
        )
        self.assertNotIn("overlapCapture", url)
        self.assertNotIn("overlapTick", url)

    def test_overlap_capture_follows_tick_gate_and_both_panel_hides(self):
        host = object()
        join = object()
        events: list[str] = []

        def game(driver):
            events.append("tick-host" if driver is host else "tick-join")
            return {"currentTick": 8}

        def key(driver, _actions, actor, value):
            self.assertEqual(value, Keys.F4)
            events.append(f"hide-{actor}")

        def request(driver, _root, peer):
            events.append(f"capture-{peer}")
            return 10

        with patch(
            "nostr_multiplayer_smoke_test.game_diagnostics", side_effect=game,
        ), patch(
            "nostr_multiplayer_smoke_test.audited_key", side_effect=key,
        ), patch(
            "nostr_multiplayer_smoke_test.request_browser_overlap",
            side_effect=request,
        ):
            result = capture_initial_gameplay_overlap(
                host, join, Path("unused"), []
            )

        self.assertEqual(result, {"host": 10, "join": 10})
        self.assertEqual(events, [
            "tick-host", "tick-join", "hide-host", "hide-join",
            "capture-host", "capture-join",
        ])

    def test_overlap_checkpoint_persists_peer_lockstep_proof(self):
        host = object()
        join = object()
        states = {
            host: {"publicKey": "host-key", "game": {
                "currentTick": 17, "stateHash": "same-hash",
                "blueContiguous": 4, "redContiguous": 3,
                "blueMissing": [], "redMissing": [],
            }},
            join: {"publicKey": "join-key", "game": {
                "currentTick": 17, "stateHash": "same-hash",
                "blueContiguous": 4, "redContiguous": 3,
                "blueMissing": [], "redMissing": [],
            }},
        }
        with tempfile.TemporaryDirectory() as directory, patch(
            "nostr_multiplayer_smoke_test.diagnostics",
            side_effect=lambda driver: states[driver],
        ):
            checkpoint = write_overlap_checkpoint(
                Path(directory), host, join, {"host": 10, "join": 10}
            )
            stored = json.loads(
                (Path(directory) / "overlap-checkpoint.json").read_text()
            )

        self.assertEqual(stored, checkpoint)
        self.assertEqual(stored["status"], "FOCUSED_PASS")
        self.assertNotEqual(stored["hostPublicKey"],
                            stored["joinPublicKey"])
        self.assertEqual(stored["host"]["stateHash"],
                         stored["join"]["stateHash"])

    def test_prepares_both_divergent_cameras_before_correlated_capture(self):
        host = object()
        join = object()
        host_journey = object()
        join_journey = object()
        games = [{"units": [{
            "id": 7, "owner": 0, "x": 18, "y": 11,
        }]} for _ in range(2)]

        def rendered(driver):
            return {"entities": [{
                "id": 7,
                "layers": [{"visible": True}],
                "cameraMarker": "host" if driver is host else "join",
            }]}

        with patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ), patch(
            "nostr_multiplayer_smoke_test.center_camera_for_tile",
        ) as center, patch(
            "nostr_multiplayer_smoke_test.wait_for_drawable_direction",
        ) as drawable, patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            side_effect=rendered,
        ):
            position = prepare_correlated_entity_capture(
                host_journey, host, "host",
                join_journey, join, "join",
                host, join, [], owner=0, entity_id=7, direction=3,
                baseline_position=(17, 11),
            )

        self.assertEqual(position, (18, 11))
        self.assertEqual(center.call_args_list, [
            unittest.mock.call(host_journey, host, [], "host", 18, 11),
            unittest.mock.call(join_journey, join, [], "join", 18, 11),
        ])
        drawable.assert_called_once_with(
            host, join, owner=0, entity_id=7, direction=3,
            baseline_position=(17, 11),
        )

    def test_post_camera_direction_expiry_has_dedicated_type(self):
        games = [{"units": [{
            "id": 7, "owner": 0, "x": 18, "y": 11,
        }]} for _ in range(2)]
        with patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ), patch(
            "nostr_multiplayer_smoke_test.center_camera_for_tile",
        ), patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            return_value={"frame": -1, "entities": []},
        ), patch(
            "nostr_multiplayer_smoke_test.wait_for_drawable_direction",
            side_effect=Failure(
                "timed out waiting for entity 7 drawable direction 3; "
                "last=None"
            ),
        ):
            with self.assertRaisesRegex(
                PostCameraDirectionExpired, "drawable direction 3"
            ):
                prepare_correlated_entity_capture(
                    object(), object(), "host", object(), object(), "join",
                    object(), object(), [], owner=0, entity_id=7,
                    direction=3, baseline_position=(17, 11),
                )

    def test_non_timeout_post_camera_failure_propagates(self):
        games = [{"units": [{
            "id": 7, "owner": 0, "x": 18, "y": 11,
        }]} for _ in range(2)]
        with patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ), patch(
            "nostr_multiplayer_smoke_test.center_camera_for_tile",
        ), patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            return_value={"frame": -1, "entities": []},
        ), patch(
            "nostr_multiplayer_smoke_test.wait_for_drawable_direction",
            side_effect=Failure("render diagnostics failed"),
        ):
            with self.assertRaisesRegex(Failure, "render diagnostics failed"):
                prepare_correlated_entity_capture(
                    object(), object(), "host", object(), object(), "join",
                    object(), object(), [], owner=0, entity_id=7,
                    direction=3, baseline_position=(17, 11),
                )

    def test_retries_empty_capture_after_fresh_visibility_preparation(self):
        captured = {"peers": {"host": {}, "join": {}}}
        with tempfile.TemporaryDirectory() as directory, patch(
            "nostr_multiplayer_smoke_test.prepare_correlated_entity_capture",
            return_value=(18, 11),
        ) as prepare, patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            return_value={"entities": [{
                "id": 7, "layers": [{"visible": True}],
            }]},
        ), patch(
            "nostr_multiplayer_smoke_test.request_correlated_pixel_capture",
            side_effect=[EmptyPixelCapture("host empty"), captured],
        ) as request:
            result = request_prepared_correlated_pixel_capture(
                object(), object(), "host", object(), object(), "join",
                object(), object(), [], Path(directory), "direction-3",
                owner=0, entity_id=7, direction=3,
                baseline_position=(17, 11),
            )

            ledger = json.loads((Path(directory) / result[
                "attemptLedger"
            ]).read_text())

        self.assertEqual(prepare.call_count, 2)
        self.assertEqual([
            call.args[3] for call in request.call_args_list
        ], ["direction-3-exact-attempt-1",
            "direction-3-exact-attempt-2"])
        self.assertEqual(
            [attempt["status"] for attempt in ledger["attempts"]],
            ["EMPTY", "CAPTURED"],
        )

    def test_pixel_capture_excludes_opponent_hidden_by_fog(self):
        host = object()
        join = object()

        def render(driver):
            if driver is host:
                return {"entities": []}
            return {"entities": [{
                "id": 9, "layers": [{"visible": True}],
            }]}

        captured = {"peers": {"join": {}}}
        with tempfile.TemporaryDirectory() as directory, patch(
            "nostr_multiplayer_smoke_test.prepare_correlated_entity_capture",
            return_value=(36, 4),
        ), patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            side_effect=render,
        ), patch(
            "nostr_multiplayer_smoke_test.request_correlated_pixel_capture",
            return_value=captured,
        ) as request:
            result = request_prepared_correlated_pixel_capture(
                object(), host, "join", object(), join, "host",
                host, join, [], Path(directory), "direction-4",
                owner=1, entity_id=9, direction=4,
                baseline_position=(40, 8),
            )

        self.assertEqual(result, {
            **captured,
            "attempts": [{
                "attempt": 1,
                "requestLabel": "direction-4-exact-attempt-1",
                "position": {"x": 36, "y": 4},
                "status": "CAPTURED",
            }],
            "attemptLedger": (
                "pixel-oracle/direction-4/capture-attempts.json"
            ),
        })
        self.assertEqual(request.call_args.kwargs["peers"], ("join",))

    def test_exhausted_empty_capture_attempts_remain_a_failure(self):
        with tempfile.TemporaryDirectory() as directory, patch(
            "nostr_multiplayer_smoke_test.prepare_correlated_entity_capture",
            return_value=(18, 11),
        ) as prepare, patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            return_value={"entities": [{
                "id": 7, "layers": [{"visible": True}],
            }]},
        ), patch(
            "nostr_multiplayer_smoke_test.request_correlated_pixel_capture",
            side_effect=[EmptyPixelCapture(f"empty {attempt}")
                         for attempt in range(3)],
        ) as request:
            with self.assertRaisesRegex(EmptyPixelCapture, "empty 2"):
                request_prepared_correlated_pixel_capture(
                    object(), object(), "host", object(), object(), "join",
                    object(), object(), [], Path(directory), "direction-3",
                    owner=0, entity_id=7, direction=3,
                    baseline_position=(17, 11),
                )
            ledger = json.loads((
                Path(directory) / "pixel-oracle" / "direction-3" /
                "capture-attempts.json"
            ).read_text())

        self.assertEqual(prepare.call_count, 3)
        self.assertEqual(request.call_count, 3)
        self.assertEqual(
            [attempt["status"] for attempt in ledger["attempts"]],
            ["EMPTY", "EMPTY", "EMPTY"],
        )

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

    def test_catalog_pixel_capture_accepts_only_visible_owner_peer(self):
        capture = {"peers": {"join": {
            "manifest": "join/manifest.json",
            "previousPosition": {"x": 2, "y": 2},
            "currentPosition": {"x": 2, "y": 3},
            "actualLogicalDirection": 1,
        }}}
        manifest = {"cases": [{"metadata": {"sprite_frames": [{
            "direction_count": 8,
        }]}}]}
        retained = {
            "verdict": "PASS", "images": {"actual": "actual.png"},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            host = object()
            join = object()
            (root / "join").mkdir()
            (root / "join" / "manifest.json").write_text(
                json.dumps(manifest)
            )
            with patch(
                "nostr_multiplayer_smoke_test.request_correlated_pixel_capture",
                return_value=capture,
            ) as request, patch(
                "nostr_multiplayer_smoke_test.evaluate_packaged_capture",
                return_value=retained,
            ):
                result = capture_catalog_semantic_pixels(
                    host, join, root, "formation", 9, owner=1,
                    unit_kind="unit-villager", action="formation",
                    catalog_ids=["formation"], phase="formation",
                    peers=("join",),
                )

        request.assert_called_once_with(
            host, join, root, "formation", 9, peers=("join",),
        )
        self.assertEqual(
            [oracle["peer"] for oracle in result["visualOracles"]],
            ["join"],
        )

    def test_formation_pixels_capture_owner_before_fresh_observer(self):
        owner_capture = {
            "peers": {"join": {}}, "visualOracles": [{"peer": "join"}],
        }
        observer_capture = {
            "peers": {"host": {}}, "visualOracles": [{"peer": "host"}],
        }
        renders = iter((
            {"frame": 10},
            {"frame": 11, "entities": [{
                "id": 9, "layers": [{"visible": True}],
            }]},
            {"frame": 11, "entities": [{
                "id": 9, "layers": [{"visible": True}],
            }]},
        ))
        with patch(
            "nostr_multiplayer_smoke_test.capture_catalog_semantic_pixels",
            side_effect=(owner_capture, observer_capture),
        ) as capture, patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            side_effect=lambda _driver: next(renders),
        ), patch(
            "nostr_multiplayer_smoke_test.wait_until",
            side_effect=lambda _label, callback, timeout=None: callback(),
        ):
            result = capture_owner_then_visible_observer_pixels(
                object(), object(), Path("root"), "formation", 9,
                actor="join", observer="host", owner=1,
                unit_kind="unit-villager", action="formation",
                catalog_ids=["formation"], phase="formation",
            )

        self.assertEqual(
            [call.args[3] for call in capture.call_args_list],
            ["formation-owner", "formation-observer"],
        )
        self.assertEqual(
            [call.kwargs["peers"] for call in capture.call_args_list],
            [("join",), ("host",)],
        )
        self.assertEqual(result["optionalObserver"]["renderFrame"], 11)
        self.assertEqual(set(result["peers"]), {"host", "join"})

    def test_formation_pixels_skip_fresh_hidden_observer(self):
        owner_capture = {
            "peers": {"join": {}}, "visualOracles": [{"peer": "join"}],
        }
        renders = iter((
            {"frame": 20}, {"frame": 21, "entities": []},
            {"frame": 21, "entities": []},
        ))
        with patch(
            "nostr_multiplayer_smoke_test.capture_catalog_semantic_pixels",
            return_value=owner_capture,
        ) as capture, patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            side_effect=lambda _driver: next(renders),
        ), patch(
            "nostr_multiplayer_smoke_test.wait_until",
            side_effect=lambda _label, callback, timeout=None: callback(),
        ):
            result = capture_owner_then_visible_observer_pixels(
                object(), object(), Path("root"), "formation", 9,
                actor="join", observer="host", owner=1,
                unit_kind="unit-villager", action="formation",
                catalog_ids=["formation"], phase="formation",
            )

        self.assertEqual(capture.call_count, 1)
        self.assertEqual(result["optionalObserver"]["status"], "NOT_VISIBLE")

    def test_formation_pixels_propagate_owner_capture_failure(self):
        with patch(
            "nostr_multiplayer_smoke_test.capture_catalog_semantic_pixels",
            side_effect=Failure("owner exact pixel capture found 0 layers"),
        ), patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
        ) as render:
            with self.assertRaisesRegex(Failure, "owner exact pixel"):
                capture_owner_then_visible_observer_pixels(
                    object(), object(), Path("root"), "formation", 9,
                    actor="join", observer="host", owner=1,
                    unit_kind="unit-villager", action="formation",
                    catalog_ids=["formation"], phase="formation",
                )
        render.assert_not_called()

    def test_direction_change_before_pixel_readback_is_recapturable_block(self):
        manifest = {
            "cases": [{
                "actual": "actual.png",
                "terrain": "terrain.png",
                "metadata": {
                    "entity_id": 10,
                    "tick": 1842,
                    "sprite_frames": [{
                        "resource_id": 123,
                        "frame": 4,
                        "palette_player": 1,
                        "flip_horizontal": False,
                        "visible": True,
                        "ground": [20.0, 30.0],
                        "action_frame": 2,
                        "frames_per_direction": 5,
                        "direction_count": 8,
                        "logical_direction": 7,
                        "mirroring_mode": 1,
                        "physical_frame_count": 25,
                    }],
                },
            }],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest))
            evidence = root / "semantic-direction"
            result = evaluate_packaged_capture(
                manifest_path=manifest_path,
                graphics_drs=root / "graphics.drs",
                interface_drs=root / "interface.drs",
                expected_logical_direction=6,
                evidence_directory=evidence,
            )
            retained = json.loads((evidence / "blocked.json").read_text())

        self.assertEqual(result["verdict"], "BLOCKED")
        self.assertEqual(
            result["reason"],
            "captured-direction-changed-before-pixel-readback",
        )
        self.assertEqual(retained, result)

    def test_final_blocked_recapture_record_has_no_capture_cycle(self):
        capture = {"peers": {"host": {}, "join": {}}}
        record = recapture_attempt_record(
            capture, [{"verdict": "BLOCKED"}],
        )
        capture["recaptureAttempts"] = [record]

        encoded = json.dumps(capture)

        self.assertIsNot(record["capture"], capture)
        self.assertIn('"recaptureAttempts"', encoded)

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
    def test_command_acceptance_distinguishes_absent_accepted_and_arrived(self):
        def game(x, y, destination_x, destination_y, *, waypoints=0):
            return {"units": [{
                "id": 9, "owner": 1, "x": x, "y": y,
                "destinationX": destination_x,
                "destinationY": destination_y,
                "moving": (x, y) != (destination_x, destination_y),
                "waypointCount": waypoints,
            }]}

        absent = [game(34, 8, 34, 8), game(34, 8, 34, 8)]
        accepted = [game(34, 8, 28, 12), game(34, 8, 28, 12)]
        arrived = [game(28, 12, 28, 12), game(28, 12, 28, 12)]
        self.assertIsNone(command_acceptance_status(
            absent, owner=1, unit_id=9, destination=(28, 12)
        ))
        self.assertEqual(command_acceptance_status(
            accepted, owner=1, unit_id=9, destination=(28, 12)
        ), "accepted")
        self.assertEqual(command_acceptance_status(
            arrived, owner=1, unit_id=9, destination=(28, 12)
        ), "arrived")

    def test_command_acceptance_requires_peer_agreement(self):
        games = [
            {"units": [{
                "id": 9, "owner": 1, "x": 34, "y": 8,
                "destinationX": 28, "destinationY": 12,
                "moving": True, "waypointCount": 0,
            }]},
            {"units": [{
                "id": 9, "owner": 1, "x": 34, "y": 8,
                "destinationX": 34, "destinationY": 8,
                "moving": False, "waypointCount": 0,
            }]},
        ]
        self.assertIsNone(command_acceptance_status(
            games, owner=1, unit_id=9, destination=(28, 12)
        ))

    def test_command_acceptance_tolerates_tick_and_position_skew(self):
        games = [
            {"currentTick": 1363, "units": [{
                "id": 9, "owner": 1, "x": 27, "y": 11,
                "destinationX": 28, "destinationY": 12,
                "moving": True, "waypointCount": 0,
            }]},
            {"currentTick": 1360, "units": [{
                "id": 9, "owner": 1, "x": 26, "y": 10,
                "destinationX": 28, "destinationY": 12,
                "moving": True, "waypointCount": 0,
            }]},
        ]
        self.assertEqual(command_acceptance_status(
            games, owner=1, unit_id=9, destination=(28, 12)
        ), "accepted")
        for game in games:
            game["units"][0].update({
                "x": 28, "y": 12, "moving": False,
            })
        self.assertEqual(command_acceptance_status(
            games, owner=1, unit_id=9, destination=(28, 12)
        ), "arrived")

    def test_command_acceptance_rejects_different_destinations(self):
        games = [
            {"units": [{
                "id": 9, "owner": 1, "x": 34, "y": 8,
                "destinationX": destination_x,
                "destinationY": destination_y,
                "moving": True, "waypointCount": 0,
            }]}
            for destination_x, destination_y in ((28, 12), (29, 12))
        ]
        self.assertIsNone(command_acceptance_status(
            games, owner=1, unit_id=9, destination=(28, 12)
        ))

    def test_arrival_during_camera_preparation_is_synchronized(self):
        games = [{"units": [{
            "id": 10, "owner": 1, "x": 40, "y": 8,
        }]} for _ in range(2)]
        with patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ):
            self.assertTrue(entity_arrived_on_both_peers(
                object(), object(), owner=1, entity_id=10,
                destination=(40, 8),
            ))
            self.assertFalse(entity_arrived_on_both_peers(
                object(), object(), owner=1, entity_id=10,
                destination=(36, 4),
            ))

    def test_typed_camera_expiry_is_unsampled_then_route_continues(self):
        games = [{"units": [{
            "id": 10, "owner": 1, "x": 40, "y": 8,
        }]} for _ in range(2)]
        captures = unittest.mock.Mock(side_effect=[
            PostCameraDirectionExpired("direction 0 expired"),
            {"peers": {"join": {"actualLogicalDirection": 1}}},
        ])
        with patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ):
            first = capture_route_pixel_or_unsampled(
                captures, object(), object(), owner=1, entity_id=10,
                destination=(40, 8),
            )
            second = capture_route_pixel_or_unsampled(
                captures, object(), object(), owner=1, entity_id=10,
                destination=(40, 8),
            )
        self.assertEqual(first, {
            "status": "UNSAMPLED",
            "reason": "arrived-during-camera-preparation",
        })
        self.assertEqual(second["peers"]["join"]["actualLogicalDirection"], 1)
        self.assertEqual(captures.call_count, 2)

    def test_unrelated_capture_failure_propagates(self):
        with self.assertRaisesRegex(Failure, "pixel write failed"):
            capture_route_pixel_or_unsampled(
                unittest.mock.Mock(side_effect=Failure("pixel write failed")),
                object(), object(), owner=1, entity_id=10,
                destination=(40, 8),
            )

    def test_absent_command_blocks_before_pathfinding_failure(self):
        game = {"units": [{
            "id": 9, "owner": 1, "x": 34, "y": 8,
            "destinationX": 34, "destinationY": 8,
            "moving": False, "waypointCount": 0,
        }]}
        with patch(
            "nostr_multiplayer_smoke_test.capture_correlated_frames",
            return_value=[],
        ), patch(
            "nostr_multiplayer_smoke_test.game_diagnostics",
            side_effect=[game, copy.deepcopy(game)],
        ), patch(
            "nostr_multiplayer_smoke_test.time.monotonic",
            side_effect=[0.0, 1.0, 11.0],
        ):
            with self.assertRaisesRegex(
                InfrastructureBlocked, "BLOCKED_COMMAND_ABSENT"
            ):
                capture_until_arrival(
                    object(), object(), owner=1, unit_id=9,
                    destination=(28, 12), artifact_dir=Path("ignored"),
                    label="absent-command",
                )

    def test_closed_route_waits_for_departure_before_final_arrival(self):
        def game(x, y, destination_x, destination_y, *, waypoints=0):
            return {"units": [{
                "id": 9, "owner": 1, "x": x, "y": y,
                "destinationX": destination_x,
                "destinationY": destination_y,
                "moving": (x, y) != (destination_x, destination_y),
                "waypointCount": waypoints,
            }]}

        initial = game(20, 12, 20, 12, waypoints=4)
        moving = game(22, 12, 20, 12, waypoints=3)
        arrived = game(20, 12, 20, 12)
        diagnostics = [
            initial, copy.deepcopy(initial),
            moving, copy.deepcopy(moving),
            arrived, copy.deepcopy(arrived),
        ]
        with patch(
            "nostr_multiplayer_smoke_test.capture_correlated_frames",
            return_value=[],
        ), patch(
            "nostr_multiplayer_smoke_test.game_diagnostics",
            side_effect=diagnostics,
        ), patch(
            "nostr_multiplayer_smoke_test.time.monotonic",
            side_effect=[0.0, 0.1, 0.2, 0.3, 0.4],
        ):
            self.assertEqual(capture_until_arrival(
                object(), object(), owner=1, unit_id=9,
                destination=(20, 12), artifact_dir=Path("ignored"),
                label="closed-route", require_departure=True,
            ), [])

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

    def test_loads_exact_canonical_relay_pool(self):
        relays = load_default_relays()
        self.assertEqual(len(relays), 20)
        self.assertEqual(len(set(relays)), 20)
        self.assertTrue(all(relay.startswith("wss://") for relay in relays))
        self.assertEqual(len(relay_pool_digest(relays)), 64)

    def test_relay_pool_digest_covers_order_and_membership(self):
        relays = ["wss://one.example", "wss://two.example"]
        self.assertNotEqual(
            relay_pool_digest(relays), relay_pool_digest(list(reversed(relays)))
        )
        self.assertNotEqual(
            relay_pool_digest(relays), relay_pool_digest(relays[:1])
        )

    def test_rejects_noncanonical_relay_pool(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "relays.json"
            path.write_text(json.dumps({"relays": ["wss://one"]}))
            with self.assertRaisesRegex(ValueError, "20 unique wss URLs"):
                load_default_relays(path)

    def test_relay_probe_preserves_configured_order_for_quorum(self):
        class Socket:
            def __init__(self):
                self.messages = []

            def settimeout(self, _timeout):
                pass

            def send(self, payload):
                message = json.loads(payload)
                if message[0] == "EVENT":
                    self.event = message[1]
                    self.messages.append(json.dumps([
                        "OK", self.event["id"], True, "",
                    ]))
                elif message[0] == "REQ":
                    subscription = message[1]
                    self.messages.extend((
                        json.dumps(["EVENT", subscription, self.event]),
                        json.dumps(["EOSE", subscription]),
                    ))

            def recv(self):
                return self.messages.pop(0)

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
            probe_event={"id": "a" * 64, "kind": 78},
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

    def test_relay_probe_excludes_kind_78_rejection(self):
        class Socket:
            def __init__(self):
                self.closed = False

            def settimeout(self, _timeout):
                pass

            def send(self, payload):
                message = json.loads(payload)
                if message[0] == "EVENT":
                    self.messages = [json.dumps([
                        "OK", message[1]["id"], False,
                        "unsupported event kind: 78",
                    ])]

            def recv(self):
                return self.messages.pop(0)

            def close(self):
                self.closed = True

        socket = Socket()
        report = probe_relay_pool(
            ["wss://rejects-kind-78"], timeout=0.25,
            connector=lambda *_args, **_kwargs: socket,
            probe_event={"id": "b" * 64, "kind": 78},
        )
        self.assertEqual(report["selectedQuorum"], [])
        self.assertFalse(report["results"][0]["healthy"])
        self.assertIn("unsupported event kind: 78",
                      report["results"][0]["error"])
        self.assertTrue(socket.closed)

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

    def test_allocates_exact_destination_with_nested_report_root(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact_root = Path(directory) / "artifacts"
            run_id = "declared-run"
            destination = allocate_audit_destination(
                artifact_root, artifact_root / run_id / "reports", run_id
            )
            self.assertEqual(destination.artifacts, artifact_root / run_id)
            self.assertEqual(destination.report.parent,
                             artifact_root / run_id / "reports")
            self.assertTrue(destination.artifacts.is_dir())
            self.assertTrue(destination.report.is_file())

    def test_adopts_exact_destination_reserved_by_preflight(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact_root = Path(directory) / "artifacts"
            artifacts = artifact_root / "declared-run"
            artifacts.mkdir(parents=True)
            preflight = artifacts / "preflight.md"
            preflight.write_text("# Preflight\n")
            destination = allocate_audit_destination(
                artifact_root, artifacts / "reports", "declared-run"
            )
            self.assertEqual(destination.artifacts, artifacts)
            self.assertEqual(preflight.read_text(), "# Preflight\n")
            self.assertTrue(destination.report.is_file())

    def test_rejects_non_preflight_exact_destination_content(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact_root = Path(directory) / "artifacts"
            artifacts = artifact_root / "declared-run"
            artifacts.mkdir(parents=True)
            (artifacts / "run.json").write_text("{}")
            with self.assertRaises(FileExistsError):
                allocate_audit_destination(
                    artifact_root, artifacts / "reports", "declared-run"
                )

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
    def test_held_camera_key_does_not_click_canvas(self):
        calls = []

        class Chain:
            def __init__(self, driver):
                calls.append(("driver", driver))

            def key_down(self, *args):
                calls.append(("down", args))
                return self

            def pause(self, seconds):
                calls.append(("pause", seconds))
                return self

            def key_up(self, *args):
                calls.append(("up", args))
                return self

            def perform(self):
                calls.append(("perform",))

        driver = object()
        with patch(
            "nostr_multiplayer_smoke_test.render_diagnostics",
            return_value={"tick": 12, "frame": 34},
        ), patch(
            "nostr_multiplayer_smoke_test.ActionChains", Chain,
        ):
            audited_held_key(
                driver, [], "join", Keys.ARROW_DOWN, seconds=0.2,
            )

        self.assertEqual(calls[1], ("down", (Keys.ARROW_DOWN,)))
        self.assertEqual(calls[3], ("up", (Keys.ARROW_DOWN,)))

    def test_camera_accepts_right_clamp_route_tile_left_of_controls(self):
        class Journey:
            def telemetry(self):
                return {"camera": {"x": 1280.0, "y": 524.0, "zoom": 1.0}}

        actions = []
        with patch(
            "nostr_multiplayer_smoke_test.audited_held_key",
        ) as held_key:
            center_camera_for_tile(
                Journey(), object(), actions, "join", 40, 8,
            )
        held_key.assert_not_called()

    def test_correlated_capture_centers_both_peer_cameras(self):
        actor_journey = object()
        observer_journey = object()
        actor_driver = object()
        observer_driver = object()
        actions: list[dict[str, object]] = []
        with patch(
            "nostr_multiplayer_smoke_test.center_camera_for_tile",
        ) as center:
            center_peer_cameras_for_tile(
                actor_journey, actor_driver, "join",
                observer_journey, observer_driver, "host",
                actions, 27, 20,
            )
        self.assertEqual(center.call_args_list, [
            call(actor_journey, actor_driver, actions, "join", 27, 20),
            call(observer_journey, observer_driver, actions, "host", 27, 20),
        ])

    def test_route_selection_reacquires_position_after_camera_pan(self):
        class Journey:
            def telemetry(self):
                return {"selectedUnit": 10}

        before = {10: (28, 15)}
        after = {10: (34, 8)}
        games = [{"units": [
            {"id": 10, "owner": 1, "x": x, "y": y}
        ]} for x, y in [after[10], after[10]]]
        actions = []

        def pointer_action(*_args, **_kwargs):
            actions.append({})

        synchronized_reads = iter((before, after, after, after))

        def wait(_label, callback, timeout=None):
            if "position" not in _label:
                return callback()
            positions = next(synchronized_reads)
            return [{"units": [
                {"id": 10, "owner": 1, "x": positions[10][0],
                 "y": positions[10][1]}
            ]}] * 2

        with patch(
            "nostr_multiplayer_smoke_test.wait_until",
            side_effect=wait,
        ), patch(
            "nostr_multiplayer_smoke_test.matching_games",
            return_value=games,
        ), patch(
            "nostr_multiplayer_smoke_test.center_camera_for_tile",
        ), patch(
            "nostr_multiplayer_smoke_test.audited_world_pointer",
            side_effect=pointer_action,
        ) as pointer:
            selected = select_route_unit_at_current_position(
                Journey(), object(), "join", 1, 10,
                object(), object(), actions,
            )
        self.assertEqual(selected, after[10])
        pointer.assert_called_once()
        self.assertEqual(pointer.call_args.args[-2:], (34, 8))

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
            "queued-waypoints", "quantization-boundary-vectors",
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
        boundary_vectors = [
            (right[0] - left[0], right[1] - left[1])
            for left, right in zip(
                routes["quantization-boundary-vectors"],
                routes["quantization-boundary-vectors"][1:],
            )
        ]
        self.assertTrue(all(
            sorted((abs(dx), abs(dy))) == [2, 4]
            for dx, dy in boundary_vectors
        ))

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

    def test_missing_correlated_frames_are_blocked_not_failed(self):
        result = analyze_render_samples_for_audit([], "queued-waypoints")

        self.assertEqual(result["verdict"], "BLOCKED")
        self.assertEqual(result["frames"], 0)
        self.assertEqual(visual_failures({"oracle": result}), [])
        self.assertEqual(visual_findings({"oracle": result}), [result])

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

    def test_accepts_composite_overlay_after_canonical_primary_body(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["category"] = "building-house"
            entity["layers"].append({"resourceId": 429, "frame": 3})
        self.assertEqual(analyze_render_samples([value])["frames"], 1)

    def test_animated_building_does_not_require_movement_positions(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["category"] = "building-house"
            entity["layers"].append({
                "resourceId": 429,
                "frame": 3,
                "framesPerDirection": 20,
                "physicalFrameCount": 20,
                "mirroringMode": 0,
                "actionFrame": 3,
                "resolvedStoredDirection": 0,
            })

        result = analyze_render_samples([value])

        self.assertEqual(result["legacy"], 2)
        self.assertFalse(any(
            oracle["oracleKind"] == "frame-selection"
            for oracle in result["visualOracles"]
        ))
        self.assertEqual(
            sum(oracle["oracleKind"] == "animation-progress"
                for oracle in result["visualOracles"]),
            4,
        )

    def test_animated_movable_entity_requires_movement_positions(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            value[peer]["entities"][0]["layers"][0].update({
                "framesPerDirection": 2,
                "physicalFrameCount": 10,
                "mirroringMode": 6,
                "actionFrame": 1,
                "resolvedStoredDirection": 0,
            })

        with self.assertRaisesRegex(Failure, "frame oracle lacks positions"):
            analyze_render_samples([value])

    def test_rejects_unexpected_composite_primary_body(self):
        value = sample(1, 10.0)
        for peer in ("host", "join"):
            entity = value[peer]["entities"][0]
            entity["layers"][0]["resourceId"] = 999
            entity["layers"].append({"resourceId": 1479, "frame": 1})
        with self.assertRaisesRegex(Failure, "violates mapping"):
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

    def test_negative_control_mutations_are_not_visual_failures(self):
        production_failure = {"verdict": "FAIL", "kind": "production"}
        evidence = {
            "oracle": production_failure,
            "pixelCapture": {
                "mutationProof": {
                    "peers": {
                        "host": {"verdict": "FAIL", "kind": "mutation"},
                        "join": {"verdict": "FAIL", "kind": "mutation"},
                    },
                    "wrongPosition": {
                        "peers": {
                            "host": {"verdict": "FAIL"},
                            "join": {"verdict": "FAIL"},
                        },
                    },
                },
            },
        }

        self.assertEqual(visual_failures(evidence), [production_failure])
        self.assertEqual(visual_findings(evidence), [production_failure])

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

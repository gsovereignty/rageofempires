#!/usr/bin/env python3
"""Two-browser production-path smoke test over ordinary public Nostr relays."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import math
import secrets
import subprocess
import sys
import time
import traceback
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.actions.action_builder import ActionBuilder
from selenium.webdriver.common.actions.pointer_input import PointerInput
from selenium.webdriver.common.by import By
from selenium.common.exceptions import StaleElementReferenceException
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import Select
from PIL import Image

from browser_risk_spike_test import (
    DIST,
    Failure,
    Journey,
    make_driver,
    static_server,
    wait_until,
)

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from audit_multiplayer_screenshots import audit as audit_screenshots
from nostr_visual_frame_oracle import FrameOracleError, evaluate_layer
from nostr_visual_coverage import evaluate_coverage, load_specification


ROOT = Path(__file__).resolve().parents[2]
AUDIT_ROOT = ROOT / "artifacts" / "nostr-e2e-visual"
AUDIT_REPORT_ROOT = ROOT / "docs" / "audits"
WAIT_SECONDS = 180.0


@dataclass(frozen=True)
class AuditDestination:
    artifacts: Path
    report: Path
    run_id: str


def atomic_write_json(path: Path, value: object) -> None:
    temporary = path.with_name(f".{path.name}.{secrets.token_hex(6)}.tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def allocate_audit_destination(
    artifact_root: Path = AUDIT_ROOT,
    report_root: Path = AUDIT_REPORT_ROOT,
) -> AuditDestination:
    """Atomically reserve durable report and artifact destinations."""
    artifact_root.mkdir(parents=True, exist_ok=True)
    report_root.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc)
    stamp = now.strftime("%Y%m%dT%H%M%SZ")
    report_day = now.strftime("%Y-%m-%d")
    while True:
        run_id = secrets.token_hex(6)
        artifacts = artifact_root / f"{stamp}-{run_id}"
        try:
            artifacts.mkdir()
            break
        except FileExistsError:
            continue
    report = report_root / f"{report_day}-NOSTR-E2E-VISUAL-GAMEPLAY.md"
    if report.exists():
        report = report_root / (
            f"{report_day}-NOSTR-E2E-VISUAL-GAMEPLAY-{run_id}.md"
        )
    report.touch(exist_ok=False)
    return AuditDestination(artifacts, report, run_id)


def allocate_audit_directory(root: Path = AUDIT_ROOT) -> Path:
    """Compatibility helper for direct test runs."""
    return allocate_audit_destination(root).artifacts


def initialize_run_ledger(
    destination: AuditDestination,
    *,
    relays: str | None,
    headed: bool,
    port: int,
    seed: int,
    retry_budget: int,
) -> None:
    def ledger_path(path: Path) -> str:
        try:
            return str(path.relative_to(ROOT))
        except ValueError:
            return str(path)

    package_files = sorted({
        *DIST.glob("aoe_web.*"), DIST / "aoe_nostr.js",
    })
    package_digests = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in package_files if path.is_file()
    }
    commit = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
        capture_output=True, text=True,
    ).stdout.strip()
    ledger = {
        "schemaVersion": 2,
        "status": "RUNNING",
        "startedUtc": datetime.now(timezone.utc).isoformat(),
        "runId": destination.run_id,
        "reportPath": ledger_path(destination.report),
        "artifactPath": ledger_path(destination.artifacts),
        "sourceCommit": commit,
        "packageSha256": package_digests,
        "browser": {"name": "chrome", "headed": headed,
                    "versions": "captured after driver creation"},
        "scenario": "packaged Nostr multiplayer production scenario",
        "seed": seed,
        "hostPublicKey": "pending product identity initialization",
        "joinPublicKey": "pending product identity initialization",
        "relayPool": relays.split(",") if relays else
            "packaged production defaults",
        "selectedQuorum": [],
        "retryBudget": retry_budget,
        "serverPort": port,
        "viewport": {"width": 1280, "height": 720},
        "dpr": [1, 2],
        "capture": {
            "renderTelemetry": True,
            "correlatedScreenshots": True,
            "frameFlipOracle": "aok-fun-00510160-v1",
        },
        "privateKeysRetained": False,
    }
    atomic_write_json(destination.artifacts / "run.json", ledger)
    write_jsonl(destination.artifacts / "actions.jsonl", [])
    write_jsonl(destination.artifacts / "correlated-frames.jsonl", [])
    write_jsonl(destination.artifacts / "visual-oracles.jsonl", [])
    atomic_write_json(destination.artifacts / "coverage.json", {
        "schemaVersion": 1, "requiredCells": [], "cells": {},
        "status": "RUNNING",
    })
    atomic_write_json(destination.artifacts / "verdict.json", {
        "schemaVersion": 1, "status": "RUNNING",
    })
    write_report(
        destination.artifacts, "RUNNING",
        "Durable destinations allocated before browser launch.",
        report_path=destination.report,
    )


def write_report(root: Path, verdict: str, detail: str,
                 report_path: Path | None = None) -> None:
    report = (
        "# Nostr multiplayer gameplay audit\n\n"
        f"- Run: `{root.name}`\n"
        f"- Started UTC: `{datetime.now(timezone.utc).isoformat()}`\n"
        f"- Verdict: **{verdict}**\n\n"
        "## Result\n\n"
        f"{detail}\n"
    )
    (root / "report.md").write_text(report, encoding="utf-8")
    if report_path is not None:
        report_path.write_text(report, encoding="utf-8")


def diagnostics(driver) -> dict[str, object] | None:
    value = driver.execute_script(
        "return typeof Module !== 'undefined' && "
        "typeof Module.browserNostrDiagnostics === 'function' "
        "? Module.browserNostrDiagnostics() : null"
    )
    return value if isinstance(value, dict) else None


def game_diagnostics(driver) -> dict[str, object] | None:
    value = diagnostics(driver)
    game = value.get("game") if value else None
    return game if isinstance(game, dict) else None


def render_diagnostics(driver) -> dict[str, object] | None:
    value = driver.execute_script(
        "return typeof Module !== 'undefined' "
        "? (Module.browserRenderTelemetry || null) : null"
    )
    return value if isinstance(value, dict) else None


def capture_failure_value(label: str, callback) -> object:
    """Capture secondary diagnostics without masking the primary failure."""
    try:
        return callback()
    except Exception as error:
        return {
            "captureError": f"{label}: {type(error).__name__}: {error}",
        }


def capture_correlated_frames(host, join, seconds: float = 1.0,
                              artifact_dir: Path | None = None,
                              label: str = "motion",
                              maximum_samples: int = 24) \
        -> list[dict[str, object]]:
    deadline = time.monotonic() + seconds
    samples: list[dict[str, object]] = []
    last_frames: tuple[int, int] | None = None
    while time.monotonic() < deadline and len(samples) < maximum_samples:
        games = [game_diagnostics(driver) or {}
                 for driver in (host, join)]
        if (int(games[0].get("currentTick", -1)) !=
                int(games[1].get("currentTick", -2)) or
                games[0].get("stateHash") != games[1].get("stateHash")):
            time.sleep(0.01)
            continue
        states = [render_diagnostics(driver) or {}
                  for driver in (host, join)]
        frames = tuple(int(state.get("frame", -1)) for state in states)
        render_ticks = tuple(int(state.get("tick", -1)) for state in states)
        authoritative_tick = int(games[0].get("currentTick", -1))
        games_after = [game_diagnostics(driver) or {}
                       for driver in (host, join)]
        stable_authoritative = all(
            int(game.get("currentTick", -2)) == authoritative_tick and
            game.get("stateHash") == games[0].get("stateHash")
            for game in games_after
        )
        if (frames != last_frames and min(frames) >= 0 and
                render_ticks == (authoritative_tick, authoritative_tick) and
                stable_authoritative):
            sample: dict[str, object] = {
                "host": states[0], "join": states[1],
                "authoritativeHost": games[0],
                "authoritativeJoin": games[1],
                "capturedMonotonic": time.monotonic(),
                "action": label,
                "screenshots": {},
            }
            if artifact_dir is not None:
                for peer, driver, state in zip(
                    ("host", "join"), (host, join), states, strict=True
                ):
                    directory = artifact_dir / "frames" / peer
                    directory.mkdir(parents=True, exist_ok=True)
                    path = directory / (
                        f"{label}-tick-{int(state.get('tick', -1))}-"
                        f"frame-{int(state.get('frame', -1))}.png"
                    )
                    driver.save_screenshot(str(path))
                    state_after = render_diagnostics(driver) or {}
                    game_after = game_diagnostics(driver) or {}
                    sample["screenshots"][peer] = {
                        "path": str(path.relative_to(artifact_dir)),
                        "beforeTick": int(state.get("tick", -1)),
                        "beforeFrame": int(state.get("frame", -1)),
                        "afterTick": int(state_after.get("tick", -1)),
                        "afterFrame": int(state_after.get("frame", -1)),
                        "camera": state.get("camera"),
                        "authoritativeTickAfter": int(
                            game_after.get("currentTick", -1)
                        ),
                        "authoritativeHashAfter": game_after.get("stateHash"),
                        "exactFrame": (
                            int(state_after.get("frame", -2)) ==
                            int(state.get("frame", -1)) and
                            int(state_after.get("tick", -2)) ==
                            authoritative_tick and
                            int(game_after.get("currentTick", -2)) ==
                            authoritative_tick and
                            game_after.get("stateHash") ==
                            games[0].get("stateHash")
                        ),
                    }
            samples.append(sample)
            last_frames = frames
        time.sleep(0.03)
    return samples


def audited_pointer(journey: Journey, actions: list[dict[str, object]],
                    actor: str, target: str, button: int = 0,
                    logical_dx: float = 0, logical_dy: float = 0) -> None:
    telemetry = journey.telemetry()
    point = telemetry["targets"][target]
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "pointer",
        "target": target,
        "button": button,
        "logicalDx": logical_dx,
        "logicalDy": logical_dy,
        "targetLogicalX": float(point["x"]),
        "targetLogicalY": float(point["y"]),
        "telemetryTick": int(telemetry["tick"]),
    })
    journey.pointer(target, button, logical_dx, logical_dy)


def audited_key(driver, actions: list[dict[str, object]], actor: str,
                key: str) -> None:
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "key",
        "key": key,
    })
    driver.find_element(By.ID, "canvas").send_keys(key)


def audited_held_key(driver, actions: list[dict[str, object]], actor: str,
                     key: str, seconds: float = 0.15) -> None:
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "held-key",
        "key": key,
        "seconds": seconds,
    })
    canvas = driver.find_element(By.ID, "canvas")
    ActionChains(driver).key_down(key, canvas).pause(seconds).key_up(
        key, canvas
    ).perform()


def collapse_match_details(
    driver, actions: list[dict[str, object]], actor: str
) -> None:
    button = driver.find_element(By.ID, "toggle-nostr-session-details")
    if button.get_attribute("aria-expanded") == "true":
        actions.append({
            "monotonic": time.monotonic(),
            "actor": actor,
            "kind": "ui-button",
            "target": "toggle-nostr-session-details",
        })
        button.click()
    wait_until(
        f"{actor} collapsed public match details",
        lambda: (
            True if driver.find_element(
                By.ID, "nostr-session-details"
            ).get_attribute("hidden") is not None else None
        ),
    )


def pan_world_target_clear(
    journey: Journey,
    driver,
    actions: list[dict[str, object]],
    actor: str,
    target_name: str,
    edge_margin: float = 80.0,
) -> None:
    """Move a world target above in-canvas multiplayer panels before click."""
    for _ in range(64):
        target = journey.telemetry()["targets"][target_name]
        x = float(target["x"])
        y = float(target["y"])
        if edge_margin < x < 1280.0 - edge_margin and \
                10.0 < y < 520.0:
            return
        if y >= 520.0:
            key = Keys.ARROW_DOWN
        elif y <= 10.0:
            key = Keys.ARROW_UP
        elif x <= edge_margin:
            key = Keys.ARROW_LEFT
        else:
            key = Keys.ARROW_RIGHT
        audited_held_key(driver, actions, actor, key)
    raise Failure(
        f"{actor} {target_name} never entered unobstructed world area"
    )


def select_barracks_through_footprint(
    journey: Journey,
    driver,
    actions: list[dict[str, object]],
    actor: str,
) -> int:
    pan_world_target_clear(
        journey, driver, actions, actor, "barracks", edge_margin=120.0
    )
    zoom = float(journey.telemetry()["camera"]["zoom"])
    # Telemetry reports the Barracks footprint center. Try every tile in its
    # 3x3 footprint so units drawn over one tile cannot intercept every
    # selection attempt.
    offsets = tuple(
        (
            (tile_x - tile_y) * 32.0 * zoom,
            (tile_x + tile_y) * 16.0 * zoom,
        )
        for tile_y in range(-1, 2)
        for tile_x in range(-1, 2)
    )
    for logical_dx, logical_dy in offsets:
        audited_pointer(
            journey, actions, actor, "barracks",
            logical_dx=logical_dx, logical_dy=logical_dy,
        )
        try:
            return wait_until(
                f"{actor} barracks footprint selection",
                lambda: int(
                    journey.telemetry().get("selectedBuilding", 0)
                ) or None,
                timeout=2.0,
            )
        except Failure:
            continue
    raise Failure(f"{actor} could not select Barracks through its footprint")


def audited_command_button(driver, actions: list[dict[str, object]],
                           actor: str, grid_slot: int) -> None:
    column = grid_slot % 5
    row = grid_slot // 5
    logical_x = 37 + 41 * column + 20
    logical_y = (720 - 175) + 31 + 41 * row + 20
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "command-button",
        "gridSlot": grid_slot,
        "targetLogicalX": logical_x,
        "targetLogicalY": logical_y,
    })
    click_canvas_logical(driver, logical_x, logical_y)


def audited_world_pointer(
    journey: Journey,
    driver,
    actions: list[dict[str, object]],
    actor: str,
    tile_x: int,
    tile_y: int,
    button: int = 2,
    modifiers: int = 0,
) -> None:
    telemetry = journey.telemetry()
    camera = telemetry["camera"]
    zoom = float(camera["zoom"])
    # Fixed packaged scenario is 48x32. These constants match production
    # isometric projection: map_origin_x = map_height * 32.
    logical_x = (32 * 32 + (tile_x - tile_y) * 32 -
                 float(camera["x"])) * zoom
    logical_y = (16 + 16 + (tile_x + tile_y) * 16 -
                 float(camera["y"])) * zoom
    actions.append({
        "monotonic": time.monotonic(), "actor": actor,
        "kind": "world-pointer", "button": button,
        "tileX": tile_x, "tileY": tile_y,
        "logicalX": logical_x, "logicalY": logical_y,
        "telemetryTick": int(telemetry["tick"]),
    })
    click_canvas_logical(
        driver, logical_x, logical_y, button=button, modifiers=modifiers
    )


def canonical_direction_route(
    center: tuple[int, int], radius: int = 4
) -> list[tuple[int, int]]:
    """Return closed octagonal route whose segments cover directions 0..7."""
    if radius < 1:
        raise ValueError("route radius must be positive")
    vectors = (
        (1, 1), (0, 1), (-1, 1), (-1, 0),
        (-1, -1), (0, -1), (1, -1), (1, 0),
    )
    points = [center]
    x, y = center
    for dx, dy in vectors:
        x += dx * radius
        y += dy * radius
        points.append((x, y))
    return points


def center_camera_for_tile(
    journey: Journey, driver, actions: list[dict[str, object]],
    actor: str, tile_x: int, tile_y: int,
) -> None:
    for _ in range(64):
        camera = journey.telemetry()["camera"]
        zoom = float(camera["zoom"])
        logical_x = (32 * 32 + (tile_x - tile_y) * 32 -
                     float(camera["x"])) * zoom
        logical_y = (32 + (tile_x + tile_y) * 16 -
                     float(camera["y"])) * zoom
        # The production Nostr session controls occupy the right side of the
        # canvas. Keep world commands left of that overlay so they reach SDL.
        if 100.0 < logical_x < 900.0 and 80.0 < logical_y < 500.0:
            return
        if logical_y >= 500.0:
            key = Keys.ARROW_DOWN
        elif logical_y <= 80.0:
            key = Keys.ARROW_UP
        elif logical_x <= 100.0:
            key = Keys.ARROW_LEFT
        else:
            key = Keys.ARROW_RIGHT
        audited_held_key(driver, actions, actor, key)
    raise Failure(f"{actor} route tile never entered visible world area")


def capture_until_arrival(
    host, join, *, owner: int, unit_id: int,
    destination: tuple[int, int], artifact_dir: Path, label: str,
) -> list[dict[str, object]]:
    deadline = time.monotonic() + WAIT_SECONDS
    samples: list[dict[str, object]] = []
    seen: set[tuple[int, int]] = set()
    while time.monotonic() < deadline:
        for sample in capture_correlated_frames(
            host, join, seconds=0.12, artifact_dir=artifact_dir,
            label=label, maximum_samples=3,
        ):
            identity = (
                int((sample.get("host") or {}).get("frame", -1)),
                int((sample.get("join") or {}).get("frame", -1)),
            )
            if identity not in seen:
                seen.add(identity)
                samples.append(sample)
        games = matching_games(host, join)
        if games is not None:
            positions = [owned_unit_positions(game, owner) for game in games]
            if (positions[0] == positions[1] and
                    positions[0].get(unit_id) == destination):
                return samples
    raise Failure(
        f"unit {unit_id} did not arrive at route destination {destination}"
    )


def exercise_all_direction_route(
    journey: Journey, driver, actor: str, owner: int,
    observer_journey: Journey, observer_driver, observer_actor: str,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    center: tuple[int, int],
) -> dict[str, object]:
    games = wait_until(
        f"{actor} all-direction villager selection",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    candidates = owned_villager_positions(games[0], owner)
    if not candidates:
        raise Failure(f"{actor} has no owned route villager")
    unit_id, unit_position = min(
        candidates.items(),
        key=lambda item: (
            abs(item[1][0] - center[0]) + abs(item[1][1] - center[1]),
            item[0],
        ),
    )
    center_camera_for_tile(
        journey, driver, actions, actor, center[0], center[1]
    )
    center_camera_for_tile(
        observer_journey, observer_driver, actions, observer_actor,
        center[0] + 1, center[1],
    )
    audited_world_pointer(
        journey, driver, actions, actor,
        unit_position[0], unit_position[1], button=0,
    )
    wait_until(
        f"{actor} explicit route villager {unit_id} selection",
        lambda: unit_id if int(
            journey.telemetry().get("selectedUnit", 0)
        ) == unit_id else None,
    )
    audited_world_pointer(
        journey, driver, actions, actor, center[0], center[1]
    )
    approach = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=center,
        artifact_dir=artifact_dir, label=f"{actor}-route-approach",
    )
    route = canonical_direction_route(center)
    segments: list[dict[str, object]] = []
    all_frames: list[dict[str, object]] = []
    for lap in range(2):
        for direction, destination in enumerate(route[1:]):
            center_camera_for_tile(
                journey, driver, actions, actor,
                destination[0], destination[1]
            )
            # Observer follows same unit through normal camera controls.
            # One-tile offset preserves distinct cameras while keeping sprite
            # visible on both correlated screenshots.
            center_camera_for_tile(
                observer_journey, observer_driver, actions, observer_actor,
                destination[0] + 1, destination[1],
            )
            current = route[direction]
            audited_world_pointer(
                journey, driver, actions, actor,
                current[0], current[1], button=0,
            )
            wait_until(
                f"{actor} route unit {unit_id} selection",
                lambda: unit_id if int(
                    journey.telemetry().get("selectedUnit", 0)
                ) == unit_id else None,
            )
            audited_world_pointer(
                journey, driver, actions, actor,
                destination[0], destination[1]
            )
            frames = capture_until_arrival(
                host, join, owner=owner, unit_id=unit_id,
                destination=destination, artifact_dir=artifact_dir,
                label=f"{actor}-lap-{lap}-direction-{direction}",
            )
            all_frames.extend(frames)
            segments.append({
                "lap": lap, "logicalDirection": direction,
                "destination": {"x": destination[0], "y": destination[1]},
                "frameCount": len(frames),
            })
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "center": {"x": center[0], "y": center[1]},
        "approachFrameCount": len(approach),
        "segments": segments, "frames": all_frames,
        "renderOracle": analyze_render_samples_for_audit(
            all_frames, f"{actor}-all-directions"
        ),
    }


def analyze_render_samples(samples: list[dict[str, object]]) \
        -> dict[str, object]:
    if not samples:
        raise Failure("render oracle captured no correlated frames")
    counts = {"frames": len(samples), "entities": 0, "legacy": 0,
              "proceduralFailures": 0, "unprovenSources": 0,
              "unresolvedExpectedMappings": [],
              "blockingUnresolvedExpectedMappings": [],
              "animationSequenceBlocked": 0, "visualOracles": []}
    last_frame = {"host": -1, "join": -1}
    previous_positions: dict[
        tuple[str, str, int], tuple[float, float, int, tuple[int, int] | None]
    ] = {}
    maximum_displacement = 0.0
    animation_frames: dict[
        tuple[str, str, int, int, int, str, str],
        list[tuple[int, int, bool, int, int]]
    ] = {}
    unmatched_entities: list[dict[str, object]] = []

    def logical_direction(previous: dict[str, object],
                          current: dict[str, object],
                          angle_count: int) -> int | None:
        dx = int(current["x"]) - int(previous["x"])
        dy = int(current["y"]) - int(previous["y"])
        if dx == 0 and dy == 0:
            return None
        radians = math.atan2(dy - dx, dx + dy)
        if radians < 0.0:
            radians += 2.0 * math.pi
        step = 2.0 * math.pi / angle_count
        return int(math.floor(radians / step + 0.5)) % angle_count

    for sample in samples:
        for peer in ("host", "join"):
            state = sample.get(peer) or {}
            frame = int(state.get("frame", -1))
            if frame <= last_frame[peer]:
                raise Failure(f"non-monotonic {peer} render frame: {frame}")
            last_frame[peer] = frame
            for entity in state.get("entities", []):
                if not isinstance(entity, dict):
                    continue
                counts["entities"] += 1
                source = entity.get("source")
                if source == "legacy":
                    counts["legacy"] += 1
                    if not entity.get("layers"):
                        raise Failure(f"legacy entity lacks provenance: {entity}")
                    expected_resources = set(
                        entity.get("expectedResourceIds", [])
                    )
                    if entity.get("expectedAssetStatus") != "renderable":
                        raise Failure(
                            "production visual uses non-renderable asset "
                            f"mapping: {entity}"
                        )
                    if not expected_resources:
                        unresolved = {
                            "peer": peer,
                            "frame": frame,
                            "entity": entity,
                            "reason": "renderable mapping has no expected IDs",
                        }
                        counts["unresolvedExpectedMappings"].append(unresolved)
                        if str(entity.get("category", "")).startswith(
                            ("unit-", "projectile-", "impact-", "unit-death-")
                        ) or any(
                            all(field in layer for field in (
                                "framesPerDirection", "physicalFrameCount",
                                "mirroringMode", "actionFrame",
                            ))
                            for layer in entity.get("layers", [])
                        ):
                            counts[
                                "blockingUnresolvedExpectedMappings"
                            ].append(unresolved)
                    actual_resources = {
                        layer.get("resourceId")
                        for layer in entity.get("layers", [])
                    }
                    if (expected_resources and
                            not actual_resources.issubset(expected_resources)):
                        raise Failure(
                            f"rendered asset violates mapping: {entity}"
                        )
                    for layer_index, layer in enumerate(entity.get("layers", [])):
                        entity_key = (
                            peer, str(entity.get("category", "")),
                            int(entity.get("id", -1)), layer_index,
                        )
                        oracle_fields = (
                            "framesPerDirection", "physicalFrameCount",
                            "mirroringMode", "actionFrame",
                        )
                        if all(field in layer for field in oracle_fields):
                            previous = entity.get("previousPosition")
                            current = entity.get("simulationPosition")
                            if not (isinstance(previous, dict) and
                                    isinstance(current, dict)):
                                raise Failure(
                                    f"frame oracle lacks positions: {entity}"
                                )
                            if (int(previous["x"]), int(previous["y"])) == (
                                int(current["x"]), int(current["y"])
                            ):
                                continue
                            try:
                                oracle = evaluate_layer(
                                    previous=(int(previous["x"]),
                                              int(previous["y"])),
                                    current=(int(current["x"]),
                                             int(current["y"])),
                                    direction_count=int(
                                        layer.get(
                                            "directionCount",
                                            entity.get(
                                                "expectedDirectionCount", 8
                                            ),
                                        )
                                    ),
                                    frames_per_direction=int(
                                        layer["framesPerDirection"]
                                    ),
                                    physical_frame_count=int(
                                        layer["physicalFrameCount"]
                                    ),
                                    mirroring_mode=int(layer["mirroringMode"]),
                                    action_frame=int(layer["actionFrame"]),
                                    actual_frame=int(layer.get("frame", -1)),
                                    actual_flip_horizontal=bool(
                                        layer.get("flipHorizontal", False)
                                    ),
                                    actual_stored_direction=int(
                                        layer["resolvedStoredDirection"]
                                    ) if "resolvedStoredDirection" in layer
                                    else None,
                                )
                            except FrameOracleError as error:
                                raise Failure(
                                    "invalid frame oracle input "
                                    f"{entity_key}: {error}"
                                ) from error
                            if oracle["verdict"] != "PASS":
                                raise Failure(
                                    "physical frame/flip mismatch "
                                    f"{entity_key} "
                                    f"tick={state.get('tick')} "
                                    f"resource={layer.get('resourceId')} "
                                    f"oracle={json.dumps(oracle, sort_keys=True)}"
                                )
                            counts["visualOracles"].append({
                                **oracle,
                                "peer": peer,
                                "owner": entity.get("owner"),
                                "entity": entity.get("id"),
                                "unitKind": entity.get("category"),
                                "action": entity.get("action"),
                                "tick": state.get("tick"),
                                "renderFrame": state.get("frame"),
                                "layer": layer_index,
                                "resourceId": layer.get("resourceId"),
                                "directionCount": layer.get("directionCount"),
                                "framesPerDirection":
                                    layer.get("framesPerDirection"),
                                "mirroringMode": layer.get("mirroringMode"),
                                "screenshot": (sample.get("screenshots") or {})
                                    .get(peer),
                            })
                        animation_frames.setdefault(
                            (peer, str(entity.get("category", "")),
                             int(entity.get("id", -1)), layer_index,
                             int(entity.get("facing", -1)),
                             str(entity.get("animationState", "")),
                             str(entity.get("action", ""))), []
                        ).append((
                            int(state.get("tick", -1)),
                            int(layer.get("frame", -1)),
                            bool(entity.get("moving", False)),
                            int(entity.get("expectedRequiredFrameCount", 0)),
                            int(state.get("presentationTimeMs", 0)),
                        ))
                elif source == "intentional_procedural":
                    counts["proceduralFailures"] += 1
                    raise Failure(
                        f"procedural production visual is forbidden: {entity}"
                    )
                else:
                    counts["unprovenSources"] += 1
                    raise Failure(f"unproved production render source: {entity}")
                position = entity.get("renderPosition")
                if not isinstance(position, dict):
                    continue
                key = (peer, str(entity.get("category", "")),
                       int(entity.get("id", -1)))
                camera = state.get("camera") or {}
                current = (
                    float(position["x"]) + float(camera.get("x", 0.0)),
                    float(position["y"]) + float(camera.get("y", 0.0)),
                )
                simulation_position = entity.get("simulationPosition")
                authoritative = (
                    (int(simulation_position["x"]),
                     int(simulation_position["y"]))
                    if isinstance(simulation_position, dict) else None
                )
                tick = int(state.get("tick", -1))
                if key in previous_positions:
                    previous = previous_positions[key]
                    dx = current[0] - previous[0]
                    dy = current[1] - previous[1]
                    displacement = (dx * dx + dy * dy) ** 0.5
                    maximum_displacement = max(maximum_displacement,
                                               displacement)
                    tick_delta = max(tick - previous[2], 1)
                    if displacement > 80.0 * tick_delta:
                        raise Failure(
                            f"render teleport candidate {key}: {displacement}"
                        )
                    if (authoritative is not None and
                            previous[3] is not None and
                            authoritative != previous[3] and
                            displacement < 0.5):
                        raise Failure(f"render stall candidate {key}")
                previous_positions[key] = (
                    current[0], current[1], tick, authoritative
                )
                simulation_previous = entity.get("previousPosition")
                if (isinstance(simulation_position, dict) and
                        isinstance(simulation_previous, dict)):
                    direction_count = int(
                        entity.get("expectedDirectionCount", 8)
                    )
                    if direction_count <= 0:
                        direction_count = 8
                    expected_facing = logical_direction(
                        simulation_previous, simulation_position,
                        direction_count,
                    )
                    if (bool(entity.get("moving", False)) and
                            expected_facing is not None and
                            int(entity.get("facing", -1)) != expected_facing):
                        raise Failure(
                            f"movement facing mismatch {key}: expected "
                            f"{expected_facing}, got {entity.get('facing')}"
                        )
        host_state = sample.get("host") or {}
        join_state = sample.get("join") or {}
        for peer, state in (("host", host_state), ("join", join_state)):
            entity_keys = [
                (str(entity.get("category", "")),
                 int(entity.get("id", -1)))
                for entity in state.get("entities", [])
                if isinstance(entity, dict)
            ]
            if len(entity_keys) != len(set(entity_keys)):
                raise Failure(f"duplicate {peer} render entity identity")
        host_entities = {
            (str(entity.get("category", "")), int(entity.get("id", -1))): entity
            for entity in host_state.get("entities", [])
            if isinstance(entity, dict)
        }
        join_entities = {
            (str(entity.get("category", "")), int(entity.get("id", -1))): entity
            for entity in join_state.get("entities", [])
            if isinstance(entity, dict)
        }
        for peer, entities in (("host", host_entities),
                               ("join", join_entities)):
            resource_entities = {
                entity_id for (category, entity_id) in entities
                if category.startswith("resource-")
            }
            for key, entity in entities.items():
                if (not key[0].startswith("unit-") or
                        entity.get("action") != "gathering" or
                        bool(entity.get("returningResource", False))):
                    continue
                if not bool(entity.get("hasResourceTarget", False)):
                    raise Failure(f"{peer} gathering unit lacks target: {key}")
                if not bool(entity.get("resourceTargetInMap", False)):
                    raise Failure(f"{peer} gathering target outside map: {key}")
                if not bool(entity.get("resourceTargetExists", False)):
                    raise Failure(f"{peer} gathering target does not exist: {key}")
                amount = int(entity.get("resourceTargetAmount", -1))
                if amount <= 0:
                    raise Failure(
                        f"{peer} gathering depleted resource: {key}"
                    )
                if bool(entity.get("resourceTargetVisible", False)):
                    target_id = int(entity.get("resourceTargetEntityId", -1))
                    target_kind = str(entity.get("resourceTargetKind", "tile"))
                    matching_target = (
                        target_id in resource_entities
                        if target_kind == "tile" else
                        any(
                            entity_id == target_id and
                            category.startswith(f"{target_kind}-")
                            for category, entity_id in entities
                        )
                    )
                    if not matching_target:
                        raise Failure(
                            f"{peer} gathering visible resource is not "
                            f"rendered: {key} target={target_id}"
                        )
        missing = host_entities.keys() ^ join_entities.keys()
        if missing:
            cameras = [state.get("camera") or {}
                       for state in (host_state, join_state)]
            same_camera = all(
                abs(float(cameras[0].get(field, 0.0)) -
                    float(cameras[1].get(field, 0.0))) < 0.01
                for field in ("x", "y", "zoom")
            )
            unmatched_entities.append({
                "keys": sorted(missing),
                "classification": (
                    "missing-at-shared-camera" if same_camera
                    else "not-comparable-different-camera"
                ),
            })
            if same_camera:
                raise Failure(f"client entity-set divergence: {sorted(missing)}")
        for key in host_entities.keys() & join_entities.keys():
            host_entity = host_entities[key]
            join_entity = join_entities[key]
            for field in ("source", "facing", "action", "actionDetail",
                          "animationState", "hasResourceTarget",
                          "returningResource", "resourceTarget",
                          "resourceTargetInMap", "resourceTargetKind",
                          "resourceTargetExists", "resourceTargetAmount",
                          "resourceTargetEntityId", "resourceBuildingId",
                          "resourceUnitId", "carriedResource",
                          "carriedAmount"):
                if host_entity.get(field) != join_entity.get(field):
                    raise Failure(f"client render divergence {key} {field}")
            host_layers = host_entity.get("layers", [])
            join_layers = join_entity.get("layers", [])
            host_assets = [
                (layer.get("resourceId"), layer.get("palettePlayer"),
                 layer.get("flipHorizontal"),
                 layer.get("resolvedStoredDirection"))
                for layer in host_layers
            ]
            join_assets = [
                (layer.get("resourceId"), layer.get("palettePlayer"),
                 layer.get("flipHorizontal"),
                 layer.get("resolvedStoredDirection"))
                for layer in join_layers
            ]
            if host_assets != join_assets:
                raise Failure(f"client asset divergence {key}")
            # Frame phase includes sub-tick interpolation. Compare physical
            # frame geometry only where both peers selected same action-frame
            # input. Independent oracle checks every peer/frame separately.
            for host_layer, join_layer in zip(
                host_layers, join_layers, strict=True
            ):
                if (host_layer.get("actionFrame") ==
                        join_layer.get("actionFrame")) and any(
                    host_layer.get(field) != join_layer.get(field)
                    for field in (
                        "frame", "hotspotX", "hotspotY", "width", "height"
                    )
                ):
                    raise Failure(f"client asset divergence {key}")
    if counts["legacy"] == 0:
        raise Failure("render oracle observed no production provenance")
    for key, observations in animation_frames.items():
        for _, frame, _, _, _ in observations:
            if frame < 0:
                raise Failure(f"invalid animation frame {key}: {frame}")
        if len(observations) > 1:
            counts["animationSequenceBlocked"] += 1
        moving_ticks = {
            tick for tick, _, moving, _, _ in observations if moving
        }
        moving_frames = {
            frame for _, frame, moving, _, _ in observations if moving
        }
        if len(moving_ticks) >= 4 and len(moving_frames) < 2:
            raise Failure(f"frozen moving animation {key}")
    counts["maximumFrameDisplacement"] = maximum_displacement
    counts["unmatchedEntities"] = unmatched_entities
    return counts


def analyze_render_samples_for_audit(
    samples: list[dict[str, object]], phase: str
) -> dict[str, object]:
    """Retain visual failures while allowing full-match coverage to continue."""
    try:
        result = analyze_render_samples(samples)
        result["phase"] = phase
        if result["blockingUnresolvedExpectedMappings"]:
            result["verdict"] = "BLOCKED"
            result["blocker"] = (
                "renderable production assets lack expected resource IDs"
            )
        else:
            result["verdict"] = "PASS"
        return result
    except Failure as error:
        return {
            "phase": phase,
            "verdict": "FAIL",
            "failure": str(error),
            "frames": len(samples),
        }


def visual_failures(evidence: dict[str, object]) -> list[dict[str, object]]:
    failures: list[dict[str, object]] = []

    def visit(value: object) -> None:
        if isinstance(value, dict):
            if value.get("verdict") == "FAIL" and value.get("failure"):
                failures.append(value)
            for child in value.values():
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(evidence)
    return failures


def visual_findings(evidence: dict[str, object]) -> list[dict[str, object]]:
    findings: list[dict[str, object]] = []

    def visit(value: object) -> None:
        if isinstance(value, dict):
            if value.get("verdict") in {"FAIL", "BLOCKED"}:
                findings.append(value)
            for child in value.values():
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(evidence)
    return findings


def blue_villager_positions(game: dict[str, object]) -> dict[int, tuple[int, int]]:
    return {
        int(unit["id"]): (int(unit["x"]), int(unit["y"]))
        for unit in game.get("blueVillagers", [])
        if isinstance(unit, dict)
    }


def matching_moved_villager(host, join, unit_id: int,
                            before: tuple[int, int]):
    games = [game_diagnostics(driver) or {} for driver in (host, join)]
    positions = [blue_villager_positions(game) for game in games]
    if positions[0] != positions[1] or positions[0].get(unit_id) == before:
        return None
    return games if unit_id in positions[0] else None


def owned_unit_positions(game: dict[str, object], owner: int) \
        -> dict[int, tuple[int, int]]:
    return {
        int(unit["id"]): (int(unit["x"]), int(unit["y"]))
        for unit in game.get("units", [])
        if isinstance(unit, dict) and int(unit.get("owner", -1)) == owner
    }


def owned_villager_positions(game: dict[str, object], owner: int) \
        -> dict[int, tuple[int, int]]:
    blue_ids = set(blue_villager_positions(game))
    villager_kinds = {
        int(unit["kind"]) for unit in game.get("units", [])
        if isinstance(unit, dict) and int(unit.get("id", -1)) in blue_ids
    }
    if len(villager_kinds) != 1:
        return {}
    villager_kind = next(iter(villager_kinds))
    return {
        int(unit["id"]): (int(unit["x"]), int(unit["y"]))
        for unit in game.get("units", [])
        if isinstance(unit, dict) and int(unit.get("owner", -1)) == owner and
        int(unit.get("kind", -1)) == villager_kind
    }


def matching_moved_owned_unit(host, join, owner: int, unit_id: int,
                              before: tuple[int, int]):
    games = [game_diagnostics(driver) or {} for driver in (host, join)]
    positions = [owned_unit_positions(game, owner) for game in games]
    if positions[0] != positions[1] or positions[0].get(unit_id) == before:
        return None
    return games if unit_id in positions[0] else None


def owner_buildings(game: dict[str, object], owner: int) \
        -> list[dict[str, object]]:
    return [
        building for building in game.get("buildings", [])
        if isinstance(building, dict) and
        int(building.get("owner", -1)) == owner
    ]


def matching_games(host, join):
    games = [game_diagnostics(driver) or {} for driver in (host, join)]
    if (int(games[0].get("currentTick", -1)) !=
            int(games[1].get("currentTick", -2)) or
            games[0].get("stateHash") != games[1].get("stateHash")):
        return None
    return games


def banked_resource_increased(
    telemetry: dict[str, object], resource: str, initial: int,
) -> int | None:
    value = int(telemetry["resources"][resource])
    return value if value > initial else None


def selectable_military_id(
    selected_unit: int,
    military_ids: set[int],
    ordered_ids: set[int],
) -> int | None:
    return (
        selected_unit
        if selected_unit in military_ids and selected_unit not in ordered_ids
        else None
    )


def prepare_player_for_full_match(
    journey: Journey,
    driver,
    actor: str,
    owner: int,
    host,
    join,
    actions: list[dict[str, object]],
    artifact_dir: Path | None = None,
) -> dict[str, object]:
    telemetry = journey.telemetry()
    initial_gold = int(telemetry["resources"]["gold"])
    resource_tile = (7, 10) if owner == 0 else (40, 10)
    center_camera_for_tile(
        journey, driver, actions, actor, *resource_tile
    )
    route_unit_tile = (20, 12) if owner == 0 else (28, 12)
    audited_world_pointer(
        journey, driver, actions, actor, *route_unit_tile, button=0,
    )
    wait_until(
        f"{actor} route villager selection for gathering",
        lambda: int(journey.telemetry().get("selectedUnit", 0)) or None,
    )
    audited_world_pointer(
        journey, driver, actions, actor, *resource_tile,
    )
    gather_frames = capture_correlated_frames(
        host, join, seconds=2.0,
        artifact_dir=artifact_dir,
        label=f"{actor}-gather",
    )
    gather_oracle = analyze_render_samples_for_audit(
        gather_frames, f"{actor}-gather"
    )
    gathered = wait_until(
        f"{actor} gathered gold",
        lambda: banked_resource_increased(
            journey.telemetry(), "gold", initial_gold
        ),
        # Preserve gathering target until simulation naturally fills carry
        # capacity, returns to an eligible drop-off, and banks the resource.
        timeout=WAIT_SECONDS * 3,
    )

    select_barracks_through_footprint(
        journey, driver, actions, actor
    )
    initial_military = int(journey.telemetry()["blueMilitaryCount"])
    audited_key(driver, actions, actor, "m")
    trained = wait_until(
        f"{actor} militia production group",
        lambda: (
            value if int((value := journey.telemetry())["blueMilitaryCount"])
            >= initial_military + 1 else None
        ),
        timeout=WAIT_SECONDS * 3,
    )
    audited_key(driver, actions, actor, "9")
    researched = wait_until(
        f"{actor} man-at-arms research",
        lambda: (
            value if bool((value := journey.telemetry())[
                "manAtArmsResearched"
            ]) else None
        ),
        timeout=WAIT_SECONDS * 3,
    )
    native_modified_digit(driver, "2", Keys.CONTROL)

    before_games = wait_until(
        f"{actor} pre-construction lockstep", lambda: matching_games(host, join),
        timeout=WAIT_SECONDS,
    )
    initial_building_count = len(owner_buildings(before_games[0], owner))
    villagers = [
        unit
        for unit in (diagnostics(driver) or {}).get("game", {}).get("units", [])
        if int(unit["owner"]) == owner and int(unit["kind"]) == 0
    ]
    villager_ids = {int(unit["id"]) for unit in villagers}
    selected_villager = None
    for villager in villagers:
        try:
            audited_world_pointer(
                journey, driver, actions, actor,
                int(villager["x"]), int(villager["y"]), button=0,
            )
            selected_villager = wait_until(
                f"{actor} visible villager selection for construction",
                lambda villager=villager: (
                    selected if (selected := int(journey.telemetry().get(
                        "selectedUnit", 0
                    ))) == int(villager["id"]) else None
                ),
                timeout=1.0,
            )
            break
        except Exception:
            continue
    for _ in range(max(4, len(villager_ids) + 1)):
        if selected_villager is not None:
            break
        audited_key(driver, actions, actor, ".")
        try:
            selected_villager = wait_until(
                f"{actor} villager selection for construction",
                lambda: (
                    selected if (selected := int(journey.telemetry().get(
                        "selectedUnit", 0
                    ))) in villager_ids else None
                ),
                timeout=1.0,
            )
            break
        except Failure:
            continue
    if selected_villager is None:
        raise Failure(f"{actor} could not cycle to construction villager")
    # Root slot 0 opens economic buildings; economic slot 0 is House.
    # The root-page H hotkey is Garrison, so ordinary visible buttons are
    # required to exercise the actual construction UI without ambiguity.
    audited_command_button(driver, actions, actor, 0)
    audited_command_button(driver, actions, actor, 0)
    construction_started = None
    candidate_tiles = (
        ((15, 20), (15, 22), (8, 14), (14, 15)) if owner == 0 else
        ((32, 10), (33, 13), (29, 10), (30, 14))
    )
    for tile_x, tile_y in candidate_tiles:
        audited_world_pointer(
            journey, driver, actions, actor, tile_x, tile_y,
        )
        try:
            construction_started = wait_until(
                f"{actor} house construction",
                lambda: (
                    games if len(owner_buildings(games[0], owner)) >
                    initial_building_count else None
                ) if (games := matching_games(host, join)) else None,
                timeout=12.0,
            )
            break
        except Failure:
            continue
    if construction_started is None:
        raise Failure(f"{actor} could not place a house through production UI")
    constructed_ids = {
        int(building["id"])
        for building in owner_buildings(construction_started[0], owner)
    } - {
        int(building["id"])
        for building in owner_buildings(before_games[0], owner)
    }
    if not constructed_ids:
        raise Failure(f"{actor} construction lacks new building identity")
    wait_until(
        f"{actor} house completed",
        lambda: (
            games if all(
                int(building.get("constructionTicksRemaining", -1)) == 0
                for building in owner_buildings(games[0], owner)
                if int(building.get("id", -1)) in constructed_ids
            ) else None
        ) if (games := matching_games(host, join)) else None,
        timeout=WAIT_SECONDS * 3,
    )
    return {
        "owner": owner,
        "initialGold": initial_gold,
        "gatheredGold": int(gathered),
        "gatherFrames": gather_frames,
        "gatherRenderOracle": gather_oracle,
        "constructedBuildingIds": sorted(constructed_ids),
        "militaryCount": int(trained["blueMilitaryCount"]),
        "researched": bool(researched["manAtArmsResearched"]),
    }


def order_enemy_attack(
    journey: Journey,
    driver,
    actor: str,
    actions: list[dict[str, object]],
    maximum_units: int | None = None,
    target_name: str = "enemyTarget",
) -> int:
    owner = 0 if actor == "host" else 1
    military = [
        unit
        for unit in (diagnostics(driver) or {}).get("game", {}).get("units", [])
        if int(unit["owner"]) == owner and int(unit["kind"]) == 9
    ]
    if not military:
        raise Failure(f"{actor} lacks trained military for attack")
    if maximum_units is not None:
        military = military[:maximum_units]
    if bool(journey.telemetry().get("pendingBuilding", False)):
        audited_key(driver, actions, actor, Keys.ESCAPE)
        wait_until(
            f"{actor} build placement cancelled",
            lambda: True if not bool(
                journey.telemetry().get("pendingBuilding", False)
            ) else None,
            timeout=2.0,
        )
    pan_world_target_clear(
        journey, driver, actions, actor, target_name,
        edge_margin=8.0 if target_name == "enemyTarget" else 80.0,
    )
    if bool(journey.telemetry().get("pendingBuilding", False)):
        audited_key(driver, actions, actor, Keys.ESCAPE)
        wait_until(
            f"{actor} post-pan build placement cancelled",
            lambda: True if not bool(
                journey.telemetry().get("pendingBuilding", False)
            ) else None,
            timeout=2.0,
        )
    initial_hit_points = int(journey.telemetry()["enemyTotalHitPoints"])
    if bool(journey.telemetry().get("pendingBuilding", False)):
        audited_key(driver, actions, actor, Keys.ESCAPE)
        wait_until(
            f"{actor} attack build placement cancelled",
            lambda: True if not bool(
                journey.telemetry().get("pendingBuilding", False)
            ) else None,
            timeout=2.0,
        )
    ordered_ids: set[int] = set()
    military_ids = {int(unit["id"]) for unit in military}
    owned_unit_count = sum(
        1 for unit in (diagnostics(driver) or {}).get("game", {}).get(
            "units", []
        ) if int(unit["owner"]) == owner
    )
    for _ in range(max(owned_unit_count * 2, len(military) * 2)):
        audited_key(driver, actions, actor, ",")
        unit_id = int(journey.telemetry().get("selectedUnit", 0))
        if selectable_military_id(
            unit_id, military_ids, ordered_ids
        ) is None:
            continue
        if bool(journey.telemetry().get("pendingBuilding", False)):
            audited_key(driver, actions, actor, Keys.ESCAPE)
        audited_pointer(journey, actions, actor, target_name, button=2)
        ordered_ids.add(unit_id)
        if len(ordered_ids) == len(military):
            break
    if not ordered_ids:
        # The comma hotkey intentionally visits idle military only.  A newly
        # trained unit can still be following the Barracks rally order, so
        # select the telemetry-published military target directly.
        pan_world_target_clear(
            journey, driver, actions, actor, "military", edge_margin=80.0,
        )
        audited_pointer(journey, actions, actor, "military", button=0)
        unit_id = int(journey.telemetry().get("selectedUnit", 0))
        if selectable_military_id(
            unit_id, military_ids, ordered_ids
        ) is not None:
            # Panning to select rallying military can move enemy target far
            # outside canvas. Recenter target after selection, before issuing
            # production right-click.
            pan_world_target_clear(
                journey, driver, actions, actor, target_name,
                edge_margin=(8.0 if target_name == "enemyTarget" else 80.0),
            )
            audited_pointer(journey, actions, actor, target_name, button=2)
            ordered_ids.add(unit_id)
    if not ordered_ids:
        raise Failure(f"{actor} ordered only {len(ordered_ids)} military units")
    wait_until(
        f"{actor} enemy-building combat start",
        lambda: (
            hit_points if 0 <= (hit_points := int(journey.telemetry()[
                "enemyTotalHitPoints"
            ])) < initial_hit_points else None
        ),
        timeout=WAIT_SECONDS,
    )
    return initial_hit_points


def launch_attack_wave(
    journey: Journey, driver, actor: str, actions: list[dict[str, object]],
) -> None:
    # Keep survivors fighting, but add affordable reinforcements first.
    if int(journey.telemetry()["resources"]["food"]) < 60:
        starting_food = int(journey.telemetry()["resources"]["food"])
        pan_world_target_clear(journey, driver, actions, actor, "food")
        for _ in range(3):
            audited_key(driver, actions, actor, ".")
            audited_pointer(journey, actions, actor, "food", button=2)
        wait_until(
            f"{actor} replacement-wave food economy",
            lambda: value if int((value := journey.telemetry())[
                "resources"
            ]["food"]) >= max(60, starting_food + 60) else None,
            timeout=WAIT_SECONDS * 3,
        )
    before = int(journey.telemetry()["blueMilitaryCount"])
    resources = journey.telemetry()["resources"]
    affordable = min(int(resources["food"]) // 60,
                     int(resources["gold"]) // 20, 3)
    if affordable > 0:
        audited_key(driver, actions, actor, "2")
        try:
            selected = wait_until(
                f"{actor} Barracks control-group recall",
                lambda: int(journey.telemetry().get(
                    "selectedBuilding", 0
                )) or None,
                timeout=2.0,
            )
        except Failure:
            if before == 0:
                raise
            selected = None
        if selected:
            for _ in range(affordable):
                audited_key(driver, actions, actor, "m")
            wait_until(
                f"{actor} replacement military wave",
                lambda: value if int((value := journey.telemetry())[
                    "blueMilitaryCount"
                ]) > before else None,
                timeout=WAIT_SECONDS,
            )
    order_enemy_attack(
        journey, driver, actor, actions, target_name="enemyTownCenter"
    )


def launch(driver, base_url: str, mode: str, relays: str | None,
           match_reference: str = "", allied: bool = True) -> Journey:
    driver.get(
        f"{base_url}/aoe_web.html?scenario=nostr-visual&"
        "overlapCapture=/audit-overlap&overlapTick=0"
    )
    wait_until(
        f"{mode} browser storage",
        lambda: driver.execute_script(
            "return Module.storageReady === true && "
            "Module.HEAPU8 instanceof Uint8Array && "
            "!document.getElementById('start').hidden"
        ),
    )
    Select(driver.find_element(By.ID, "launch-mode")).select_by_value(mode)
    if relays is not None:
        relay_input = driver.find_element(By.ID, "relays")
        relay_input.send_keys(Keys.COMMAND, "a")
        relay_input.send_keys(relays)
    if mode == "join":
        reference_input = driver.find_element(By.ID, "match-reference")
        reference_input.send_keys(match_reference)
    if allied:
        driver.find_element(By.ID, "allied").click()
    driver.find_element(By.ID, "start").click()
    wait_until(
        f"{mode} Nostr initialization",
        lambda: diagnostics(driver),
        timeout=WAIT_SECONDS,
    )
    driver.execute_script("Module.browserRenderTelemetryEnabled = true")
    return Journey(driver, base_url, {})


def capture_browser_overlap(driver, root: Path, peer: str) -> int:
    """Export renderer-produced matched pixels from browser virtual FS."""
    virtual_root = "/audit-overlap"
    manifest_text = wait_until(
        f"{peer} overlap manifest",
        lambda: driver.execute_script(
            "if (!FS.analyzePath(arguments[0]).exists) return null;"
            "return FS.readFile(arguments[0], {encoding: 'utf8'});",
            virtual_root + "/manifest.json",
        ),
        timeout=WAIT_SECONDS,
    )
    source_manifest = json.loads(manifest_text)
    output_root = root / "overlap"
    output_root.mkdir(parents=True, exist_ok=True)
    aggregate_path = output_root / "manifest.json"
    aggregate = ({"schema_version": 1, "cases": []}
                 if not aggregate_path.is_file()
                 else json.loads(aggregate_path.read_text()))

    def export_image(source: str, destination: Path) -> None:
        encoded = driver.execute_script(
            "const bytes=FS.readFile(arguments[0]); let value='';"
            "const size=0x8000; for(let i=0;i<bytes.length;i+=size){"
            "value+=String.fromCharCode(...bytes.subarray(i,i+size));}"
            "return btoa(value);",
            virtual_root + "/" + source,
        )
        temporary = output_root / (destination.stem + ".source" +
                                   Path(source).suffix)
        temporary.write_bytes(base64.b64decode(encoded))
        with Image.open(temporary) as image:
            image.save(output_root / destination, format="PNG")
        temporary.unlink()

    actual_name = f"{peer}-gameplay.png"
    terrain_name = f"{peer}-terrain.png"
    export_image("actual.bmp", Path(actual_name))
    export_image("terrain.bmp", Path(terrain_name))
    count = 0
    for case in source_manifest.get("cases", []):
        if case.get("blocked_reason"):
            continue
        case_id = f"{peer}-{case['id']}"
        sprite_name = f"{case_id}-sprite.png"
        export_image(str(case["sprite"]), Path(sprite_name))
        aggregate["cases"].append({
            "id": case_id,
            "actual": actual_name,
            "terrain": terrain_name,
            "sprite": sprite_name,
            "x": int(case["x"]), "y": int(case["y"]),
            "metadata": case.get("metadata", {}),
        })
        count += 1
    aggregate_path.write_text(
        json.dumps(aggregate, indent=2, sort_keys=True) + "\n"
    )
    return count


def key_chord(driver, key: str, modifier: str | None = None) -> None:
    canvas = driver.find_element(By.ID, "canvas")
    canvas.click()
    if modifier == Keys.ALT:
        ActionChains(driver).key_down(Keys.ALT, canvas).pause(0.1) \
            .send_keys_to_element(canvas, key).pause(0.1) \
            .key_up(Keys.ALT, canvas).perform()
        return
    actions = ActionChains(driver)
    if modifier:
        actions.key_down(modifier)
    actions.send_keys(key)
    if modifier:
        actions.key_up(modifier)
    actions.perform()


def native_modified_digit(driver, digit: str, modifier: str) -> None:
    if modifier not in (Keys.CONTROL, Keys.SHIFT):
        raise ValueError("native digit modifier must be Control or Shift")
    modifier_key = "Control" if modifier == Keys.CONTROL else "Shift"
    modifier_code = "ControlLeft" if modifier == Keys.CONTROL else "ShiftLeft"
    modifier_value = 2 if modifier == Keys.CONTROL else 8
    virtual_key = 17 if modifier == Keys.CONTROL else 16
    driver.execute_cdp_cmd("Input.dispatchKeyEvent", {
        "type": "keyDown", "key": modifier_key, "code": modifier_code,
        "windowsVirtualKeyCode": virtual_key,
        "nativeVirtualKeyCode": virtual_key, "modifiers": modifier_value,
    })
    for event_type in ("keyDown", "keyUp"):
        driver.execute_cdp_cmd("Input.dispatchKeyEvent", {
            "type": event_type, "key": digit, "code": f"Digit{digit}",
            "windowsVirtualKeyCode": ord(digit),
            "nativeVirtualKeyCode": ord(digit), "modifiers": modifier_value,
        })
    driver.execute_cdp_cmd("Input.dispatchKeyEvent", {
        "type": "keyUp", "key": modifier_key, "code": modifier_code,
        "windowsVirtualKeyCode": virtual_key,
        "nativeVirtualKeyCode": virtual_key,
    })


def click_canvas_logical(driver, x: float, y: float,
                         button: int = 0, modifiers: int = 0) -> None:
    canvas = driver.find_element(By.ID, "canvas")
    rect = canvas.rect
    if modifiers:
        page_x = rect["x"] + x * rect["width"] / 1280.0
        page_y = rect["y"] + y * rect["height"] / 720.0
        for event_type in ("mousePressed", "mouseReleased"):
            driver.execute_cdp_cmd("Input.dispatchMouseEvent", {
                "type": event_type, "x": page_x, "y": page_y,
                "button": "left",
                "buttons": 1 if event_type == "mousePressed" else 0,
                "clickCount": 1, "modifiers": modifiers,
            })
        return
    mouse = PointerInput("mouse", "multiplayer UI")
    actions = ActionBuilder(driver, mouse=mouse)
    actions.pointer_action.move_to_location(
        round(rect["x"] + x * rect["width"] / 1280.0),
        round(rect["y"] + y * rect["height"] / 720.0),
    )
    actions.pointer_action.pointer_down(button=button)
    actions.pointer_action.pointer_up(button=button)
    actions.perform()


def set_relay_enabled(driver, relay_index: int, enabled: bool) -> None:
    session = driver.find_element(By.ID, "nostr-session-details")
    driver.execute_script("arguments[0].hidden = false", session)
    details = driver.find_element(By.ID, "relay-management")
    if not details.get_attribute("open"):
        driver.execute_script("arguments[0].open = true", details)
    selector = f"#relay-controls button[data-relay-index='{relay_index}']"
    for attempt in range(10):
        try:
            button = driver.find_element(By.CSS_SELECTOR, selector)
            current = button.get_attribute("data-enabled") == "true"
            if current != enabled:
                button.click()
            return
        except StaleElementReferenceException:
            if attempt == 9:
                raise


def matching_relay_state(host, join, *, disabled: int, status: int,
                         eose: int = 0, reason: int | None = None,
                         minimum_tick: int | None = None):
    states = [diagnostics(host) or {}, diagnostics(join) or {}]
    games = [state.get("game") or {} for state in states]
    if not all(
        len(state.get("disabledRelays", [])) == disabled and
        len(state.get("eoseRelays", [])) >= eose and
        int(game.get("reliabilityStatus", -1)) == status and
        (reason is None or int(game.get("reliabilityReason", -1)) == reason)
        for state, game in zip(states, games, strict=True)
    ):
        return None
    ticks = [int(game.get("currentTick", -1)) for game in games]
    if minimum_tick is not None and (
        min(ticks) < minimum_tick or len(set(ticks)) != 1
    ):
        return None
    return states


def require_quorum(driver, name: str) -> dict[str, object]:
    value = wait_until(
        f"{name} relay quorum and EOSE",
        lambda: (
            state
            if len((state := diagnostics(driver) or {}).get("eoseRelays", []))
            >= 2
            else None
        ),
        timeout=WAIT_SECONDS,
    )
    assert isinstance(value, dict)
    return value


def exercise_relay_chaos(host, join, relays: str) -> dict[str, object]:
    relay_count = len(relays.split(","))
    if relay_count < 3:
        raise Failure("relay chaos requires at least three relays")
    recovery: dict[str, object] = {
        "before": {"host": diagnostics(host), "join": diagnostics(join)}
    }
    initial_tick = min(
        int((game_diagnostics(driver) or {}).get("currentTick", 0))
        for driver in (host, join)
    )
    for driver in (host, join):
        set_relay_enabled(driver, 0, False)
    one_relay_loss = wait_until(
        "continued lockstep with one relay disconnected",
        lambda: matching_relay_state(
            host, join, disabled=1, status=0, eose=2,
            minimum_tick=initial_tick + 2,
        ),
        timeout=WAIT_SECONDS,
    )
    recovery["oneRelayLoss"] = {
        "host": one_relay_loss[0], "join": one_relay_loss[1]
    }

    # Keep one relay connected: below quorum two, but still available for
    # deterministic restoration/backfill testing.
    for relay_index in range(1, relay_count - 1):
        for driver in (host, join):
            set_relay_enabled(driver, relay_index, False)
    quorum_loss = wait_until(
        "quorum-loss suspension",
        lambda: matching_relay_state(
            host, join, disabled=relay_count - 1, status=2, reason=5,
        ),
        timeout=WAIT_SECONDS,
    )
    stopped_ticks = [
        int((state.get("game") or {}).get("currentTick", -1))
        for state in quorum_loss
    ]
    time.sleep(2.0)
    if [
        int((game_diagnostics(driver) or {}).get("currentTick", -2))
        for driver in (host, join)
    ] != stopped_ticks:
        raise Failure("lockstep advanced while relay quorum was lost")
    recovery["quorumLoss"] = {
        "host": quorum_loss[0], "join": quorum_loss[1],
        "stableTicks": stopped_ticks,
    }

    # Restore the full production relay pool before requiring active status.
    # Two EOSE relays satisfy transport quorum, but failed turn publications
    # can still require observations from the original pool before the
    # runtime can leave backfill_incomplete.
    for relay_index in range(relay_count):
        for driver in (host, join):
            set_relay_enabled(driver, relay_index, True)
    recovered = wait_until(
        "relay EOSE backfill and lockstep recovery",
        lambda: matching_relay_state(
            host, join, disabled=0, status=0, eose=relay_count,
        ),
        timeout=WAIT_SECONDS,
    )
    recovery["recovered"] = {
        "host": recovered[0], "join": recovered[1]
    }
    wait_until(
        "all configured relays restored through EOSE",
        lambda: (
            True if all(
                not (state := diagnostics(driver) or {}).get(
                    "disabledRelays"
                ) and len(state.get("eoseRelays", [])) >= relay_count
                for driver in (host, join)
            ) else None
        ),
        timeout=WAIT_SECONDS,
    )
    resumed_tick = int((recovered[0].get("game") or {}).get(
        "currentTick", -1
    ))
    for driver, owner, actor in ((host, 0, "host"), (join, 1, "join")):
        journey = Journey(driver, "", {})
        audited_key(driver, [], actor, ".")
        click_canvas_logical(driver, 640.0, 300.0, button=2)
    wait_until(
        "post-recovery ordinary input advances lockstep",
        lambda: matching_relay_state(
            host, join, disabled=0, status=0, eose=relay_count,
            minimum_tick=resumed_tick + 1,
        ),
        timeout=WAIT_SECONDS,
    )
    recovery["allRestored"] = {
        "host": diagnostics(host), "join": diagnostics(join)
    }
    for driver in (host, join):
        driver.execute_script(
            "arguments[0].hidden = true",
            driver.find_element(By.ID, "nostr-session-details"),
        )
    return recovery


def run(relays: str | None, headed: bool, port: int = 8888,
        checkpoint: bool = False,
        artifact_dir: Path | None = None) -> dict[str, object]:
    if artifact_dir is None:
        artifact_dir = allocate_audit_directory()
    if not (DIST / "aoe_web.html").exists():
        raise Failure("packaged browser distribution is missing")
    artifact_dir.mkdir(parents=True, exist_ok=True)
    evidence: dict[str, object] = {
        "relays": [],
        "relaySource": ("explicit-override" if relays is not None
                        else "packaged-production-default"),
        "actions": [],
    }
    actions = evidence["actions"]
    host = make_driver("chrome", headed)
    join = make_driver("chrome", headed)
    evidence["browser"] = {
        "host": host.capabilities,
        "join": join.capabilities,
    }
    with static_server(port) as (base_url, requests):
        host_journey: Journey | None = None
        join_journey: Journey | None = None
        try:
            host_journey = launch(host, base_url, "host", relays)
            active_relays = str(
                host.find_element(By.ID, "relays").get_attribute("value") or ""
            )
            if not active_relays:
                raise Failure("production launch form has no relays")
            evidence["relays"] = active_relays.split(",")
            host_state = require_quorum(host, "host")
            evidence.setdefault("overlapEvidence", {})["host"] = \
                capture_browser_overlap(host, artifact_dir, "host")
            reference = str(host_state.get("matchReference", ""))
            if not reference.startswith("aoe-nostr:1:"):
                raise Failure(f"invalid host match reference: {reference!r}")

            join_journey = launch(
                join, base_url, "join", active_relays, reference
            )
            require_quorum(join, "join")
            evidence.setdefault("overlapEvidence", {})["join"] = \
                capture_browser_overlap(join, artifact_dir, "join")
            wait_until(
                "canonical lobby revision 2",
                lambda: (
                    [host_game, join_game]
                    if int((host_game := game_diagnostics(host) or {}).get(
                        "lobbyRevision", 0
                    )) >= 2 and int((join_game := game_diagnostics(join) or {}).get(
                        "lobbyRevision", 0
                    )) >= 2
                    else None
                ),
                timeout=WAIT_SECONDS,
            )

            key_chord(host, "r")
            key_chord(join, "r")
            wait_until(
                "both exact-lobby ready events",
                lambda: (
                    True
                    if all(
                        bool((game_diagnostics(driver) or {}).get("blueReady"))
                        and bool((game_diagnostics(driver) or {}).get("redReady"))
                        for driver in (host, join)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            click_canvas_logical(host, 842, 516)
            wait_until(
                "deterministic lockstep tick exchange",
                lambda: (
                    [host_game, join_game]
                    if int((host_game := game_diagnostics(host) or {}).get(
                        "currentTick", 0
                    )) >= 8 and int((join_game := game_diagnostics(join) or {}).get(
                        "currentTick", 0
                    )) >= 8
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            collapse_match_details(host, actions, "host")
            collapse_match_details(join, actions, "join")
            # Production F4 hides in-canvas lockstep/chat/signal panels.
            # Save-browser routing must not consume this multiplayer control.
            audited_key(host, actions, "host", Keys.F4)
            audited_key(join, actions, "join", Keys.F4)

            # Normal world input creates a non-empty lockstep turn batch.
            movement_before = [game_diagnostics(driver) or {}
                               for driver in (host, join)]
            before_positions = [blue_villager_positions(game)
                                for game in movement_before]
            if (
                before_positions[0] != before_positions[1] or
                not before_positions[0]
            ):
                raise Failure(
                    f"peers lack matching blue villager: {before_positions}"
                )
            host_candidate = min(before_positions[0])
            host_candidate_start = before_positions[0][host_candidate]
            center_camera_for_tile(
                host_journey, host, actions, "host",
                host_candidate_start[0] + 1, host_candidate_start[1],
            )
            audited_key(host, actions, "host", ".")
            selected_id = wait_until(
                "host selected observed blue villager",
                lambda: (
                    unit_id
                    if (unit_id := int(host_journey.telemetry().get(
                        "selectedUnit", 0
                    ))) in before_positions[0]
                    else None
                ),
            )
            host_start = before_positions[0][selected_id]
            host_destination = (host_start[0] + 1, host_start[1])
            audited_world_pointer(
                host_journey, host, actions, "host", *host_destination
            )
            host_motion_frames = capture_correlated_frames(
                host, join, artifact_dir=artifact_dir, label="host-move"
            )
            movement_after = wait_until(
                "matching world movement on both peers",
                lambda: matching_moved_villager(
                    host, join, selected_id,
                    before_positions[0][selected_id],
                ),
                timeout=WAIT_SECONDS,
            )
            evidence["movement"] = {
                "unitId": selected_id,
                "before": {
                    "host": movement_before[0],
                    "join": movement_before[1],
                },
                "after": {
                    "host": movement_after[0],
                    "join": movement_after[1],
                },
                "frames": host_motion_frames,
                "renderOracle": analyze_render_samples_for_audit(
                    host_motion_frames, "host-move"
                ),
            }

            # Joiner uses local-player-relative production telemetry and sends
            # a distinct Red command through the same visible canvas path.
            red_before_games = [game_diagnostics(driver) or {}
                                for driver in (host, join)]
            red_before = [owned_unit_positions(game, 1)
                          for game in red_before_games]
            if red_before[0] != red_before[1] or not red_before[0]:
                raise Failure(f"peers lack matching red unit: {red_before}")
            join_candidate = min(red_before[0])
            join_candidate_start = red_before[0][join_candidate]
            center_camera_for_tile(
                join_journey, join, actions, "join",
                join_candidate_start[0] - 1, join_candidate_start[1],
            )
            audited_key(join, actions, "join", ".")
            red_selected_id = wait_until(
                "join selected observed red villager",
                lambda: (
                    unit_id
                    if (unit_id := int(join_journey.telemetry().get(
                        "selectedUnit", 0
                    ))) in red_before[0]
                    else None
                ),
            )
            join_start = red_before[0][red_selected_id]
            join_destination = (join_start[0] - 1, join_start[1])
            audited_world_pointer(
                join_journey, join, actions, "join", *join_destination
            )
            join_motion_frames = capture_correlated_frames(
                host, join, artifact_dir=artifact_dir, label="join-move"
            )
            red_after = wait_until(
                "matching join world movement on both peers",
                lambda: matching_moved_owned_unit(
                    host, join, 1, red_selected_id,
                    red_before[0][red_selected_id],
                ),
                timeout=WAIT_SECONDS,
            )
            evidence["joinMovement"] = {
                "unitId": red_selected_id,
                "before": {
                    "host": red_before_games[0],
                    "join": red_before_games[1],
                },
                "after": {
                    "host": red_after[0],
                    "join": red_after[1],
                },
                "frames": join_motion_frames,
                "renderOracle": analyze_render_samples_for_audit(
                    join_motion_frames, "join-move"
                ),
            }

            simultaneous_before_games = [game_diagnostics(driver) or {}
                                         for driver in (host, join)]
            simultaneous_blue = owned_unit_positions(
                simultaneous_before_games[0], 0
            )
            simultaneous_red = owned_unit_positions(
                simultaneous_before_games[0], 1
            )
            audited_key(host, actions, "host", ".")
            audited_key(join, actions, "join", ".")
            wait_until(
                "simultaneous owner-resolved villager selections",
                lambda: (
                    [blue_id, red_id]
                    if (blue_id := int(host_journey.telemetry().get(
                        "selectedUnit", 0
                    ))) in simultaneous_blue and
                    (red_id := int(join_journey.telemetry().get(
                        "selectedUnit", 0
                    ))) in simultaneous_red else None
                ),
            )
            simultaneous_blue_id = int(
                host_journey.telemetry().get("selectedUnit", 0)
            )
            simultaneous_red_id = int(
                join_journey.telemetry().get("selectedUnit", 0)
            )
            if (simultaneous_blue_id not in simultaneous_blue or
                    simultaneous_red_id not in simultaneous_red):
                raise Failure("simultaneous selections did not resolve owners")
            audited_world_pointer(
                host_journey, host, actions, "host", 14, 22,
            )
            audited_world_pointer(
                join_journey, join, actions, "join", 34, 8,
            )
            simultaneous_frames = capture_correlated_frames(
                host, join, artifact_dir=artifact_dir,
                label="simultaneous-move",
            )

            def simultaneous_commands_applied():
                games = [game_diagnostics(driver) or {}
                         for driver in (host, join)]
                if games[0].get("stateHash") != games[1].get("stateHash"):
                    return None
                blue = [owned_unit_positions(game, 0) for game in games]
                red = [owned_unit_positions(game, 1) for game in games]
                if blue[0] != blue[1] or red[0] != red[1]:
                    return None
                if (blue[0].get(simultaneous_blue_id) ==
                        simultaneous_blue[simultaneous_blue_id] or
                        red[0].get(simultaneous_red_id) ==
                        simultaneous_red[simultaneous_red_id]):
                    return None
                return games

            simultaneous_after = wait_until(
                "simultaneous opposing world commands",
                simultaneous_commands_applied,
                timeout=WAIT_SECONDS,
            )
            evidence["simultaneousMovement"] = {
                "blueUnitId": simultaneous_blue_id,
                "redUnitId": simultaneous_red_id,
                "before": simultaneous_before_games,
                "after": simultaneous_after,
                "frames": simultaneous_frames,
                "renderOracle": analyze_render_samples_for_audit(
                    simultaneous_frames, "simultaneous-move"
                ),
            }

            # Slow speed is ordinary negotiated multiplayer control. It keeps
            # four-tile route segments drawable long enough for three exact
            # correlated captures per required direction cell.
            key_chord(host, Keys.F8)
            wait_until(
                "negotiated fast speed before slow direction capture",
                lambda: True if all(
                    int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
                    == 2 for driver in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            key_chord(host, Keys.F8)
            wait_until(
                "negotiated slow speed for direction capture",
                lambda: True if all(
                    int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
                    == 0 for driver in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            evidence["allDirections"] = {
                "host": exercise_all_direction_route(
                    host_journey, host, "host", 0,
                    join_journey, join, "join",
                    host, join, actions, artifact_dir, (20, 12),
                ),
                "join": exercise_all_direction_route(
                    join_journey, join, "join", 1,
                    host_journey, host, "host",
                    host, join, actions, artifact_dir, (28, 12),
                ),
            }
            key_chord(host, Keys.F8)
            wait_until(
                "restored normal speed after direction capture",
                lambda: True if all(
                    int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
                    == 1 for driver in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )

            evidence["fullGameplay"] = {
                "host": prepare_player_for_full_match(
                    host_journey, host, "host", 0, host, join, actions,
                    artifact_dir,
                ),
                "join": prepare_player_for_full_match(
                    join_journey, join, "join", 1, host, join, actions,
                    artifact_dir,
                ),
            }
            host_target_hit_points = order_enemy_attack(
                host_journey, host, "host", actions, maximum_units=3,
                target_name="enemyTownCenter",
            )
            join_target_hit_points = order_enemy_attack(
                join_journey, join, "join", actions, maximum_units=3,
                target_name="enemyTownCenter",
            )
            combat_frames = capture_correlated_frames(
                host, join, seconds=2.0, artifact_dir=artifact_dir,
                label="two-sided-town-center-combat",
            )
            evidence["fullGameplay"]["combat"] = {
                "hostEnemyTownCenterInitialHitPoints":
                    host_target_hit_points,
                "joinEnemyTownCenterInitialHitPoints":
                    join_target_hit_points,
                "frames": combat_frames,
                "renderOracle": analyze_render_samples_for_audit(
                    combat_frames, "two-sided-town-center-combat"
                ),
            }

            # Transport chaos comes only after retained two-sided economy,
            # construction, production, research, motion, provenance, and
            # active combat evidence.
            evidence["recovery"] = exercise_relay_chaos(
                host, join, active_relays
            )

            # Normal chat input becomes public side-channel state on both peers.
            key_chord(join, Keys.ENTER)
            join.find_element(By.ID, "canvas").send_keys("public relay hello")
            key_chord(join, Keys.ENTER)
            wait_until(
                "public chat delivery",
                lambda: (
                    True
                    if all(int((game_diagnostics(driver) or {}).get(
                        "chatCount", 0
                    )) >= 1 for driver in (host, join))
                    else None
                ),
                timeout=WAIT_SECONDS,
            )

            # Visible signal control then a world click is the normal UI path.
            # Earlier gameplay deliberately hid this overlay with F4.
            audited_key(host, actions, "host", Keys.F4)
            signal_delivered = False
            for attempt in range(3):
                pan_world_target_clear(
                    host_journey, host, actions, "host", "townCenter"
                )
                for _ in range(32):
                    town_center = host_journey.telemetry()["targets"][
                        "townCenter"
                    ]
                    if float(town_center["y"]) < 285.0:
                        break
                    audited_held_key(
                        host, actions, "host", Keys.ARROW_DOWN
                    )
                else:
                    raise Failure(
                        "host Town Center remained behind signal overlay"
                    )
                for signal_y in (326.0, 338.0, 346.0):
                    click_canvas_logical(host, 1165.0, signal_y)
                    try:
                        wait_until(
                            "public map signal armed",
                            lambda: True if bool(
                                host_journey.telemetry().get(
                                    "pendingMapSignal", False
                                )
                            ) else None,
                            timeout=1.0,
                        )
                        break
                    except Failure:
                        continue
                else:
                    raise Failure("visible public map signal button missed")
                time.sleep(0.25)
                if attempt == 0:
                    host.save_screenshot(str(artifact_dir / "signal-armed.png"))
                audited_pointer(
                    host_journey, actions, "host", "townCenter", button=0
                )
                try:
                    wait_until(
                        "public map signal delivery",
                        lambda: (
                            True
                            if all(int((game_diagnostics(driver) or {}).get(
                                "signalCount", 0
                            )) >= 1 for driver in (host, join))
                            else None
                        ),
                        timeout=WAIT_SECONDS / 3,
                    )
                    signal_delivered = True
                    break
                except Failure:
                    if attempt == 2:
                        raise
            if not signal_delivered:
                raise Failure("public map signal delivery retry exhausted")

            # F8 negotiates speed and F6 negotiates a save/hash barrier.
            prior_speed = int((game_diagnostics(host) or {}).get("gameSpeed", 0))
            key_chord(host, Keys.F8)
            wait_until(
                "committed speed control",
                lambda: (
                    speed
                    if (speed := int((game_diagnostics(host) or {}).get(
                        "gameSpeed", prior_speed
                    ))) != prior_speed and speed == int(
                        (game_diagnostics(join) or {}).get("gameSpeed", prior_speed)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            key_chord(host, Keys.F7)
            paused = wait_until(
                "committed pause control",
                lambda: (
                    [host_game, join_game]
                    if bool((host_game := game_diagnostics(host) or {}).get(
                        "paused", False
                    )) and bool((join_game := game_diagnostics(join) or {}).get(
                        "paused", False
                    )) and int(host_game.get("currentTick", -1)) == int(
                        join_game.get("currentTick", -2)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            pause_tick = int(paused[0]["currentTick"])
            key_chord(host, Keys.F7)
            wait_until(
                "committed resume control",
                lambda: (
                    True
                    if all(
                        not bool((game_diagnostics(driver) or {}).get("paused"))
                        and int((game_diagnostics(driver) or {}).get(
                            "currentTick", 0
                        )) > pause_tick
                        for driver in (host, join)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            if checkpoint:
                key_chord(host, Keys.F6)
                wait_until(
                    "matched public checkpoint digest",
                    lambda: (
                        True
                        if all(int((game_diagnostics(driver) or {}).get(
                            "stateHashStatus", 0
                        )) == 2 for driver in (host, join))
                        else None
                    ),
                    timeout=WAIT_SECONDS,
                )
                evidence.update({
                    "host": diagnostics(host) or {},
                    "join": diagnostics(join) or {},
                    "requests": list(requests),
                    "hostConsole": host.get_log("browser"),
                    "joinConsole": join.get_log("browser"),
                })
                host.save_screenshot(str(artifact_dir / "checkpoint-host.png"))
                join.save_screenshot(str(artifact_dir / "checkpoint-join.png"))
                return evidence
            audited_key(host, actions, "host", Keys.F4)
            terminal = None
            for _ in range(12):
                if int((game_diagnostics(join) or {}).get("outcome", 0)) == 0:
                    launch_attack_wave(join_journey, join, "join", actions)
                try:
                    terminal = wait_until(
                        "agreed natural conquest result",
                        lambda: (
                    [host_game, join_game]
                    if int((host_game := game_diagnostics(host) or {}).get(
                        "outcome", 0
                    )) != 0 and int(host_game.get("outcome", 0)) == int(
                        (join_game := game_diagnostics(join) or {}).get(
                            "outcome", 0
                        )
                    ) and bool(host_game.get("terminalStateHash")) and
                    host_game.get("terminalStateHash") ==
                    join_game.get("terminalStateHash") and all(
                        int(game.get("resultCount", 0)) == 2 and
                        bool(game.get("terminalResultAgreement"))
                        for game in (host_game, join_game)
                    )
                            else None
                        ),
                        timeout=90.0,
                    )
                    break
                except Failure:
                    continue
            if terminal is None:
                raise Failure("natural conquest not reached after twelve waves")
            evidence["fullGameplay"]["naturalVictory"] = {
                "host": terminal[0], "join": terminal[1],
                "method": "opposing town-center destruction",
            }
            time.sleep(1.0)
            settled_ticks = [
                int((game_diagnostics(driver) or {}).get("currentTick", -1))
                for driver in (host, join)
            ]
            time.sleep(1.0)
            final_ticks = [
                int((game_diagnostics(driver) or {}).get("currentTick", -2))
                for driver in (host, join)
            ]
            if len(set(settled_ticks + final_ticks)) != 1:
                raise Failure("lockstep tick advanced after terminal result")

            host_final = diagnostics(host) or {}
            join_final = diagnostics(join) or {}
            evidence.update({
                "host": host_final,
                "join": join_final,
                "hostRender": render_diagnostics(host),
                "joinRender": render_diagnostics(join),
                "requests": list(requests),
                "hostConsole": host.get_log("browser"),
                "joinConsole": join.get_log("browser"),
            })
            (artifact_dir / "frames" / "host").mkdir(
                parents=True, exist_ok=True
            )
            (artifact_dir / "frames" / "join").mkdir(
                parents=True, exist_ok=True
            )
            host.save_screenshot(str(
                artifact_dir / "frames" / "host" / "terminal.png"
            ))
            join.save_screenshot(str(
                artifact_dir / "frames" / "join" / "terminal.png"
            ))
            return evidence
        except Exception as error:
            failure = {
                "error": f"{type(error).__name__}: {error}",
                "traceback": traceback.format_exc(),
                "completedEvidence": evidence,
                "relays": evidence.get("relays", []),
                "host": capture_failure_value(
                    "host diagnostics", lambda: diagnostics(host)
                ),
                "join": capture_failure_value(
                    "join diagnostics", lambda: diagnostics(join)
                ),
                "hostRender": capture_failure_value(
                    "host render diagnostics", lambda: render_diagnostics(host)
                ),
                "joinRender": capture_failure_value(
                    "join render diagnostics", lambda: render_diagnostics(join)
                ),
                "browser": evidence.get("browser"),
                "requests": list(requests),
                "hostConsole": capture_failure_value(
                    "host console", lambda: host.get_log("browser")
                ),
                "joinConsole": capture_failure_value(
                    "join console", lambda: join.get_log("browser")
                ),
            }
            (artifact_dir / "first-failure.json").write_text(
                json.dumps(failure, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            write_audit_bundle(artifact_dir, failure)
            capture_failure_value(
                "host screenshot",
                lambda: host.save_screenshot(
                    str(artifact_dir / "last-failure-host.png")
                ),
            )
            capture_failure_value(
                "join screenshot",
                lambda: join.save_screenshot(
                    str(artifact_dir / "last-failure-join.png")
                ),
            )
            raise
        finally:
            if host_journey is not None:
                host_journey.evidence.clear()
            if join_journey is not None:
                join_journey.evidence.clear()
            host.quit()
            join.quit()


def write_jsonl(path: Path, records: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(json.dumps(record, sort_keys=True) + "\n"
                for record in records),
        encoding="utf-8",
    )


def write_audit_bundle(root: Path, evidence: dict[str, object]) -> None:
    root.mkdir(parents=True, exist_ok=True)
    package = DIST / "aoe_web.html"
    commit = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
        capture_output=True, text=True,
    ).stdout.strip()
    package_files = sorted({
        *DIST.glob("aoe_web.*"), DIST / "aoe_nostr.js",
    })
    package_digests = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in package_files if path.is_file()
    }
    source_paths = [
        ROOT / "include/aoe/browser_telemetry.hpp",
        ROOT / "src/browser_telemetry_native.cpp",
        ROOT / "src/browser_telemetry_web.cpp",
        ROOT / "src/nostr_multiplayer_runtime.cpp",
        ROOT / "src/sdl_app.cpp",
        ROOT / "tests/web/nostr_multiplayer_smoke_test.py",
        ROOT / "tests/web/test_nostr_multiplayer_audit_tools.py",
        ROOT / "tools/nostr_visual_frame_oracle.py",
        ROOT / "tools/nostr_visual_coverage.py",
        ROOT / "resources/nostr-visual-gameplay-coverage.json",
    ]
    source_digests = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in source_paths
    }
    host = evidence.get("host", {})
    join = evidence.get("join", {})
    existing_ledger = (
        json.loads((root / "run.json").read_text(encoding="utf-8"))
        if (root / "run.json").is_file() else {}
    )
    run_ledger = {
        **existing_ledger,
        "schemaVersion": 2,
        "status": "FINALIZING",
        "completedUtc": datetime.now(timezone.utc).isoformat(),
        "sourceCommit": commit,
        "package": str(package.relative_to(ROOT)),
        "packageSha256": package_digests,
        "sourceFilesSha256": source_digests,
        "browser": evidence.get("browser"),
        "relays": evidence.get("relays", []),
        "relaySource": evidence.get("relaySource"),
        "hostPublicKey": (host.get("publicKey") if isinstance(host, dict)
                          else existing_ledger.get("hostPublicKey")),
        "joinPublicKey": (join.get("publicKey") if isinstance(join, dict)
                          else existing_ledger.get("joinPublicKey")),
        "matchReference": host.get("matchReference") if isinstance(host, dict) else None,
        "lobbyRevision": ((host.get("game") or {}).get("lobbyRevision")
                          if isinstance(host, dict) else None),
        "scenarioDigest": ((host.get("game") or {}).get("scenarioDigest")
                           if isinstance(host, dict) else None),
        "tickCadenceMs": ((host.get("game") or {}).get("tickCadenceMs")
                          if isinstance(host, dict) else None),
        "hostDisplay": ((evidence.get("hostRender") or {}).get("display")
                        if isinstance(evidence.get("hostRender"), dict)
                        else None),
        "joinDisplay": ((evidence.get("joinRender") or {}).get("display")
                        if isinstance(evidence.get("joinRender"), dict)
                        else None),
        "mapSeed": None,
        "actionSeed": None,
        "transportFaultSeed": None,
        "randomized": False,
        "assertions": [
            "distinct identities", "equal tick and state hash",
            "contiguous sender sequences", "both players issue world commands",
            "quorum loss stops both peers", "EOSE restore resumes both peers",
            "terminal result agrees and tick freezes",
        ],
    }
    atomic_write_json(root / "run.json", run_ledger)
    actions = evidence.get("actions") or [
        {"phase": "movement", "actor": "host", "kind": "move",
         "unitId": (evidence.get("movement") or {}).get("unitId")},
        {"phase": "movement", "actor": "join", "kind": "move",
         "unitId": (evidence.get("joinMovement") or {}).get("unitId")},
        {"phase": "simultaneous", "actor": "host", "kind": "move",
         "unitId": (evidence.get("simultaneousMovement") or {}).get(
             "blueUnitId")},
        {"phase": "simultaneous", "actor": "join", "kind": "move",
         "unitId": (evidence.get("simultaneousMovement") or {}).get(
             "redUnitId")},
        {"phase": "side-channel", "actor": "join", "kind": "chat"},
        {"phase": "side-channel", "actor": "host", "kind": "signal"},
        {"phase": "control", "actor": "host", "kind": "speed-pause-resume"},
        {"phase": "terminal", "actor": "both", "kind": "natural-conquest"},
    ]
    write_jsonl(root / "actions.jsonl", actions)
    recovery = evidence.get("recovery") or {}
    write_jsonl(root / "transport.jsonl", [
        {"phase": phase, "state": state}
        for phase, state in recovery.items()
    ])
    host_states: list[dict[str, object]] = []
    join_states: list[dict[str, object]] = []
    for phase, state in recovery.items():
        if isinstance(state, dict):
            if isinstance(state.get("host"), dict):
                host_states.append({"phase": phase, "state": state["host"]})
            if isinstance(state.get("join"), dict):
                join_states.append({"phase": phase, "state": state["join"]})
    for phase in ("movement", "joinMovement", "simultaneousMovement"):
        phase_value = evidence.get(phase) or {}
        for sample in phase_value.get("frames", []):
            if isinstance(sample.get("authoritativeHost"), dict):
                host_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeHost"],
                })
            if isinstance(sample.get("authoritativeJoin"), dict):
                join_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeJoin"],
                })
    all_directions = evidence.get("allDirections") or {}
    for actor in ("host", "join"):
        phase = f"allDirections{actor.title()}"
        for sample in (all_directions.get(actor) or {}).get("frames", []):
            if isinstance(sample.get("authoritativeHost"), dict):
                host_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeHost"],
                })
            if isinstance(sample.get("authoritativeJoin"), dict):
                join_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeJoin"],
                })
    full_gameplay = evidence.get("fullGameplay") or {}
    combat = full_gameplay.get("combat") or {}
    gather_phases = {
        f"fullGameplay{actor.title()}Gather":
            (full_gameplay.get(actor) or {}).get("gatherFrames", [])
        for actor in ("host", "join")
    }
    for phase, samples in gather_phases.items():
        for sample in samples:
            if isinstance(sample.get("authoritativeHost"), dict):
                host_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeHost"],
                })
            if isinstance(sample.get("authoritativeJoin"), dict):
                join_states.append({
                    "phase": phase,
                    "capturedMonotonic": sample.get("capturedMonotonic"),
                    "state": sample["authoritativeJoin"],
                })
    for sample in combat.get("frames", []):
        if isinstance(sample.get("authoritativeHost"), dict):
            host_states.append({
                "phase": "fullGameplayCombat",
                "capturedMonotonic": sample.get("capturedMonotonic"),
                "state": sample["authoritativeHost"],
            })
        if isinstance(sample.get("authoritativeJoin"), dict):
            join_states.append({
                "phase": "fullGameplayCombat",
                "capturedMonotonic": sample.get("capturedMonotonic"),
                "state": sample["authoritativeJoin"],
            })
    if isinstance(host, dict):
        host_states.append({"phase": "final", "state": host})
    if isinstance(join, dict):
        join_states.append({"phase": "final", "state": join})
    write_jsonl(root / "states" / "host.jsonl", host_states)
    write_jsonl(root / "states" / "join.jsonl", join_states)
    motion = {
        "hostMovement": evidence.get("movement"),
        "joinMovement": evidence.get("joinMovement"),
        "simultaneousMovement": evidence.get("simultaneousMovement"),
        "fullGameplayGather": gather_phases,
        "fullGameplayCombat": combat,
    }
    (root / "motion.json").write_text(
        json.dumps(motion, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    provenance: list[dict[str, object]] = []
    for phase in ("movement", "joinMovement", "simultaneousMovement"):
        phase_value = evidence.get(phase) or {}
        for sample in phase_value.get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": phase,
                    "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
    for actor in ("host", "join"):
        phase = f"allDirections{actor.title()}"
        for sample in (all_directions.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": phase,
                    "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
    for sample in combat.get("frames", []):
        for peer in ("host", "join"):
            render_state = sample.get(peer) or {}
            provenance.append({
                "phase": "fullGameplayCombat",
                "peer": peer,
                "frame": render_state.get("frame"),
                "tick": render_state.get("tick"),
                "entities": render_state.get("entities", []),
            })
    for phase, samples in gather_phases.items():
        for sample in samples:
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": phase,
                    "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
    write_jsonl(root / "sprite-provenance.jsonl", provenance)
    correlated_records: list[dict[str, object]] = []
    for phase in ("movement", "joinMovement", "simultaneousMovement"):
        for sample in (evidence.get(phase) or {}).get("frames", []):
            correlated_records.append({"phase": phase, **sample})
    for actor in ("host", "join"):
        phase = f"allDirections{actor.title()}"
        for sample in (all_directions.get(actor) or {}).get("frames", []):
            correlated_records.append({"phase": phase, **sample})
    for phase, samples in gather_phases.items():
        for sample in samples:
            correlated_records.append({"phase": phase, **sample})
    for sample in combat.get("frames", []):
        correlated_records.append({
            "phase": "fullGameplayCombat", **sample,
        })
    write_jsonl(root / "correlated-frames.jsonl", correlated_records)

    visual_oracles: list[dict[str, object]] = []

    def collect_oracles(value: object) -> None:
        if isinstance(value, dict):
            records = value.get("visualOracles")
            if isinstance(records, list):
                visual_oracles.extend(
                    record for record in records if isinstance(record, dict)
                )
            for child in value.values():
                collect_oracles(child)
        elif isinstance(value, list):
            for child in value:
                collect_oracles(child)

    collect_oracles(evidence)
    write_jsonl(root / "visual-oracles.jsonl", visual_oracles)

    coverage_specification = load_specification(
        ROOT / "resources" / "nostr-visual-gameplay-coverage.json"
    )
    coverage = evaluate_coverage(coverage_specification, visual_oracles)
    atomic_write_json(root / "coverage.json", coverage)
    (root / "console-host.json").write_text(
        json.dumps(evidence.get("hostConsole", []), indent=2) + "\n",
        encoding="utf-8",
    )
    (root / "console-join.json").write_text(
        json.dumps(evidence.get("joinConsole", []), indent=2) + "\n",
        encoding="utf-8",
    )
    (root / "visual-failures.json").write_text(
        json.dumps(visual_failures(evidence), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (root / "visual-findings.json").write_text(
        json.dumps(visual_findings(evidence), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    screenshot_report = audit_screenshots(root)
    (root / "screenshot-audit.json").write_text(
        json.dumps(screenshot_report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    failures = visual_failures(evidence)
    status = "FAIL" if failures else "BLOCKED"
    atomic_write_json(root / "verdict.json", {
        "schemaVersion": 1,
        "status": status,
        "visualFailureCount": len(failures),
        "visualOracleCount": len(visual_oracles),
        "coverageStatus": coverage["status"],
        "missingRequiredCells": coverage["missingRequiredCells"],
    })
    run_ledger["status"] = status
    run_ledger["finalizedUtc"] = datetime.now(timezone.utc).isoformat()
    atomic_write_json(root / "run.json", run_ledger)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--relays",
        help="explicit override; default uses packaged production relay list",
    )
    parser.add_argument("--port", type=int, default=8888)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--checkpoint", action="store_true")
    parser.add_argument(
        "--audit-root", type=Path, default=AUDIT_ROOT,
        help="parent for durable timestamped run artifact directories",
    )
    parser.add_argument("--report-root", type=Path,
                        default=AUDIT_REPORT_ROOT)
    parser.add_argument("--seed", type=int, default=0xA0E20260812)
    parser.add_argument("--retry-budget", type=int, default=3)
    arguments = parser.parse_args()
    destination = allocate_audit_destination(
        arguments.audit_root, arguments.report_root
    )
    audit_dir = destination.artifacts
    evidence_path = audit_dir / "evidence.json"
    initialize_run_ledger(
        destination,
        relays=arguments.relays,
        headed=arguments.headed,
        port=arguments.port,
        seed=arguments.seed,
        retry_budget=arguments.retry_budget,
    )
    try:
        evidence = run(
            arguments.relays, arguments.headed, arguments.port,
            checkpoint=arguments.checkpoint,
            artifact_dir=audit_dir,
        )
        evidence_path.write_text(
            json.dumps(evidence, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        write_audit_bundle(audit_dir, evidence)
    except Exception as error:
        failure_path = audit_dir / "first-failure.json"
        if failure_path.exists():
            failure = json.loads(failure_path.read_text(encoding="utf-8"))
        else:
            failure = {
                "error": f"{type(error).__name__}: {error}",
                "relays": (arguments.relays.split(",")
                           if arguments.relays else []),
            }
            failure_path.write_text(
                json.dumps(failure, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        evidence_path.write_text(
            json.dumps(failure, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        write_report(
            audit_dir, "BLOCKED",
            "Run stopped before acceptance completed. Infrastructure versus "
            "product classification remains unproved pending evidence review.\n\n"
            f"Primary failure: `{failure.get('error', str(error))}`",
            report_path=destination.report,
        )
        verdict_path = audit_dir / "verdict.json"
        current_verdict = (
            json.loads(verdict_path.read_text(encoding="utf-8"))
            if verdict_path.is_file() else {}
        )
        if current_verdict.get("status") == "RUNNING":
            atomic_write_json(verdict_path, {
                "schemaVersion": 1, "status": "BLOCKED",
                "failure": failure.get("error", str(error)),
            })
        print(f"Nostr multiplayer audit blocked: {audit_dir}", file=sys.stderr)
        return 1
    failures = visual_failures(evidence)
    if failures:
        write_report(
            audit_dir, "PROBLEMS FOUND",
            f"Automated oracles found {len(failures)} visual failure(s). "
            "See `visual-failures.json` and retained evidence.",
            report_path=destination.report,
        )
        print(
            f"Nostr multiplayer audit found {len(failures)} visual "
            f"failure(s): {audit_dir}"
        )
        return 1
    verdict = json.loads(
        (audit_dir / "verdict.json").read_text(encoding="utf-8")
    )
    status = str(verdict.get("status", "BLOCKED"))
    if status != "PASS":
        write_report(
            audit_dir, status,
            "Mandatory visual coverage remains incomplete. See "
            "`coverage.json` and `verdict.json`.",
            report_path=destination.report,
        )
        print(f"Nostr multiplayer audit {status.lower()}: {audit_dir}",
              file=sys.stderr)
        return 1
    write_report(
        audit_dir, "PASS", "Every mandatory coverage cell and oracle passed.",
        report_path=destination.report,
    )
    print(f"Nostr multiplayer audit passed: {audit_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

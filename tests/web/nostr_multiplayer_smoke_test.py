#!/usr/bin/env python3
"""Two-browser production-path smoke test over ordinary public Nostr relays."""

from __future__ import annotations

import argparse
import base64
import concurrent.futures
import hashlib
import json
import math
import re
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
from websocket import create_connection

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
from nostr_visual_frame_oracle import (
    FrameOracleError,
    evaluate_layer,
    logical_direction as oracle_logical_direction,
)
from nostr_visual_coverage import evaluate_coverage, load_specification
from nostr_packaged_pixel_oracle import (
    evaluate_packaged_capture,
    write_wrong_direction_mutation,
    write_wrong_position_mutation,
)
from nostr_visual_transition_oracle import evaluate_transitions
from nostr_visual_route_coverage import evaluate_route_coverage
from nostr_seeded_action_generator import (
    causal_replay_prefix,
    coverage_priority_directions,
    coverage_priority_plan,
)


ROOT = Path(__file__).resolve().parents[2]
AUDIT_ROOT = ROOT / "artifacts" / "nostr-e2e-visual"
AUDIT_REPORT_ROOT = ROOT / "docs" / "audits"
WAIT_SECONDS = 180.0
CDP_SHIFT_MODIFIER = 8
DEFAULT_RELAYS = (
    "wss://nostr-pub.wellorder.net", "wss://nostr.oxtr.dev",
    "wss://nostr.bond", "wss://relay.nostr.net", "wss://yabu.me",
    "wss://relay.nostr.wirednet.jp", "wss://relay.nostr.info",
    "wss://nostr.sathoarder.com", "wss://relay.wavlake.com",
    "wss://relay.noswhere.com",
)


class ActionLimitReached(Failure):
    """Candidate replay reached its prefix boundary without earlier abort."""


class InfrastructureBlocked(Failure):
    """External relay state prevented production journey progress."""


class BoundedActionLog(list[dict[str, object]]):
    def __init__(self, limit: int | None = None):
        super().__init__()
        if limit is not None and limit < 0:
            raise ValueError("action limit must be non-negative")
        self.limit = limit

    def append(self, value: dict[str, object]) -> None:
        if self.limit is not None and len(self) >= self.limit:
            raise ActionLimitReached(f"action limit {self.limit} reached")
        super().append(value)


@dataclass(frozen=True)
class AuditDestination:
    artifacts: Path
    report: Path
    run_id: str


def probe_relay_pool(
    relays: list[str], *, timeout: float = 5.0,
    connector=create_connection,
) -> dict[str, object]:
    """Probe WebSocket handshakes concurrently; select deterministic quorum."""
    normalized = list(dict.fromkeys(
        relay.strip().rstrip("/") for relay in relays if relay.strip()
    ))

    def probe(relay: str) -> dict[str, object]:
        started = time.monotonic()
        try:
            socket = connector(relay, timeout=timeout)
            socket.close()
            return {
                "relay": relay, "healthy": True,
                "latencyMs": round((time.monotonic() - started) * 1000, 3),
            }
        except Exception as error:
            return {
                "relay": relay, "healthy": False,
                "latencyMs": round((time.monotonic() - started) * 1000, 3),
                "error": f"{type(error).__name__}: {error}",
            }

    with concurrent.futures.ThreadPoolExecutor(
        max_workers=min(len(normalized), 10) or 1
    ) as executor:
        by_relay = {
            result["relay"]: result
            for result in executor.map(probe, normalized)
        }
    results = [by_relay[relay] for relay in normalized]
    healthy = [
        result["relay"] for result in results if result["healthy"]
    ]
    return {
        "schemaVersion": 1,
        "policy": "configured-order-first-three-healthy-v1",
        "results": results,
        "selectedQuorum": healthy[:3],
    }


def parse_viewport(value: str) -> tuple[int, int]:
    try:
        width_text, height_text = value.lower().split("x", 1)
        width, height = int(width_text), int(height_text)
    except (ValueError, AttributeError) as error:
        raise argparse.ArgumentTypeError(
            "viewport must be WIDTHxHEIGHT"
        ) from error
    if width < 640 or height < 480:
        raise argparse.ArgumentTypeError("viewport is below supported minimum")
    return width, height


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
    run_id: str | None = None,
) -> AuditDestination:
    """Atomically reserve durable report and artifact destinations."""
    artifact_root.mkdir(parents=True, exist_ok=True)
    report_root.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc)
    stamp = now.strftime("%Y%m%dT%H%M%SZ")
    report_day = now.strftime("%Y-%m-%d")
    if run_id is not None:
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", run_id):
            raise ValueError("audit destination id contains unsafe characters")
        artifacts = artifact_root / run_id
        artifacts.mkdir()
        report = report_root / (
            f"{report_day}-NOSTR-E2E-VISUAL-GAMEPLAY-{run_id}.md"
        )
    else:
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
    viewport: tuple[int, int] = (1280, 900),
    dpr: float = 1.0,
    browser_arguments: list[str] | None = None,
    zoom: float = 1.0,
    action_limit: int | None = None,
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
                    "versions": "captured after driver creation",
                    "arguments": list(browser_arguments or [])},
        "scenario": "packaged Nostr multiplayer production scenario",
        "seed": seed,
        "actionLimit": action_limit,
        "hostPublicKey": "pending product identity initialization",
        "joinPublicKey": "pending product identity initialization",
        "relayPool": relays.split(",") if relays else
            "packaged production defaults",
        "selectedQuorum": [],
        "retryBudget": retry_budget,
        "serverPort": port,
        "viewport": {"width": viewport[0], "height": viewport[1]},
        "dpr": dpr,
        "zoom": zoom,
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
        "renderFrame": int(telemetry.get("frame", -1)),
        "targetTelemetry": dict(point),
    })
    journey.pointer(target, button, logical_dx, logical_dy)


def audited_key(driver, actions: list[dict[str, object]], actor: str,
                key: str) -> None:
    try:
        render = render_diagnostics(driver) or {}
    except Exception:
        render = {}
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "key",
        "key": key,
        "telemetryTick": int(render.get("tick", -1)),
        "renderFrame": int(render.get("frame", -1)),
    })
    driver.find_element(By.ID, "canvas").send_keys(key)


def audited_held_key(driver, actions: list[dict[str, object]], actor: str,
                     key: str, seconds: float = 0.15) -> None:
    try:
        render = render_diagnostics(driver) or {}
    except Exception:
        render = {}
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "held-key",
        "key": key,
        "seconds": seconds,
        "telemetryTick": int(render.get("tick", -1)),
        "renderFrame": int(render.get("frame", -1)),
    })
    canvas = driver.find_element(By.ID, "canvas")
    ActionChains(driver).key_down(key, canvas).pause(seconds).key_up(
        key, canvas
    ).perform()


def audited_zoom(
    journey: Journey, driver, actions: list[dict[str, object]], actor: str,
    target_zoom: float,
) -> None:
    if target_zoom not in (1.0, 2.0):
        raise ValueError("audit zoom must be supported minimum or maximum")
    for _ in range(16):
        current = float(journey.telemetry()["camera"]["zoom"])
        if abs(current - target_zoom) < 0.001:
            return
        delta = -100 if current < target_zoom else 100
        actions.append({
            "monotonic": time.monotonic(), "actor": actor,
            "kind": "wheel", "deltaY": delta,
            "targetZoom": target_zoom,
            "telemetryTick": int(journey.telemetry()["tick"]),
            "sourceZoom": current,
        })
        canvas = driver.find_element(By.ID, "canvas")
        rect = canvas.rect
        driver.execute_cdp_cmd("Input.dispatchMouseEvent", {
            "type": "mouseMoved",
            "x": float(rect["x"]) + float(rect["width"]) / 2.0,
            "y": float(rect["y"]) + float(rect["height"]) / 2.0,
        })
        driver.execute_cdp_cmd("Input.dispatchMouseEvent", {
            "type": "mouseWheel",
            "x": float(rect["x"]) + float(rect["width"]) / 2.0,
            "y": float(rect["y"]) + float(rect["height"]) / 2.0,
            "deltaX": 0,
            "deltaY": delta,
        })
        time.sleep(0.1)
    raise Failure(f"{actor} camera did not reach zoom {target_zoom}")


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
                edge_margin < y < 520.0 - edge_margin:
            return
        if y >= 520.0 - edge_margin:
            key = Keys.ARROW_DOWN
        elif y <= edge_margin:
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
    try:
        render = render_diagnostics(driver) or {}
    except Exception:
        render = {}
    actions.append({
        "monotonic": time.monotonic(),
        "actor": actor,
        "kind": "command-button",
        "gridSlot": grid_slot,
        "targetLogicalX": logical_x,
        "targetLogicalY": logical_y,
        "telemetryTick": int(render.get("tick", -1)),
        "renderFrame": int(render.get("frame", -1)),
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
        "renderFrame": int(telemetry.get("frame", -1)),
        "camera": {
            "x": float(camera["x"]), "y": float(camera["y"]),
            "zoom": zoom,
        },
        "modifiers": modifiers,
    })
    click_canvas_logical(
        driver, logical_x, logical_y, button=button, modifiers=modifiers
    )


def canonical_direction_route(
    center: tuple[int, int], radius: int = 4,
    direction_order: list[int] | None = None,
) -> list[tuple[int, int]]:
    """Return closed octagonal route in the requested 8-way command order."""
    if radius < 1:
        raise ValueError("route radius must be positive")
    vectors = (
        (1, 1), (0, 1), (-1, 1), (-1, 0),
        (-1, -1), (0, -1), (1, -1), (1, 0),
    )
    order = list(range(8)) if direction_order is None else direction_order
    if sorted(order) != list(range(8)):
        raise ValueError("direction order must be a permutation of 0..7")
    points = [center]
    x, y = center
    for direction in order:
        dx, dy = vectors[direction]
        x += dx * radius
        y += dy * radius
        points.append((x, y))
    return points


def deterministic_replacement_destination(
    start: tuple[int, int], attempted: tuple[int, int], seed: int,
    owner: int, replacement_index: int = 0,
) -> tuple[int, int]:
    """Choose a stable adjacent visible-command target unlike the stuck one."""
    vectors = (
        (1, 0), (1, 1), (0, 1), (-1, 1),
        (-1, 0), (-1, -1), (0, -1), (1, -1),
    )
    attempted_vector = (
        max(-1, min(1, attempted[0] - start[0])),
        max(-1, min(1, attempted[1] - start[1])),
    )
    offset = (seed + owner * 3 + replacement_index * 5) % len(vectors)
    for step in range(len(vectors)):
        vector = vectors[(offset + step) % len(vectors)]
        if vector != attempted_vector:
            return start[0] + vector[0], start[1] + vector[1]
    raise AssertionError("8-way replacement set has no alternate destination")


def canonical_transition_routes(
    center: tuple[int, int], radius: int = 2,
) -> dict[str, list[tuple[int, int]]]:
    """Deterministic turn routes kept inside each clear scenario arena."""
    x, y = center
    return {
        "right-angle": [(x, y), (x + radius, y), (x + radius, y + radius)],
        "u-turn": [(x, y), (x + radius, y), (x, y)],
        "zigzag": [
            (x, y), (x + radius, y + radius),
            (x + radius * 2, y), (x + radius * 3, y + radius),
        ],
        "clockwise-loop": [
            (x, y), (x + radius, y), (x + radius, y + radius),
            (x, y + radius), (x, y),
        ],
        "counter-clockwise-loop": [
            (x, y), (x, y + radius), (x + radius, y + radius),
            (x + radius, y), (x, y),
        ],
        "queued-waypoints": [
            (x, y), (x + radius, y), (x + radius, y + radius),
            (x, y + radius), (x, y),
        ],
        # Unequal 2:1 and 1:2 command vectors straddle the angular regions
        # beside cardinal/diagonal quantization boundaries. Authoritative
        # consecutive tile steps, not these destinations, remain the facing
        # oracle input.
        "quantization-boundary-vectors": [
            (x, y), (x + radius * 2, y + radius), (x, y),
            (x + radius, y + radius * 2), (x, y),
            (x - radius * 2, y + radius), (x, y),
            (x - radius, y - radius * 2), (x, y),
        ],
    }


def catalog_ids_for_entity(entity: dict[str, object]) -> list[str]:
    """Classify observed production entity state into tracked catalog rows."""
    category = str(entity.get("category", ""))
    action = str(entity.get("action", ""))
    detail = str(entity.get("actionDetail", ""))
    animation = str(entity.get("animationState", ""))
    carried = str(entity.get("carriedResource", "")).lower()
    result: set[str] = set()
    if category == "unit-villager":
        if action == "moving" and not carried:
            result.add("villager-empty-moving")
        if carried in {"food", "wood", "gold", "stone"}:
            result.add(f"villager-carrying-{carried}")
        for state in ("gathering", "returning", "constructing", "repairing"):
            if action == state or detail == state:
                result.add(f"villager-{state}")
    if category == "unit-militia":
        result.add("infantry-before-upgrade")
    if category in {
        "unit-man_at_arms", "unit-long_swordsman", "unit-two_handed_swordsman",
        "unit-champion",
    }:
        result.add("infantry-after-upgrade")
    if category in {"unit-archer", "unit-crossbowman", "unit-arbalester"}:
        result.add("archer-ranged-transition")
    if category in {
        "unit-scout_cavalry", "unit-light_cavalry", "unit-knight",
        "unit-cavalier", "unit-paladin",
    }:
        result.add("cavalry")
    if category in {
        "unit-battering_ram", "unit-capped_ram", "unit-siege_ram",
        "unit-mangonel", "unit-onager", "unit-siege_onager",
        "unit-scorpion", "unit-heavy_scorpion",
    }:
        result.add("siege-composite")
    if category in {
        "unit-sheep", "unit-deer", "unit-boar", "unit-turkey",
    }:
        result.add("huntable-herdable-animals")
    for state in ("patrol", "chase", "flee", "formation"):
        if action == state or detail == state:
            result.add(state)
    if int(entity.get("formationGroupId", 0)) != 0:
        result.add("formation")
    if bool(entity.get("patrolling", False)):
        result.add("patrol")
    if bool(entity.get("attackMoving", False)):
        result.add("attack-movement")
    if bool(entity.get("chasing", False)):
        result.add("chase")
    if action in {"attack_moving", "attack-moving"} or detail in {
        "attack_moving", "attack-moving",
    }:
        result.add("attack-movement")
    if animation in {"dying", "decaying", "dead"}:
        result.add("death-decay-direction")
    if category.startswith(("projectile-", "impact-")):
        result.add("projectile-impact-orientation")
    return sorted(result)


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
        # Keep audited sprite well inside unobstructed world viewport. Merely
        # being clickable is insufficient: edge placement can put buildings
        # between target sprite and terrain-only semantic pixel background.
        if 300.0 < logical_x < 750.0 and 160.0 < logical_y < 400.0:
            return
        if logical_y >= 400.0:
            key = Keys.ARROW_DOWN
        elif logical_y <= 160.0:
            key = Keys.ARROW_UP
        elif logical_x <= 300.0:
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
    center: tuple[int, int], seed: int, direction_order: list[int],
    progress: dict[str, object],
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
    progress.update({
        "actor": actor, "owner": owner, "unitId": unit_id,
        "center": {"x": center[0], "y": center[1]},
        "approachFrameCount": 0, "segments": [], "frames": [],
    })
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
    # Direction sampling uses negotiated slow speed so each authoritative
    # step remains drawable. Arena positioning is not evidence for a required
    # direction cell and can exceed the bounded journey at that speed.
    negotiate_game_speed(host, join, 1)
    approach = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=center,
        artifact_dir=artifact_dir, label=f"{actor}-route-approach",
    )
    progress["approachFrameCount"] = len(approach)
    negotiate_game_speed(host, join, 0)
    route = canonical_direction_route(center, direction_order=direction_order)
    segments: list[dict[str, object]] = []
    all_frames: list[dict[str, object]] = []
    progress["segments"] = segments
    progress["frames"] = all_frames
    for lap in range(3):
        for segment_index, destination in enumerate(route[1:]):
            direction = direction_order[segment_index]
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
            current = route[segment_index]
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
            wait_for_drawable_direction(
                host, join, owner=owner, entity_id=unit_id,
                direction=direction, baseline_position=current,
            )
            recapture_attempts: list[dict[str, object]] = []
            pixel_capture: dict[str, object] = {}
            baseline = current
            for capture_attempt in range(3):
                capture_label = (
                    f"{actor}-lap-{lap}-direction-{direction}"
                    + (f"-recapture-{capture_attempt}"
                       if capture_attempt else "")
                )
                pixel_capture = request_correlated_pixel_capture(
                    host, join, artifact_dir, capture_label, unit_id,
                )
                pixel_oracles = []
                for peer in ("host", "join"):
                    capture_metadata = pixel_capture["peers"][peer]
                    manifest_path = artifact_dir / capture_metadata[
                        "manifest"
                    ]
                    retained = evaluate_packaged_capture(
                        manifest_path=manifest_path,
                        graphics_drs=ROOT / "game_data/Data/graphics.drs",
                        interface_drs=ROOT / "game_data/Data/interfac.drs",
                        expected_logical_direction=direction,
                        evidence_directory=(
                            manifest_path.parent / "semantic-direction"
                        ),
                    )
                    pixel_oracles.append({
                        **retained,
                        "manifestPath": str(manifest_path),
                        "oracleKind": "semantic-pixel-direction",
                        "phase": f"{actor}-all-directions",
                        "peer": peer,
                        "owner": owner,
                        "unitKind": "unit-villager",
                        "action": "moving",
                        "seed": seed,
                        "entity": unit_id,
                        "logicalDirection": direction,
                        "expectedLogicalDirection": direction,
                        "actualLogicalDirection": capture_metadata.get(
                            "actualLogicalDirection"
                        ),
                        "authoritativeTick": capture_metadata.get(
                            "authoritativeTick"
                        ),
                        "authoritativeHash": capture_metadata.get(
                            "authoritativeHash"
                        ),
                        "renderFrame": capture_metadata.get("renderFrame"),
                        "previousPosition": capture_metadata.get(
                            "previousPosition"
                        ),
                        "currentPosition": capture_metadata.get(
                            "currentPosition"
                        ),
                        "destinationPosition": capture_metadata.get(
                            "destinationPosition"
                        ),
                        "screenshot": str(
                            manifest_path.parent / retained["images"]["actual"]
                        ),
                        "transitionKind": "authoritative-step",
                        "catalogIds": ["villager-empty-moving"],
                        "assertions": ["pixel-direction"],
                    })
                if not any(
                    oracle["verdict"] == "BLOCKED"
                    for oracle in pixel_oracles
                ):
                    pixel_capture["visualOracles"] = pixel_oracles
                    if actor == "host" and lap == 0 and direction == 0:
                        mutations = {}
                        for peer, oracle in zip(("host", "join"), pixel_oracles):
                            if oracle["verdict"] != "PASS":
                                raise Failure(
                                    "baseline pixel oracle must pass before mutation"
                                )
                            peer_manifest = Path(str(oracle["manifestPath"]))
                            mutations[peer] = write_wrong_direction_mutation(
                                manifest_path=peer_manifest,
                                graphics_drs=ROOT / "game_data/Data/graphics.drs",
                                interface_drs=ROOT / "game_data/Data/interfac.drs",
                                expected_logical_direction=direction,
                                evidence_directory=(
                                    peer_manifest.parent /
                                    "wrong-direction-mutation"
                                ),
                            )
                        pixel_capture["mutationProof"] = {
                            "onePeerWrong": (
                                mutations["host"]["verdict"] == "FAIL" and
                                pixel_oracles[1]["verdict"] == "PASS"
                            ),
                            "bothPeersIdenticallyWrong": all(
                                value["verdict"] == "FAIL"
                                for value in mutations.values()
                            ),
                            "peers": mutations,
                        }
                        position_mutations = {}
                        for peer, oracle in zip(("host", "join"), pixel_oracles):
                            peer_manifest = Path(str(oracle["manifestPath"]))
                            position_mutations[peer] = \
                                write_wrong_position_mutation(
                                    manifest_path=peer_manifest,
                                    graphics_drs=(
                                        ROOT / "game_data/Data/graphics.drs"
                                    ),
                                    interface_drs=(
                                        ROOT / "game_data/Data/interfac.drs"
                                    ),
                                    expected_logical_direction=direction,
                                    evidence_directory=(
                                        peer_manifest.parent /
                                        "wrong-position-mutation"
                                    ),
                                )
                        pixel_capture["mutationProof"]["wrongPosition"] = {
                            "bothPeersFail": all(
                                value["verdict"] == "FAIL"
                                for value in position_mutations.values()
                            ),
                            "peers": position_mutations,
                        }
                    break
                recapture_attempts.append({
                    "capture": pixel_capture,
                    "blockedOracles": pixel_oracles,
                })
                next_position = pixel_capture["peers"][actor].get(
                    "currentPosition"
                )
                if not isinstance(next_position, dict):
                    pixel_capture["visualOracles"] = pixel_oracles
                    break
                baseline = (
                    int(next_position["x"]), int(next_position["y"])
                )
                if baseline == destination or capture_attempt == 2:
                    pixel_capture["visualOracles"] = pixel_oracles
                    break
                wait_for_drawable_direction(
                    host, join, owner=owner, entity_id=unit_id,
                    direction=direction, baseline_position=baseline,
                )
            pixel_capture["recaptureAttempts"] = recapture_attempts
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
                "pixelCapture": pixel_capture,
            })
    progress["renderOracle"] = analyze_render_samples_for_audit(
        all_frames, f"{actor}-all-directions"
    )
    return progress


def exercise_transition_routes(
    journey: Journey, driver, actor: str, owner: int, unit_id: int,
    observer_journey: Journey, observer_driver, observer_actor: str,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    center: tuple[int, int],
) -> dict[str, object]:
    """Capture complete command lifetimes for deterministic turn shapes."""
    routes: dict[str, object] = {}
    for route_name, points in canonical_transition_routes(center).items():
        games = wait_until(
            f"{actor} transition route synchronized state",
            lambda: matching_games(host, join), timeout=WAIT_SECONDS,
        )
        current = owned_unit_positions(games[0], owner).get(unit_id)
        if current != center:
            center_camera_for_tile(
                journey, driver, actions, actor, center[0], center[1]
            )
            audited_world_pointer(
                journey, driver, actions, actor, current[0], current[1],
                button=0,
            )
            audited_world_pointer(
                journey, driver, actions, actor, center[0], center[1]
            )
            capture_until_arrival(
                host, join, owner=owner, unit_id=unit_id,
                destination=center, artifact_dir=artifact_dir,
                label=f"{actor}-{route_name}-reset",
            )
        route_frames: list[dict[str, object]] = []
        step_records: list[dict[str, object]] = []
        if route_name == "queued-waypoints":
            current = points[0]
            center_camera_for_tile(
                journey, driver, actions, actor, *current
            )
            audited_world_pointer(
                journey, driver, actions, actor, *current, button=0,
            )
            wait_until(
                f"{actor} queued route unit {unit_id} selection",
                lambda: unit_id if int(
                    journey.telemetry().get("selectedUnit", 0)
                ) == unit_id else None,
            )
            for step_index, destination in enumerate(points[1:], 1):
                center_camera_for_tile(
                    journey, driver, actions, actor, *destination
                )
                audited_world_pointer(
                    journey, driver, actions, actor, *destination,
                    modifiers=(
                        0 if step_index == 1 else CDP_SHIFT_MODIFIER
                    ),
                )
                step_records.append({
                    "step": step_index,
                    "destination": {
                        "x": destination[0], "y": destination[1]
                    },
                    "queued": True,
                })
            route_frames.extend(capture_until_arrival(
                host, join, owner=owner, unit_id=unit_id,
                destination=points[-1], artifact_dir=artifact_dir,
                label=f"{actor}-{route_name}",
            ))
            for record in step_records:
                record["frameCount"] = len(route_frames)
        else:
            for step_index, destination in enumerate(points[1:], 1):
                current = owned_unit_positions(
                    wait_until(
                        f"{actor} {route_name} step synchronized state",
                        lambda: matching_games(host, join),
                        timeout=WAIT_SECONDS,
                    )[0], owner,
                ).get(unit_id)
                if current is None:
                    raise Failure(
                        f"{actor} {route_name} unit {unit_id} disappeared"
                    )
                center_camera_for_tile(
                    journey, driver, actions, actor, *destination
                )
                center_camera_for_tile(
                    observer_journey, observer_driver, actions, observer_actor,
                    destination[0] + 1, destination[1],
                )
                audited_world_pointer(
                    journey, driver, actions, actor, *current, button=0,
                )
                wait_until(
                    f"{actor} {route_name} unit {unit_id} selection",
                    lambda: unit_id if int(
                        journey.telemetry().get("selectedUnit", 0)
                    ) == unit_id else None,
                )
                audited_world_pointer(
                    journey, driver, actions, actor, *destination
                )
                frames = capture_until_arrival(
                    host, join, owner=owner, unit_id=unit_id,
                    destination=destination, artifact_dir=artifact_dir,
                    label=f"{actor}-{route_name}-step-{step_index}",
                )
                route_frames.extend(frames)
                step_records.append({
                    "step": step_index,
                    "destination": {
                        "x": destination[0], "y": destination[1]
                    },
                    "frameCount": len(frames),
                })
        routes[route_name] = {
            "steps": step_records,
            "frameCount": len(route_frames),
            "renderOracle": analyze_render_samples_for_audit(
                route_frames, f"{actor}-{route_name}"
            ),
            "frames": route_frames,
        }
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "routes": routes,
    }


def exercise_formation_route(
    journey: Journey, driver, actor: str, owner: int,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    center: tuple[int, int],
) -> dict[str, object]:
    """Move two units through normal multi-selection and prove regrouping."""
    games = wait_until(
        f"{actor} formation synchronized state",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    candidates = owned_villager_positions(games[0], owner)
    if len(candidates) < 2:
        raise Failure(f"{actor} formation needs two owned villagers")
    selected = sorted(
        candidates,
        key=lambda unit_id: (
            abs(candidates[unit_id][0] - center[0]) +
            abs(candidates[unit_id][1] - center[1]), unit_id,
        ),
    )[:2]
    for index, unit_id in enumerate(selected):
        position = candidates[unit_id]
        center_camera_for_tile(journey, driver, actions, actor, *position)
        audited_world_pointer(
            journey, driver, actions, actor, *position, button=0,
            modifiers=0 if index == 0 else CDP_SHIFT_MODIFIER,
        )
    wait_until(
        f"{actor} two-unit formation selection",
        lambda: True if int(journey.telemetry().get(
            "selectedUnitCount", 0
        )) == 2 else None,
    )
    destination = (center[0] + 4, center[1] + 4)
    center_camera_for_tile(journey, driver, actions, actor, *destination)
    audited_world_pointer(
        journey, driver, actions, actor, *destination,
    )

    frames: list[dict[str, object]] = []
    formation_observations: list[dict[str, object]] = []
    pixel_capture: dict[str, object] | None = None
    movement_seen = False
    settled_polls = 0
    deadline = time.monotonic() + WAIT_SECONDS
    while time.monotonic() < deadline:
        frames.extend(capture_correlated_frames(
            host, join, seconds=0.15, artifact_dir=artifact_dir,
            label=f"{actor}-formation-regroup", maximum_samples=4,
        ))
        games = matching_games(host, join)
        if games is None:
            continue
        peer_units = []
        for game in games:
            by_id = {
                int(unit["id"]): unit for unit in game.get("units", [])
                if isinstance(unit, dict) and int(unit.get("id", -1)) in selected
            }
            if len(by_id) != 2:
                raise Failure(f"{actor} formation unit disappeared")
            peer_units.append(by_id)
        fields = (
            "x", "y", "moving", "formationGroupId", "formationAnchorX",
            "formationAnchorY", "formationSlotX", "formationSlotY",
            "formationWaypointCount",
        )
        for unit_id in selected:
            if any(field not in peer_units[0][unit_id] for field in fields):
                raise Failure("formation diagnostics missing production fields")
            if any(peer_units[0][unit_id][field] !=
                   peer_units[1][unit_id][field] for field in fields):
                raise Failure(f"formation peer divergence unit {unit_id}")
        units = [peer_units[0][unit_id] for unit_id in selected]
        moving = any(bool(unit["moving"]) for unit in units)
        movement_seen = movement_seen or moving
        group_ids = {int(unit["formationGroupId"]) for unit in units}
        if group_ids != {0}:
            if len(group_ids) != 1:
                raise Failure("formation members use different group IDs")
            anchors = {(int(unit["formationAnchorX"]),
                        int(unit["formationAnchorY"])) for unit in units}
            slots = {(int(unit["formationSlotX"]),
                      int(unit["formationSlotY"])) for unit in units}
            if len(anchors) != 1 or len(slots) != 2:
                raise Failure("formation anchor/slots contradict regrouping")
            formation_observations.append({
                "tick": games[0].get("currentTick"),
                "groupId": next(iter(group_ids)),
                "anchor": list(next(iter(anchors))),
                "slots": [list(slot) for slot in sorted(slots)],
            })
            if moving and pixel_capture is None:
                pixel_capture = capture_catalog_semantic_pixels(
                    host, join, artifact_dir,
                    f"{actor}-formation-semantic", selected[0],
                    owner=owner, unit_kind="unit-villager",
                    action="formation", catalog_ids=[
                        "formation", "villager-empty-moving",
                    ],
                    phase=f"{actor}-formation-regroup",
                )
        if movement_seen and not moving:
            settled_polls += 1
            if settled_polls >= 2:
                break
        else:
            settled_polls = 0
    if not movement_seen:
        raise Failure(f"{actor} formation never moved")
    if not formation_observations:
        raise Failure(f"{actor} formation group was never authoritative")
    if pixel_capture is None:
        raise Failure(f"{actor} formation lacked drawable pixel proof")
    if settled_polls < 2:
        raise Failure(f"{actor} formation did not regroup and settle")
    return {
        "actor": actor, "owner": owner, "unitIds": selected,
        "destination": {"x": destination[0], "y": destination[1]},
        "formationObservations": formation_observations,
        "pixelCapture": pixel_capture,
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-formation-regroup"
        ),
    }


def exercise_catalog_movement(
    journey: Journey, driver, actor: str, owner: int,
    observer_journey: Journey, observer_driver, observer_actor: str,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    start: tuple[int, int], destination: tuple[int, int],
    unit_kind: str, catalog_ids: list[str], command_key: str | None = None,
) -> dict[str, object]:
    """Move exact packaged fixture and retain class-specific pixel evidence."""
    catalog_id = catalog_ids[0]
    games = wait_until(
        f"{actor} {catalog_id} synchronized fixture",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    candidates = [
        int(unit["id"])
        for unit in games[0].get("units", [])
        if isinstance(unit, dict) and int(unit.get("owner", -1)) == owner and
        (int(unit.get("x", -1)), int(unit.get("y", -1))) == start
    ]
    if len(candidates) != 1:
        raise Failure(
            f"{actor} {catalog_id} fixture mismatch at {start}: {candidates}"
        )
    unit_id = candidates[0]
    center_camera_for_tile(journey, driver, actions, actor, *start)
    center_camera_for_tile(
        observer_journey, observer_driver, actions, observer_actor, *start
    )
    audited_world_pointer(
        journey, driver, actions, actor, *start, button=0,
    )
    wait_until(
        f"{actor} {catalog_id} fixture selection",
        lambda: unit_id if int(journey.telemetry().get(
            "selectedUnit", 0
        )) == unit_id else None,
    )
    if command_key is not None:
        audited_key(driver, actions, actor, command_key)
    audited_world_pointer(
        journey, driver, actions, actor, *destination,
    )
    pixel_capture: dict[str, object] | None = None
    frames: list[dict[str, object]] = []
    deadline = time.monotonic() + WAIT_SECONDS
    while time.monotonic() < deadline:
        frames.extend(capture_correlated_frames(
            host, join, seconds=0.15, artifact_dir=artifact_dir,
            label=f"{actor}-{catalog_id}", maximum_samples=4,
        ))
        games = matching_games(host, join)
        if games is None:
            continue
        positions = [owned_unit_positions(game, owner) for game in games]
        if positions[0] != positions[1]:
            raise Failure(f"{actor} {catalog_id} peer position divergence")
        if positions[0].get(unit_id) == destination:
            break
        units = [
            next((unit for unit in game.get("units", [])
                  if int(unit.get("id", -1)) == unit_id), None)
            for game in games
        ]
        if not all(isinstance(unit, dict) for unit in units):
            raise Failure(f"{actor} {catalog_id} fixture disappeared")
        if (pixel_capture is None and
                all(bool(unit.get("moving")) for unit in units)):
            pixel_capture = capture_catalog_semantic_pixels(
                host, join, artifact_dir, f"{actor}-{catalog_id}-semantic",
                unit_id, owner=owner, unit_kind=unit_kind, action="moving",
                catalog_ids=catalog_ids, phase=f"{actor}-{catalog_id}",
            )
    else:
        raise Failure(f"{actor} {catalog_id} fixture did not arrive")
    if pixel_capture is None:
        raise Failure(f"{actor} {catalog_id} lacked drawable pixel proof")
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "unitKind": unit_kind, "catalogId": catalog_id,
        "catalogIds": catalog_ids,
        "start": {"x": start[0], "y": start[1]},
        "destination": {"x": destination[0], "y": destination[1]},
        "pixelCapture": pixel_capture,
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-{catalog_id}"
        ),
    }


def exercise_patrol_route(
    journey: Journey, driver, actor: str, owner: int,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    start_hint: tuple[int, int], destination: tuple[int, int],
) -> dict[str, object]:
    """Prove outbound/return patrol and final stop through visible controls."""
    games = wait_until(
        f"{actor} patrol synchronized state",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    candidates = owned_unit_positions(games[0], owner)
    unit_id, start = min(
        candidates.items(), key=lambda item: (
            abs(item[1][0] - start_hint[0]) +
            abs(item[1][1] - start_hint[1]), item[0],
        )
    )
    if start != start_hint:
        raise Failure(
            f"{actor} packaged patrol unit missing at {start_hint}: {start}"
        )
    center_camera_for_tile(journey, driver, actions, actor, *start)
    audited_world_pointer(
        journey, driver, actions, actor, *start, button=0
    )
    wait_until(
        f"{actor} patrol unit selection",
        lambda: unit_id if int(journey.telemetry().get(
            "selectedUnit", 0
        )) == unit_id else None,
    )
    audited_key(driver, actions, actor, "p")
    center_camera_for_tile(journey, driver, actions, actor, *destination)
    audited_world_pointer(journey, driver, actions, actor, *destination)

    frames: list[dict[str, object]] = []
    authoritative_positions: list[tuple[int, int]] = []
    patrolling_seen = False
    returned = False
    pixel_capture: dict[str, object] | None = None
    deadline = time.monotonic() + WAIT_SECONDS
    while time.monotonic() < deadline:
        frames.extend(capture_correlated_frames(
            host, join, seconds=0.15, artifact_dir=artifact_dir,
            label=f"{actor}-patrol", maximum_samples=4,
        ))
        games = matching_games(host, join)
        if games is None:
            continue
        peer_units = []
        for game in games:
            unit = next((value for value in game.get("units", [])
                         if int(value.get("id", -1)) == unit_id), None)
            if not isinstance(unit, dict):
                raise Failure(f"{actor} patrol unit disappeared")
            peer_units.append(unit)
        fields = ("x", "y", "moving", "patrolling")
        if any(peer_units[0].get(field) != peer_units[1].get(field)
               for field in fields):
            raise Failure(f"{actor} patrol peer divergence")
        unit = peer_units[0]
        if "patrolling" not in unit:
            raise Failure("patrol diagnostics missing production field")
        patrolling_seen = patrolling_seen or bool(unit["patrolling"])
        if (bool(unit["patrolling"]) and bool(unit["moving"]) and
                pixel_capture is None):
            pixel_capture = capture_catalog_semantic_pixels(
                host, join, artifact_dir, f"{actor}-patrol-semantic",
                unit_id, owner=owner, unit_kind=str(
                unit.get("category", "unit-unknown")
                ), action="patrol", catalog_ids=[
                    "patrol", "infantry-before-upgrade",
                ],
                phase=f"{actor}-patrol",
            )
        position = (int(unit["x"]), int(unit["y"]))
        if not authoritative_positions or position != authoritative_positions[-1]:
            authoritative_positions.append(position)
        reached_destination = any(
            abs(value[0] - destination[0]) <= 1 and
            abs(value[1] - destination[1]) <= 1
            for value in authoritative_positions
        )
        if reached_destination and len(authoritative_positions) > 2:
            returned = (
                abs(position[0] - start[0]) <
                abs(destination[0] - start[0]) or
                abs(position[1] - start[1]) <
                abs(destination[1] - start[1])
            )
        if patrolling_seen and reached_destination and returned:
            break
    if not patrolling_seen:
        raise Failure(f"{actor} patrol state never became authoritative")
    if not returned:
        raise Failure(f"{actor} patrol never began return leg")
    if pixel_capture is None:
        raise Failure(f"{actor} patrol lacked drawable pixel proof")
    audited_key(driver, actions, actor, "s")
    wait_until(
        f"{actor} patrol stop",
        lambda: True if (
            (unit := next((value for value in (
                game_diagnostics(driver) or {}
            ).get("units", []) if int(value.get("id", -1)) == unit_id), None))
            and not bool(unit.get("moving")) and
            not bool(unit.get("patrolling"))
        ) else None,
    )
    frames.extend(capture_correlated_frames(
        host, join, seconds=0.5, artifact_dir=artifact_dir,
        label=f"{actor}-patrol-stop", maximum_samples=8,
    ))
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "start": {"x": start[0], "y": start[1]},
        "destination": {"x": destination[0], "y": destination[1]},
        "authoritativePositions": [
            {"x": value[0], "y": value[1]}
            for value in authoritative_positions
        ],
        "patrollingSeen": patrolling_seen, "returnLegSeen": returned,
        "pixelCapture": pixel_capture,
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-patrol"
        ),
    }


def exercise_obstacle_detour_route(
    journey: Journey, driver, actor: str, owner: int, unit_id: int,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
    start: tuple[int, int], destination: tuple[int, int],
    obstacle: tuple[int, int],
) -> dict[str, object]:
    """Command straight through a known House and prove path detours it."""
    games = wait_until(
        f"{actor} obstacle synchronized state",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    current = owned_unit_positions(games[0], owner).get(unit_id)
    if current is None:
        raise Failure(f"{actor} obstacle route unit missing")
    center_camera_for_tile(journey, driver, actions, actor, *current)
    audited_world_pointer(
        journey, driver, actions, actor, *current, button=0
    )
    center_camera_for_tile(journey, driver, actions, actor, *start)
    audited_world_pointer(journey, driver, actions, actor, *start)
    approach = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=start,
        artifact_dir=artifact_dir, label=f"{actor}-obstacle-approach",
    )
    center_camera_for_tile(journey, driver, actions, actor, *destination)
    audited_world_pointer(journey, driver, actions, actor, *destination)
    frames = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=destination,
        artifact_dir=artifact_dir, label=f"{actor}-obstacle-detour",
    )
    positions: list[tuple[int, int]] = []
    for sample in frames:
        game = sample.get("authoritativeHost") or {}
        value = owned_unit_positions(game, owner).get(unit_id)
        if value is not None and (not positions or positions[-1] != value):
            positions.append(value)
    if not positions:
        raise Failure(f"{actor} obstacle detour lacks authoritative positions")
    if not any(position[1] != start[1] for position in positions):
        raise Failure(
            f"{actor} path crossed House axis without observable detour"
        )
    if any(position == obstacle for position in positions):
        raise Failure(f"{actor} path entered blocked House tile {obstacle}")
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "routeKind": ["obstacle-detour", "building-corner-navigation"],
        "start": {"x": start[0], "y": start[1]},
        "destination": {"x": destination[0], "y": destination[1]},
        "obstacle": {"kind": "house", "x": obstacle[0], "y": obstacle[1]},
        "approachFrameCount": len(approach),
        "authoritativePositions": [
            {"x": value[0], "y": value[1]} for value in positions
        ],
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-obstacle-detour"
        ),
    }


def exercise_narrow_passage_route(
    journey: Journey, driver, actor: str, owner: int, unit_id: int,
    host, join, actions: list[dict[str, object]], artifact_dir: Path,
) -> dict[str, object]:
    """Traverse tracked two-House lane and prove unit stays in corridor."""
    start = (24, 1)
    destination = (24, 8)
    games = wait_until(
        f"{actor} narrow passage synchronized state",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    current = owned_unit_positions(games[0], owner).get(unit_id)
    if current is None:
        raise Failure(f"{actor} narrow-passage unit missing")
    center_camera_for_tile(journey, driver, actions, actor, *current)
    audited_world_pointer(journey, driver, actions, actor, *current, button=0)
    center_camera_for_tile(journey, driver, actions, actor, *start)
    audited_world_pointer(journey, driver, actions, actor, *start)
    approach = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=start,
        artifact_dir=artifact_dir, label=f"{actor}-narrow-approach",
    )
    center_camera_for_tile(journey, driver, actions, actor, *destination)
    audited_world_pointer(journey, driver, actions, actor, *destination)
    frames = capture_until_arrival(
        host, join, owner=owner, unit_id=unit_id, destination=destination,
        artifact_dir=artifact_dir, label=f"{actor}-narrow-passage",
    )
    positions: list[tuple[int, int]] = []
    for sample in frames:
        value = owned_unit_positions(
            sample.get("authoritativeHost") or {}, owner
        ).get(unit_id)
        if value is not None and (not positions or positions[-1] != value):
            positions.append(value)
    corridor = [position for position in positions if 3 <= position[1] <= 6]
    if not corridor:
        raise Failure(f"{actor} narrow passage has no corridor samples")
    if any(position[0] not in {23, 24} for position in corridor):
        raise Failure(
            f"{actor} bypassed narrow passage: {corridor}"
        )
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "routeKind": "narrow-passage",
        "fixtureBuildings": [
            {"owner": 0, "x": 21, "y": 4},
            {"owner": 1, "x": 26, "y": 4},
        ],
        "start": {"x": start[0], "y": start[1]},
        "destination": {"x": destination[0], "y": destination[1]},
        "corridorPositions": [
            {"x": value[0], "y": value[1]} for value in corridor
        ],
        "approachFrameCount": len(approach),
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-narrow-passage"
        ),
    }


def exercise_moving_target_chase(
    journey: Journey, driver, actor: str, owner: int,
    target_journey: Journey, target_driver, target_actor: str,
    target_owner: int, host, join, actions: list[dict[str, object]],
    artifact_dir: Path, attacker_hint: tuple[int, int],
    target_hint: tuple[int, int], target_destination: tuple[int, int],
) -> dict[str, object]:
    """Attack a separately commanded moving target and prove live chase."""
    games = wait_until(
        f"{actor} chase synchronized state",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    positions = {
        owner: owned_unit_positions(games[0], owner),
        target_owner: owned_unit_positions(games[0], target_owner),
    }
    attacker_id, attacker_position = min(
        positions[owner].items(), key=lambda item: (
            abs(item[1][0] - attacker_hint[0]) +
            abs(item[1][1] - attacker_hint[1]), item[0],
        )
    )
    target_id, target_position = min(
        positions[target_owner].items(), key=lambda item: (
            abs(item[1][0] - target_hint[0]) +
            abs(item[1][1] - target_hint[1]), item[0],
        )
    )
    if attacker_position != attacker_hint or target_position != target_hint:
        raise Failure(
            f"chase fixture mismatch attacker={attacker_position} "
            f"target={target_position}"
        )
    center_camera_for_tile(
        target_journey, target_driver, actions, target_actor, *target_position
    )
    audited_world_pointer(
        target_journey, target_driver, actions, target_actor,
        *target_position, button=0,
    )
    center_camera_for_tile(journey, driver, actions, actor, *attacker_position)
    audited_world_pointer(
        journey, driver, actions, actor, *attacker_position, button=0
    )
    center_camera_for_tile(journey, driver, actions, actor, *target_position)
    audited_world_pointer(journey, driver, actions, actor, *target_position)
    center_camera_for_tile(
        target_journey, target_driver, actions, target_actor,
        *target_destination,
    )
    audited_world_pointer(
        target_journey, target_driver, actions, target_actor,
        *target_destination,
    )

    frames: list[dict[str, object]] = []
    target_positions: set[tuple[int, int]] = set()
    attacker_destinations: set[tuple[int, int]] = set()
    chasing_ticks: list[int] = []
    pixel_capture: dict[str, object] | None = None
    deadline = time.monotonic() + WAIT_SECONDS
    while time.monotonic() < deadline:
        frames.extend(capture_correlated_frames(
            host, join, seconds=0.15, artifact_dir=artifact_dir,
            label=f"{actor}-moving-target-chase", maximum_samples=4,
        ))
        games = matching_games(host, join)
        if games is None:
            continue
        peer_units = []
        for game in games:
            by_id = {int(unit["id"]): unit for unit in game.get("units", [])
                     if int(unit.get("id", -1)) in {attacker_id, target_id}}
            if len(by_id) != 2:
                raise Failure("moving-target chase unit disappeared")
            peer_units.append(by_id)
        fields = (
            "x", "y", "destinationX", "destinationY", "moving",
            "attackTargetId",
        )
        for unit_id in (attacker_id, target_id):
            if any(peer_units[0][unit_id].get(field) !=
                   peer_units[1][unit_id].get(field) for field in fields):
                raise Failure(f"moving-target chase peer divergence {unit_id}")
        attacker = peer_units[0][attacker_id]
        target = peer_units[0][target_id]
        target_positions.add((int(target["x"]), int(target["y"])))
        if int(attacker.get("attackTargetId", 0)) == target_id:
            chasing_ticks.append(int(games[0].get("currentTick", -1)))
            attacker_destinations.add((
                int(attacker["destinationX"]),
                int(attacker["destinationY"]),
            ))
            if bool(attacker.get("moving")) and pixel_capture is None:
                pixel_capture = capture_catalog_semantic_pixels(
                    host, join, artifact_dir,
                    f"{actor}-moving-target-chase-semantic", attacker_id,
                    owner=owner, unit_kind=str(
                        attacker.get("category", "unit-unknown")
                    ), action="chase", catalog_ids=["chase", "cavalry"],
                    phase=f"{actor}-moving-target-chase",
                )
        if (len(target_positions) >= 3 and
                len(attacker_destinations) >= 2 and len(chasing_ticks) >= 3):
            break
    if len(target_positions) < 3:
        raise Failure(f"{actor} chase target did not move")
    if len(attacker_destinations) < 2:
        raise Failure(f"{actor} attacker destination did not follow target")
    if len(chasing_ticks) < 3:
        raise Failure(f"{actor} chase target binding was not retained")
    if pixel_capture is None:
        raise Failure(f"{actor} chase lacked drawable pixel proof")
    audited_key(driver, actions, actor, "s")
    audited_key(target_driver, actions, target_actor, "s")
    frames.extend(capture_correlated_frames(
        host, join, seconds=0.5, artifact_dir=artifact_dir,
        label=f"{actor}-moving-target-chase-stop", maximum_samples=8,
    ))
    return {
        "actor": actor, "owner": owner, "attackerId": attacker_id,
        "targetActor": target_actor, "targetOwner": target_owner,
        "targetId": target_id, "routeKind": "moving-target-chase",
        "targetPositionCount": len(target_positions),
        "attackerDestinationCount": len(attacker_destinations),
        "chasingTicks": chasing_ticks,
        "pixelCapture": pixel_capture,
        "frameCount": len(frames), "frames": frames,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-moving-target-chase"
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
                # Render diagnostics retain authoritative visible-world
                # candidates even when their projected rectangle is wholly
                # outside viewport. No draw submission exists in that case;
                # absence of sprite provenance is expected, not fallback.
                if (not entity.get("layers") and
                        not isinstance(entity.get("renderPosition"), dict)):
                    counts["offscreenEntities"] = (
                        int(counts.get("offscreenEntities", 0)) + 1
                    )
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
                    # Coverage metadata names canonical body alternatives.
                    # Buildings may submit legitimate damage/ambient overlay
                    # layers after that body. Prove the primary production
                    # draw uses a canonical body; validate every extra layer
                    # through its own draw telemetry and peer comparison below.
                    primary_resource = entity.get("layers", [])[0].get(
                        "resourceId"
                    )
                    if (expected_resources and
                            primary_resource not in expected_resources):
                        raise Failure(
                            f"rendered asset violates mapping: {entity}"
                        )
                    for layer_index, layer in enumerate(entity.get("layers", [])):
                        entity_key = (
                            peer, str(entity.get("category", "")),
                            int(entity.get("id", -1)), layer_index,
                        )
                        if int(state.get("schemaVersion", 0)) >= 1:
                            source_rectangle = layer.get("sourceRectangle")
                            destination = layer.get("destination")
                            clipped = layer.get("clippedDestination")
                            if (not isinstance(layer.get("drawOrder"), int) or
                                    not all(isinstance(value, dict) for value in (
                                        source_rectangle, destination, clipped
                                    ))):
                                raise Failure(
                                    f"draw submission telemetry incomplete "
                                    f"{entity_key}"
                                )
                            if (float(source_rectangle.get("w", -1)) !=
                                    float(layer.get("width", -2)) or
                                    float(source_rectangle.get("h", -1)) !=
                                    float(layer.get("height", -2)) or
                                    float(clipped.get("w", -1)) < 0 or
                                    float(clipped.get("h", -1)) < 0 or
                                    float(clipped.get("w", 0)) >
                                    float(destination.get("w", -1)) or
                                    float(clipped.get("h", 0)) >
                                    float(destination.get("h", -1))):
                                raise Failure(
                                    f"draw submission telemetry contradicts "
                                    f"actual layer {entity_key}"
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
                            oracle_previous = (
                                int(previous["x"]), int(previous["y"])
                            )
                            oracle_current = (
                                int(current["x"]), int(current["y"])
                            )
                            if oracle_previous == oracle_current:
                                target = (
                                    entity.get("resourceTarget")
                                    if entity.get("action") == "gathering"
                                    else entity.get("workTarget")
                                    if entity.get("action") in {
                                        "constructing", "repairing"
                                    } else None
                                )
                                if isinstance(target, dict):
                                    oracle_current = (
                                        int(target["x"]), int(target["y"])
                                    )
                                if oracle_previous == oracle_current:
                                    continue
                            try:
                                oracle = evaluate_layer(
                                    previous=oracle_previous,
                                    current=oracle_current,
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
                                "oracleKind": "frame-selection",
                                "assertions": [
                                    "movement-direction", "resolved-frame",
                                    "mirror",
                                ],
                                "catalogIds": catalog_ids_for_entity(entity),
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
                             int(entity.get("owner", -1)),
                             int(entity.get("facing", -1)),
                             str(entity.get("animationState", "")),
                             str(entity.get("action", "")),
                             tuple(catalog_ids_for_entity(entity))), []
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
                selection = entity.get("selectionOverlay")
                if selection is not None:
                    if not isinstance(selection, dict) or not isinstance(
                        position, dict
                    ):
                        raise Failure(
                            f"selection overlay lacks render position: {entity}"
                        )
                    center = selection.get("center")
                    color = selection.get("color")
                    layer_orders = [
                        int(layer.get("drawOrder", -1))
                        for layer in entity.get("layers", [])
                    ]
                    if (not isinstance(center, dict) or
                            not isinstance(color, dict) or
                            abs(float(center.get("x", -1)) -
                                float(position["x"])) > 0.01 or
                            abs(float(center.get("y", -1)) -
                                (float(position["y"]) + 1.0)) > 0.01 or
                            float(selection.get("halfWidth", 0)) <= 0 or
                            float(selection.get("halfHeight", 0)) <= 0 or
                            tuple(int(color.get(channel, -1))
                                  for channel in ("r", "g", "b", "a")) !=
                            (250, 220, 65, 255) or
                            int(selection.get("shadowDrawOrder", -1)) <=
                            max(layer_orders, default=-1) or
                            int(selection.get("markerDrawOrder", -1)) <=
                            int(selection.get("shadowDrawOrder", -1))):
                        raise Failure(
                            f"selection overlay draw mismatch: {entity}"
                        )
                    counts["selectionOverlayAssertions"] = (
                        int(counts.get("selectionOverlayAssertions", 0)) + 1
                    )
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
                          "animationState", "previousPosition",
                          "simulationPosition", "hasResourceTarget",
                          "returningResource", "resourceTarget",
                          "resourceTargetInMap", "resourceTargetKind",
                          "resourceTargetExists", "resourceTargetAmount",
                          "resourceTargetEntityId", "resourceBuildingId",
                          "resourceUnitId", "workTargetId", "workTarget",
                          "carriedResource",
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
        action_active = key[7] not in {"", "idle", "standing", "none"}
        moving_ticks = {
            tick for tick, _, moving, _, _ in observations
            if moving or action_active
        }
        moving_frames = {
            frame for _, frame, moving, _, _ in observations
            if moving or action_active
        }
        if len(moving_ticks) >= 4 and len(moving_frames) < 2:
            raise Failure(f"frozen moving animation {key}")
        animation_verdict = (
            "PASS" if len(moving_ticks) >= 4 and len(moving_frames) >= 2
            else "SKIPPED"
        )
        counts["visualOracles"].append({
            "schemaVersion": 1,
            "oracleKind": "animation-progress",
            "verdict": animation_verdict,
            "blocker": None if animation_verdict == "PASS" else
                "fewer than four moving ticks or two physical frames",
            "peer": key[0], "unitKind": key[1], "entity": key[2],
            "layer": key[3], "owner": key[4],
            "logicalDirection": key[5],
            "animationState": key[6], "action": key[7],
            "sampleCount": len(observations),
            "distinctMovingTickCount": len(moving_ticks),
            "distinctMovingFrameCount": len(moving_frames),
            "catalogIds": list(key[8]),
            "assertions": ["animation-progress"]
                if animation_verdict == "PASS" else [],
        })
    counts["maximumFrameDisplacement"] = maximum_displacement
    counts["unmatchedEntities"] = unmatched_entities
    transition_oracle = evaluate_transitions(samples)
    counts["transitionOracle"] = transition_oracle
    counts["visualOracles"].append(transition_oracle)
    if transition_oracle["verdict"] != "PASS":
        raise Failure(
            "temporal direction transition mismatch "
            + json.dumps(transition_oracle["failures"][0], sort_keys=True)
        )
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
            if value.get("verdict") == "FAIL":
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


def relay_blocker_from_diagnostics(
    host_state: object, join_state: object,
) -> dict[str, object] | None:
    """Classify evidenced non-active production reliability as infrastructure."""
    states = {"host": host_state, "join": join_state}
    peers: dict[str, dict[str, int]] = {}
    rejected_publications: dict[str, list[dict[str, object]]] = {}
    for peer, state in states.items():
        if not isinstance(state, dict):
            return None
        game = state.get("game")
        if not isinstance(game, dict):
            return None
        status = int(game.get("reliabilityStatus", -1))
        reason = int(game.get("reliabilityReason", -1))
        peers[peer] = {"status": status, "reason": reason}
        for publication in state.get("recentPublications", []):
            if not isinstance(publication, dict):
                continue
            results = publication.get("results", [])
            if not isinstance(results, list) or not results:
                continue
            accepted = sum(
                1 for result in results
                if isinstance(result, dict) and bool(result.get("ok"))
            )
            if accepted < 2:
                rejected_publications.setdefault(peer, []).append({
                    "intentId": publication.get("intentId"),
                    "acceptedRelayCount": accepted,
                    "results": results,
                })
    if (all(value["status"] == 0 for value in peers.values()) and
            not rejected_publications):
        return None
    return {
        "classification": "public-relay-infrastructure",
        "policy": "production-reliability-or-publish-quorum-v1",
        "peers": peers,
        "rejectedPublications": rejected_publications,
    }


def negotiate_game_speed(host, join, target: int) -> None:
    """Cycle normal F8 multiplayer control to an exact shared speed."""
    if target not in {0, 1, 2}:
        raise ValueError("game speed target must be 0, 1, or 2")
    for _ in range(3):
        speeds = [
            int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
            for driver in (host, join)
        ]
        if speeds == [target, target]:
            return
        if speeds[0] != speeds[1]:
            raise Failure(f"game speed peer divergence: {speeds}")
        key_chord(host, Keys.F8)
        wait_until(
            f"negotiated game speed {target}",
            lambda: True if all(
                int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
                != speeds[0] for driver in (host, join)
            ) else None,
            timeout=WAIT_SECONDS,
        )
    speeds = [
        int((game_diagnostics(driver) or {}).get("gameSpeed", -1))
        for driver in (host, join)
    ]
    raise Failure(f"game speed did not reach {target}: {speeds}")


def banked_resource_increased(
    telemetry: dict[str, object], resource: str, initial: int,
) -> int | None:
    value = int(telemetry["resources"][resource])
    return value if value > initial else None


def exercise_resource_lifetime(
    journey: Journey, driver, actor: str, owner: int,
    observer_journey: Journey, observer_driver, observer_actor: str,
    host, join, actions: list[dict[str, object]], artifact_dir: Path | None,
    resource: str, resource_tile: tuple[int, int], gatherer_id: int | None,
) -> dict[str, object]:
    """Capture one real gather, cargo-return, and deposit lifetime."""
    initial = int(journey.telemetry()["resources"][resource])
    center_camera_for_tile(
        journey, driver, actions, actor, *resource_tile
    )
    center_camera_for_tile(
        observer_journey, observer_driver, actions, observer_actor,
        *resource_tile,
    )
    if gatherer_id is None:
        route_unit_tile = (20, 12) if owner == 0 else (28, 12)
        audited_world_pointer(
            journey, driver, actions, actor, *route_unit_tile, button=0,
        )
        wait_until(
            f"{actor} route villager selection for {resource}",
            lambda: int(journey.telemetry().get("selectedUnit", 0)) or None,
        )
        gatherer_id = int(journey.telemetry()["selectedUnit"])
    else:
        games = wait_until(
            f"{actor} {resource} gatherer lockstep",
            lambda: matching_games(host, join), timeout=WAIT_SECONDS,
        )
        gatherer = next(
            (unit for unit in games[0].get("units", [])
             if int(unit.get("id", -1)) == gatherer_id), None
        )
        if not isinstance(gatherer, dict):
            raise Failure(f"{actor} {resource} gatherer disappeared")
        audited_world_pointer(
            journey, driver, actions, actor,
            int(gatherer["x"]), int(gatherer["y"]), button=0,
        )
        wait_until(
            f"{actor} villager selection for {resource}",
            lambda: gatherer_id if int(journey.telemetry().get(
                "selectedUnit", 0
            )) == gatherer_id else None,
        )
    audited_world_pointer(
        journey, driver, actions, actor, *resource_tile,
    )
    frames: list[dict[str, object]] = []
    gather_pixel: dict[str, object] | None = None
    return_pixel: dict[str, object] | None = None
    gathered = None
    deadline = time.monotonic() + WAIT_SECONDS * 3
    while time.monotonic() < deadline:
        frames.extend(capture_correlated_frames(
            host, join, seconds=0.2, artifact_dir=artifact_dir,
            label=f"{actor}-gather-{resource}", maximum_samples=4,
        ))
        render_states = [render_diagnostics(value) or {}
                         for value in (host, join)]
        entities = [
            next((entity for entity in state.get("entities", [])
                  if isinstance(entity, dict) and
                  int(entity.get("id", -1)) == gatherer_id), None)
            for state in render_states
        ]
        if all(isinstance(entity, dict) for entity in entities):
            if entities[0].get("action") != entities[1].get("action"):
                raise Failure(f"{actor} {resource} render action diverged")
            if (gather_pixel is None and
                    all(entity.get("action") == "gathering"
                        for entity in entities)):
                positions = []
                for entity in entities:
                    current = entity.get("simulationPosition")
                    target = entity.get("resourceTarget")
                    if not isinstance(current, dict) or not isinstance(
                        target, dict
                    ):
                        positions = []
                        break
                    positions.append((
                        (int(current["x"]), int(current["y"])),
                        (int(target["x"]), int(target["y"])),
                    ))
                if len(positions) == 2 and positions[0] == positions[1]:
                    gather_pixel = capture_catalog_semantic_pixels(
                        host, join, artifact_dir,
                        f"{actor}-gather-{resource}-semantic", gatherer_id,
                        owner=owner, unit_kind="unit-villager",
                        action="gathering",
                        catalog_ids=["villager-gathering"],
                        phase=f"{actor}-gather-{resource}",
                        direction_positions=positions[0],
                    )
            if (return_pixel is None and all(
                bool(entity.get("returningResource")) and
                bool(entity.get("moving")) and
                str(entity.get("carriedResource", "")).lower() == resource
                for entity in entities
            )):
                return_pixel = capture_catalog_semantic_pixels(
                    host, join, artifact_dir,
                    f"{actor}-return-{resource}-semantic", gatherer_id,
                    owner=owner, unit_kind="unit-villager",
                    action="returning", catalog_ids=[
                        "villager-returning", f"villager-carrying-{resource}",
                    ], phase=f"{actor}-return-{resource}",
                )
        gathered = banked_resource_increased(
            journey.telemetry(), resource, initial
        )
        if gathered is not None:
            break
    if gathered is None:
        raise Failure(f"{actor} did not gather and bank {resource}")
    if gather_pixel is None or return_pixel is None:
        raise Failure(
            f"{actor} {resource} lifetime lacked gather/return pixel proof"
        )
    return {
        "resource": resource,
        "initial": initial,
        "banked": int(gathered),
        "gathererId": gatherer_id,
        "frames": frames,
        "gatherPixelCapture": gather_pixel,
        "returnPixelCapture": return_pixel,
        "renderOracle": analyze_render_samples_for_audit(
            frames, f"{actor}-gather-{resource}"
        ),
    }


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
    observer_journey: Journey,
    observer_driver,
    observer_actor: str,
    host,
    join,
    actions: list[dict[str, object]],
    artifact_dir: Path | None = None,
) -> dict[str, object]:
    resource_tiles = ({
        "gold": (7, 10), "food": (2, 29),
        "wood": (5, 29), "stone": (10, 29),
    } if owner == 0 else {
        "gold": (40, 10), "food": (45, 2),
        "wood": (42, 2), "stone": (37, 2),
    })
    resource_lifetimes: dict[str, dict[str, object]] = {}
    gatherer_id: int | None = None
    for resource in ("gold", "food", "wood", "stone"):
        lifetime = exercise_resource_lifetime(
            journey, driver, actor, owner,
            observer_journey, observer_driver, observer_actor,
            host, join, actions, artifact_dir,
            resource, resource_tiles[resource], gatherer_id,
        )
        resource_lifetimes[resource] = lifetime
        gatherer_id = int(lifetime["gathererId"])

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
        center_camera_for_tile(
            observer_journey, observer_driver, actions, observer_actor,
            tile_x, tile_y,
        )
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
    construction_frames: list[dict[str, object]] = []
    construction_pixel: dict[str, object] | None = None
    deadline = time.monotonic() + WAIT_SECONDS * 3
    while time.monotonic() < deadline:
        construction_frames.extend(capture_correlated_frames(
            host, join, seconds=0.2, artifact_dir=artifact_dir,
            label=f"{actor}-construct-house", maximum_samples=4,
        ))
        render_states = [render_diagnostics(value) or {}
                         for value in (host, join)]
        entities = [
            next((entity for entity in state.get("entities", [])
                  if isinstance(entity, dict) and
                  int(entity.get("id", -1)) == selected_villager), None)
            for state in render_states
        ]
        if (construction_pixel is None and
                all(isinstance(entity, dict) and
                    entity.get("action") == "constructing"
                    for entity in entities)):
            positions = []
            for entity in entities:
                current = entity.get("simulationPosition")
                target = entity.get("workTarget")
                if not isinstance(current, dict) or not isinstance(
                    target, dict
                ):
                    positions = []
                    break
                positions.append((
                    (int(current["x"]), int(current["y"])),
                    (int(target["x"]), int(target["y"])),
                ))
            if len(positions) == 2 and positions[0] == positions[1]:
                construction_pixel = capture_catalog_semantic_pixels(
                    host, join, artifact_dir,
                    f"{actor}-construct-house-semantic",
                    int(selected_villager), owner=owner,
                    unit_kind="unit-villager", action="constructing",
                    catalog_ids=["villager-constructing"],
                    phase=f"{actor}-construct-house",
                    direction_positions=positions[0],
                )
        games = matching_games(host, join)
        if games is not None:
            constructed = [
                building for building in owner_buildings(games[0], owner)
                if int(building.get("id", -1)) in constructed_ids
            ]
            if constructed and all(
                int(building.get("constructionTicksRemaining", -1)) == 0
                for building in constructed
            ):
                break
    else:
        raise Failure(f"{actor} house did not complete")
    if construction_pixel is None:
        raise Failure(f"{actor} construction lacked drawable pixel proof")
    construction_oracle = analyze_render_samples_for_audit(
        construction_frames, f"{actor}-construct-house"
    )

    repair_tile = (5, 19) if owner == 0 else (42, 10)
    games = wait_until(
        f"{actor} repair fixture lockstep", lambda: matching_games(host, join),
        timeout=WAIT_SECONDS,
    )
    villager = next(
        (unit for unit in games[0].get("units", [])
         if int(unit.get("id", -1)) == selected_villager), None
    )
    if not isinstance(villager, dict):
        raise Failure(f"{actor} construction villager disappeared")
    center_camera_for_tile(journey, driver, actions, actor, *repair_tile)
    center_camera_for_tile(
        observer_journey, observer_driver, actions, observer_actor, *repair_tile
    )
    audited_world_pointer(
        journey, driver, actions, actor,
        int(villager["x"]), int(villager["y"]), button=0,
    )
    wait_until(
        f"{actor} repair villager selection",
        lambda: selected_villager if int(journey.telemetry().get(
            "selectedUnit", 0
        )) == selected_villager else None,
    )
    audited_command_button(driver, actions, actor, 2)
    audited_world_pointer(journey, driver, actions, actor, *repair_tile)
    repair_frames: list[dict[str, object]] = []
    repair_pixel: dict[str, object] | None = None
    repair_seen = False
    deadline = time.monotonic() + WAIT_SECONDS * 3
    while time.monotonic() < deadline:
        repair_frames.extend(capture_correlated_frames(
            host, join, seconds=0.2, artifact_dir=artifact_dir,
            label=f"{actor}-repair-house", maximum_samples=4,
        ))
        render_states = [render_diagnostics(value) or {}
                         for value in (host, join)]
        entities = [
            next((entity for entity in state.get("entities", [])
                  if isinstance(entity, dict) and
                  int(entity.get("id", -1)) == selected_villager), None)
            for state in render_states
        ]
        repairing = all(
            isinstance(entity, dict) and entity.get("action") == "repairing"
            for entity in entities
        )
        repair_seen = repair_seen or repairing
        if repair_pixel is None and repairing:
            positions = []
            for entity in entities:
                current = entity.get("simulationPosition")
                target = entity.get("workTarget")
                if not isinstance(current, dict) or not isinstance(
                    target, dict
                ):
                    positions = []
                    break
                positions.append((
                    (int(current["x"]), int(current["y"])),
                    (int(target["x"]), int(target["y"])),
                ))
            if len(positions) == 2 and positions[0] == positions[1]:
                repair_pixel = capture_catalog_semantic_pixels(
                    host, join, artifact_dir,
                    f"{actor}-repair-house-semantic",
                    int(selected_villager), owner=owner,
                    unit_kind="unit-villager", action="repairing",
                    catalog_ids=["villager-repairing"],
                    phase=f"{actor}-repair-house",
                    direction_positions=positions[0],
                )
        if repair_seen and not repairing:
            break
    else:
        raise Failure(f"{actor} repair did not complete")
    if repair_pixel is None:
        raise Failure(f"{actor} repair lacked drawable pixel proof")
    repair_oracle = analyze_render_samples_for_audit(
        repair_frames, f"{actor}-repair-house"
    )
    return {
        "owner": owner,
        "initialGold": int(resource_lifetimes["gold"]["initial"]),
        "gatheredGold": int(resource_lifetimes["gold"]["banked"]),
        "resourceLifetimes": resource_lifetimes,
        "gatherFrames": resource_lifetimes["gold"]["frames"],
        "gatherPixelCapture": resource_lifetimes["gold"][
            "gatherPixelCapture"
        ],
        "returnPixelCapture": resource_lifetimes["gold"][
            "returnPixelCapture"
        ],
        "gatherRenderOracle": resource_lifetimes["gold"]["renderOracle"],
        "constructedBuildingIds": sorted(constructed_ids),
        "constructionFrames": construction_frames,
        "constructionPixelCapture": construction_pixel,
        "constructionRenderOracle": construction_oracle,
        "repairFrames": repair_frames,
        "repairPixelCapture": repair_pixel,
        "repairRenderOracle": repair_oracle,
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
        if int(unit["owner"]) == owner and int(unit["kind"]) in {2, 9}
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
        # select each authoritative military tile through ordinary visible
        # world input rather than relying on one sprite hotspot.
        for _ in range(max(8, len(military_ids) * 4)):
            current_military = [
                unit for unit in
                (diagnostics(driver) or {}).get("game", {}).get("units", [])
                if int(unit.get("id", -1)) in military_ids and
                int(unit.get("id", -1)) not in ordered_ids and
                not bool(unit.get("moving", False))
            ]
            if not current_military:
                time.sleep(0.25)
                continue
            unit = current_military[0]
            tile = (int(unit["x"]), int(unit["y"]))
            center_camera_for_tile(
                journey, driver, actions, actor, tile[0], tile[1]
            )
            audited_world_pointer(
                journey, driver, actions, actor, tile[0], tile[1], button=0
            )
            try:
                unit_id = wait_until(
                    f"{actor} explicit military tile selection",
                    lambda: selected if (selected := int(
                        journey.telemetry().get("selectedUnit", 0)
                    )) in military_ids else None,
                    timeout=1.0,
                )
            except Failure:
                continue
            if selectable_military_id(
                unit_id, military_ids, ordered_ids
            ) is None:
                continue
            pan_world_target_clear(
                journey, driver, actions, actor, target_name,
                edge_margin=(8.0 if target_name == "enemyTarget" else 80.0),
            )
            audited_pointer(journey, actions, actor, target_name, button=2)
            ordered_ids.add(unit_id)
            if len(ordered_ids) == len(military):
                break
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


def browser_renderer_diagnostics(driver) -> dict[str, object]:
    """Read actual production-canvas WebGL identity without changing it."""
    value = driver.execute_script("""
        const canvas = Module.canvas || document.getElementById('canvas');
        const gl = canvas && (
          canvas.getContext('webgl2') || canvas.getContext('webgl')
        );
        if (!gl) return {available: false};
        const extension = gl.getExtension('WEBGL_debug_renderer_info');
        return {
          available: true,
          contextVersion: gl.getParameter(gl.VERSION),
          shadingLanguageVersion: gl.getParameter(
            gl.SHADING_LANGUAGE_VERSION
          ),
          vendor: gl.getParameter(gl.VENDOR),
          renderer: gl.getParameter(gl.RENDERER),
          unmaskedVendor: extension ? gl.getParameter(
            extension.UNMASKED_VENDOR_WEBGL
          ) : null,
          unmaskedRenderer: extension ? gl.getParameter(
            extension.UNMASKED_RENDERER_WEBGL
          ) : null,
        };
    """)
    if not isinstance(value, dict) or not value.get("available"):
        raise Failure("production canvas has no WebGL renderer diagnostics")
    return value


def capture_browser_overlap(
    driver, root: Path, peer: str, *,
    virtual_root: str = "/audit-overlap",
    output_name: str = "overlap",
    entity_id: int | None = None,
) -> int:
    """Export renderer-produced matched pixels from browser virtual FS."""
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
    output_root = root / output_name
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
        metadata = case.get("metadata", {})
        if entity_id is not None and int(
            metadata.get("entity_id", -1)
        ) != entity_id:
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
            "metadata": metadata,
        })
        count += 1
    aggregate_path.write_text(
        json.dumps(aggregate, indent=2, sort_keys=True) + "\n"
    )
    return count


def request_correlated_pixel_capture(
    host, join, root: Path, label: str, entity_id: int,
) -> dict[str, object]:
    """Capture each peer's next real production frame after same command."""
    safe_label = "".join(
        character if character.isalnum() or character in "-_" else "-"
        for character in label
    )
    virtual_root = f"/audit-pixels/{safe_label}"
    for driver in (host, join):
        driver.execute_script(
            "Module.browserPixelCaptureComplete = null;"
            "Module.browserPixelCaptureRequest = arguments[0];",
            virtual_root,
        )
    captures: dict[str, object] = {}
    for peer, driver in (("host", host), ("join", join)):
        wait_until(
            f"{peer} exact pixel capture {safe_label}",
            lambda: True if driver.execute_script(
                "return Module.browserPixelCaptureComplete === arguments[0];",
                virtual_root,
            ) else None,
            timeout=WAIT_SECONDS,
        )
        output_name = f"pixel-oracle/{safe_label}/{peer}"
        count = capture_browser_overlap(
            driver, root, peer, virtual_root=virtual_root,
            output_name=output_name, entity_id=entity_id,
        )
        if count != 1:
            raise Failure(
                f"{peer} exact pixel capture found {count} layers for "
                f"entity {entity_id}"
            )
        render_state = render_diagnostics(driver) or {}
        entity_state = next((
            entity for entity in render_state.get("entities", [])
            if isinstance(entity, dict) and
            int(entity.get("id", -1)) == entity_id
        ), {})
        authoritative = game_diagnostics(driver) or {}
        captures[peer] = {
            "manifest": f"{output_name}/manifest.json",
            "entityId": entity_id,
            "tick": render_state.get("tick"),
            "renderFrame": render_state.get("frame"),
            "authoritativeTick": authoritative.get("currentTick"),
            "authoritativeHash": authoritative.get("stateHash"),
            "previousPosition": entity_state.get("previousPosition"),
            "currentPosition": entity_state.get("simulationPosition"),
            "destinationPosition": entity_state.get("destination"),
            "actualLogicalDirection": entity_state.get("facing"),
        }
    return {
        "virtualRoot": virtual_root,
        "entityId": entity_id,
        "peers": captures,
    }


def capture_catalog_semantic_pixels(
    host, join, root: Path, label: str, entity_id: int, *, owner: int,
    unit_kind: str, action: str, catalog_ids: list[str], phase: str,
    direction_positions: tuple[tuple[int, int], tuple[int, int]] | None = None,
) -> dict[str, object] | None:
    """Retain peer pixel proof using direction derived from captured motion."""
    capture = request_correlated_pixel_capture(
        host, join, root, label, entity_id,
    )
    peer_directions: dict[str, int] = {}
    manifests: dict[str, tuple[Path, dict[str, object]]] = {}
    for peer in ("host", "join"):
        capture_metadata = capture["peers"][peer]
        manifest_path = root / capture_metadata["manifest"]
        manifest = json.loads(manifest_path.read_text())
        cases = manifest.get("cases", [])
        if len(cases) != 1:
            raise Failure(
                f"{peer} catalog pixel capture has {len(cases)} cases"
            )
        metadata = cases[0].get("metadata", {})
        directional_counts = {
            int(draw.get("direction_count", 1))
            for draw in metadata.get("sprite_frames", [])
            if int(draw.get("direction_count", 1)) > 1
        }
        if len(directional_counts) > 1:
            raise Failure(
                f"{peer} catalog pixel capture has mixed direction counts"
            )
        direction_count = next(iter(directional_counts), 1)
        previous = capture_metadata.get("previousPosition")
        current = capture_metadata.get("currentPosition")
        if not isinstance(previous, dict) or not isinstance(current, dict):
            return None
        previous_position = (int(previous["x"]), int(previous["y"]))
        current_position = (int(current["x"]), int(current["y"]))
        if direction_positions is not None:
            previous_position, current_position = direction_positions
        if previous_position == current_position or direction_count <= 1:
            return None
        peer_directions[peer] = oracle_logical_direction(
            previous_position, current_position, direction_count,
        )
        manifests[peer] = (manifest_path, metadata)
    if len(set(peer_directions.values())) != 1:
        raise Failure(
            f"catalog pixel peer direction divergence: {peer_directions}"
        )
    expected_direction = peer_directions["host"]
    visual_oracles = []
    for peer in ("host", "join"):
        capture_metadata = capture["peers"][peer]
        manifest_path, _ = manifests[peer]
        retained = evaluate_packaged_capture(
            manifest_path=manifest_path,
            graphics_drs=ROOT / "game_data/Data/graphics.drs",
            interface_drs=ROOT / "game_data/Data/interfac.drs",
            expected_logical_direction=expected_direction,
            evidence_directory=manifest_path.parent / "semantic-direction",
        )
        visual_oracles.append({
            **retained,
            "manifestPath": str(manifest_path),
            "oracleKind": "semantic-pixel-direction",
            "phase": phase,
            "peer": peer,
            "owner": owner,
            "unitKind": unit_kind,
            "action": action,
            "entity": entity_id,
            "logicalDirection": expected_direction,
            "expectedLogicalDirection": expected_direction,
            "actualLogicalDirection": capture_metadata.get(
                "actualLogicalDirection"
            ),
            "authoritativeTick": capture_metadata.get("authoritativeTick"),
            "authoritativeHash": capture_metadata.get("authoritativeHash"),
            "renderFrame": capture_metadata.get("renderFrame"),
            "previousPosition": capture_metadata.get("previousPosition"),
            "currentPosition": capture_metadata.get("currentPosition"),
            "destinationPosition": capture_metadata.get(
                "destinationPosition"
            ),
            "screenshot": str(
                manifest_path.parent / retained["images"]["actual"]
            ),
            "transitionKind": "authoritative-step",
            "catalogIds": catalog_ids,
            "assertions": [
                "movement-direction", "resolved-frame", "mirror",
                "pixel-direction",
            ],
        })
    capture["visualOracles"] = visual_oracles
    return capture


def capture_directional_combat_lifetime(
    host, join, root: Path, label: str, *,
    required_catalog_ids: set[str], seconds: float = 12.0,
) -> dict[str, object]:
    """Capture combat until every requested directional effect has pixels."""
    frames: list[dict[str, object]] = []
    captures: dict[str, dict[str, object]] = {}
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline and required_catalog_ids - captures.keys():
        samples = capture_correlated_frames(
            host, join, seconds=0.15, artifact_dir=root,
            label=label, maximum_samples=4,
        )
        frames.extend(samples)
        if not samples:
            continue
        host_entities = {
            (str(entity.get("category", "")), int(entity.get("id", -1))):
                entity
            for entity in (samples[-1].get("host") or {}).get("entities", [])
            if isinstance(entity, dict)
        }
        join_entities = {
            (str(entity.get("category", "")), int(entity.get("id", -1))):
                entity
            for entity in (samples[-1].get("join") or {}).get("entities", [])
            if isinstance(entity, dict)
        }
        for key in host_entities.keys() & join_entities.keys():
            category, entity_id = key
            catalog_id = (
                "projectile-impact-orientation"
                if category.startswith("projectile-")
                else "death-decay-direction"
                if category.startswith("unit-death-") else None
            )
            if catalog_id is None or catalog_id not in required_catalog_ids or \
                    catalog_id in captures:
                continue
            entities = (host_entities[key], join_entities[key])
            if not all(entity.get("layers") for entity in entities):
                continue
            directions = {
                int(layer.get("directionCount", 1))
                for entity in entities for layer in entity.get("layers", [])
                if int(layer.get("directionCount", 1)) > 1
            }
            if not directions:
                continue
            positions = []
            for entity in entities:
                previous = entity.get("previousPosition")
                current = entity.get("simulationPosition")
                if not isinstance(previous, dict) or not isinstance(
                    current, dict
                ):
                    positions = []
                    break
                positions.append((
                    (int(previous["x"]), int(previous["y"])),
                    (int(current["x"]), int(current["y"])),
                ))
            if len(positions) != 2 or positions[0] != positions[1] or \
                    positions[0][0] == positions[0][1]:
                continue
            capture = capture_catalog_semantic_pixels(
                host, join, root, f"{label}-{catalog_id}", entity_id,
                owner=int(entities[0].get("owner", -1)),
                unit_kind=category,
                action=("flying" if category.startswith("projectile-")
                        else "dying"),
                catalog_ids=[catalog_id], phase=label,
                direction_positions=positions[0],
            )
            if capture is not None:
                captures[catalog_id] = capture
    missing = sorted(required_catalog_ids - captures.keys())
    return {
        "frames": frames,
        "pixelCaptures": captures,
        "missingCatalogIds": missing,
        "renderOracle": (
            analyze_render_samples_for_audit(frames, label) if frames else None
        ),
    }


def exercise_ranged_moving_death(
    attacker_journey: Journey, attacker_driver, attacker_actor: str,
    attacker_owner: int, target_journey: Journey, target_driver,
    target_actor: str, host, join, actions: list[dict[str, object]],
    root: Path, attacker_tile: tuple[int, int],
    target_tile: tuple[int, int], target_destination: tuple[int, int],
) -> dict[str, object]:
    """Attack one visibly fleeing unit and retain projectile/death pixels."""
    for journey, driver, actor in (
        (attacker_journey, attacker_driver, attacker_actor),
        (target_journey, target_driver, target_actor),
    ):
        center_camera_for_tile(
            journey, driver, actions, actor, *target_tile
        )
    audited_world_pointer(
        target_journey, target_driver, actions, target_actor,
        *target_tile, button=0,
    )
    target_id = wait_until(
        f"{target_actor} moving-death target selection",
        lambda: int(target_journey.telemetry().get("selectedUnit", 0)) or None,
    )
    audited_world_pointer(
        attacker_journey, attacker_driver, actions, attacker_actor,
        *attacker_tile, button=0,
    )
    attacker_id = wait_until(
        f"{attacker_actor} ranged attacker selection",
        lambda: int(attacker_journey.telemetry().get("selectedUnit", 0)) or None,
    )
    audited_world_pointer(
        attacker_journey, attacker_driver, actions, attacker_actor,
        *target_tile,
    )
    audited_world_pointer(
        target_journey, target_driver, actions, target_actor,
        *target_destination,
    )
    lifetime = capture_directional_combat_lifetime(
        host, join, root, f"{attacker_actor}-ranged-moving-death",
        required_catalog_ids={
            "projectile-impact-orientation", "death-decay-direction",
        }, seconds=WAIT_SECONDS * 2,
    )
    if lifetime["missingCatalogIds"]:
        raise Failure(
            f"{attacker_actor} ranged moving death lacks catalog pixels: "
            f"{lifetime['missingCatalogIds']}"
        )
    games = wait_until(
        f"{attacker_actor} moving target death lockstep",
        lambda: matching_games(host, join), timeout=WAIT_SECONDS,
    )
    if any(
        any(int(unit.get("id", -1)) == target_id
            for unit in game.get("units", []))
        for game in games
    ):
        raise Failure(f"{attacker_actor} moving target survived ranged duel")
    return {
        "attackerActor": attacker_actor,
        "attackerOwner": attacker_owner,
        "attackerId": attacker_id,
        "targetActor": target_actor,
        "targetId": target_id,
        **lifetime,
    }


def wait_for_drawable_direction(
    host, join, *, owner: int, entity_id: int, direction: int,
    baseline_position: tuple[int, int],
) -> None:
    def matched() -> bool | None:
        for driver in (host, join):
            state = render_diagnostics(driver) or {}
            entity = next((
                candidate for candidate in state.get("entities", [])
                if isinstance(candidate, dict) and
                int(candidate.get("id", -1)) == entity_id and
                int(candidate.get("owner", -1)) == owner
            ), None)
            if not entity or not bool(entity.get("interpolating", False)):
                return None
            previous = entity.get("previousPosition")
            current = entity.get("simulationPosition")
            if not isinstance(previous, dict) or not isinstance(current, dict):
                return None
            current_position = (int(current["x"]), int(current["y"]))
            if current_position == baseline_position:
                return None
            resolved = oracle_logical_direction(
                (int(previous["x"]), int(previous["y"])),
                current_position, 8,
            )
            if resolved != direction:
                return None
        return True

    wait_until(
        f"entity {entity_id} drawable direction {direction}", matched,
        timeout=WAIT_SECONDS,
    )


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
        artifact_dir: Path | None = None,
        seed: int = 0xA0E20260812,
        viewport: tuple[int, int] = (1280, 900),
        dpr: float = 1.0, zoom: float = 1.0,
        browser_arguments: list[str] | None = None,
        action_limit: int | None = None) -> dict[str, object]:
    if artifact_dir is None:
        artifact_dir = allocate_audit_directory()
    if not (DIST / "aoe_web.html").exists():
        raise Failure("packaged browser distribution is missing")
    artifact_dir.mkdir(parents=True, exist_ok=True)
    evidence: dict[str, object] = {
        "relays": [],
        "relaySource": ("explicit-override" if relays is not None
                        else "packaged-production-default"),
        "actions": BoundedActionLog(action_limit),
        "actionSeed": seed,
    }
    coverage_specification = load_specification(
        ROOT / "resources" / "nostr-visual-gameplay-coverage.json"
    )
    evidence["actionPlan"] = coverage_priority_plan(
        coverage_specification, [], seed
    )
    evidence["actionPlan"]["directionOrderByOwner"] = {
        str(owner): coverage_priority_directions(
            evidence["actionPlan"], owner, seed
        )
        for owner in (0, 1)
    }
    actions = evidence["actions"]
    host = make_driver("chrome", headed, browser_arguments)
    join = make_driver("chrome", headed, browser_arguments)
    for driver in (host, join):
        if not hasattr(driver, "execute_cdp_cmd"):
            raise Failure("display emulation requires Chrome CDP")
        driver.execute_cdp_cmd(
            "Emulation.setDeviceMetricsOverride",
            {
                "width": viewport[0], "height": viewport[1],
                "deviceScaleFactor": dpr, "mobile": False,
            },
        )
    evidence["browser"] = {
        "host": host.capabilities,
        "join": join.capabilities,
        "arguments": list(browser_arguments or []),
    }
    with static_server(port) as (base_url, requests):
        host_journey: Journey | None = None
        join_journey: Journey | None = None
        try:
            host_journey = launch(host, base_url, "host", relays)
            evidence["browser"]["hostRenderer"] = \
                browser_renderer_diagnostics(host)
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
            evidence["browser"]["joinRenderer"] = \
                browser_renderer_diagnostics(join)
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
            audited_zoom(host_journey, host, actions, "host", zoom)
            audited_zoom(join_journey, join, actions, "join", zoom)
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
            host_movement_probe = lambda: matching_moved_villager(
                    host, join, selected_id,
                    before_positions[0][selected_id],
                )
            replacement = None
            try:
                movement_after = wait_until(
                    "matching world movement on both peers",
                    host_movement_probe, timeout=30.0,
                )
            except Failure:
                replacement_destination = deterministic_replacement_destination(
                    host_start, host_destination, seed, owner=0,
                )
                audited_world_pointer(
                    host_journey, host, actions, "host", *host_start, button=0,
                )
                wait_until(
                    "host replacement command unit selection",
                    lambda: selected_id if int(
                        host_journey.telemetry().get("selectedUnit", 0)
                    ) == selected_id else None,
                )
                audited_world_pointer(
                    host_journey, host, actions, "host",
                    *replacement_destination,
                )
                replacement = {
                    "reason": "no-authoritative-progress-for-30-seconds",
                    "attempted": {"x": host_destination[0],
                                  "y": host_destination[1]},
                    "replacement": {"x": replacement_destination[0],
                                    "y": replacement_destination[1]},
                    "seed": seed,
                }
                movement_after = wait_until(
                    "matching replacement world movement on both peers",
                    host_movement_probe, timeout=WAIT_SECONDS - 30.0,
                )
            evidence["movement"] = {
                "unitId": selected_id,
                "stuckActionReplacement": replacement,
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
            red_before = [owned_villager_positions(game, 1)
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
            join_movement_probe = lambda: matching_moved_owned_unit(
                    host, join, 1, red_selected_id,
                    red_before[0][red_selected_id],
                )
            join_replacement = None
            try:
                red_after = wait_until(
                    "matching join world movement on both peers",
                    join_movement_probe, timeout=30.0,
                )
            except Failure:
                replacement_destination = deterministic_replacement_destination(
                    join_start, join_destination, seed, owner=1,
                )
                audited_world_pointer(
                    join_journey, join, actions, "join", *join_start, button=0,
                )
                wait_until(
                    "join replacement command unit selection",
                    lambda: red_selected_id if int(
                        join_journey.telemetry().get("selectedUnit", 0)
                    ) == red_selected_id else None,
                )
                audited_world_pointer(
                    join_journey, join, actions, "join",
                    *replacement_destination,
                )
                join_replacement = {
                    "reason": "no-authoritative-progress-for-30-seconds",
                    "attempted": {"x": join_destination[0],
                                  "y": join_destination[1]},
                    "replacement": {"x": replacement_destination[0],
                                    "y": replacement_destination[1]},
                    "seed": seed,
                }
                red_after = wait_until(
                    "matching replacement join movement on both peers",
                    join_movement_probe, timeout=WAIT_SECONDS - 30.0,
                )
            evidence["joinMovement"] = {
                "unitId": red_selected_id,
                "stuckActionReplacement": join_replacement,
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
            actor_specs = {
                0: ("host", host_journey, host, "join", join_journey, join,
                    (20, 12)),
                1: ("join", join_journey, join, "host", host_journey, host,
                    (28, 12)),
            }
            owner_order = list(evidence["actionPlan"]["ownerOrder"])
            evidence["allDirections"] = {}
            for owner in owner_order:
                actor, journey, driver, observer_actor, observer_journey, \
                    observer_driver, center = actor_specs[int(owner)]
                route_progress: dict[str, object] = {}
                evidence["allDirections"][actor] = route_progress
                evidence["allDirections"][actor] = exercise_all_direction_route(
                    journey, driver, actor, int(owner),
                    observer_journey, observer_driver, observer_actor,
                    host, join, actions, artifact_dir, center, seed,
                    list(evidence["actionPlan"]["directionOrderByOwner"][
                        str(owner)
                    ]),
                    route_progress,
                )
            evidence["transitionRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, observer_actor, observer_journey, \
                    observer_driver, center = actor_specs[int(owner)]
                evidence["transitionRoutes"][actor] = exercise_transition_routes(
                    journey, driver, actor, int(owner),
                    int(evidence["allDirections"][actor]["unitId"]),
                    observer_journey, observer_driver, observer_actor,
                    host, join, actions, artifact_dir, center,
                )
            obstacle_specs = {
                0: ((7, 17), (13, 17), (10, 17)),
                1: ((34, 13), (40, 13), (37, 13)),
            }
            evidence["obstacleRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, _, _, _, _ = actor_specs[int(owner)]
                start, destination, obstacle = obstacle_specs[int(owner)]
                evidence["obstacleRoutes"][actor] = \
                    exercise_obstacle_detour_route(
                        journey, driver, actor, int(owner),
                        int(evidence["allDirections"][actor]["unitId"]),
                        host, join, actions, artifact_dir,
                        start, destination, obstacle,
                    )
            evidence["narrowPassageRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, _, _, _, _ = actor_specs[int(owner)]
                evidence["narrowPassageRoutes"][actor] = \
                    exercise_narrow_passage_route(
                        journey, driver, actor, int(owner),
                        int(evidence["allDirections"][actor]["unitId"]),
                        host, join, actions, artifact_dir,
                    )
            evidence["formationRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, _, _, _, center = actor_specs[
                    int(owner)
                ]
                evidence["formationRoutes"][actor] = \
                    exercise_formation_route(
                        journey, driver, actor, int(owner), host, join,
                        actions, artifact_dir, center,
                    )
            catalog_specs = {
                0: [
                    ((3, 3), (3, 6), "unit-archer",
                     ["archer-ranged-transition", "attack-movement"], "a"),
                    ((5, 3), (5, 6), "unit-battering_ram",
                     ["siege-composite"], None),
                    ((7, 3), (7, 6), "unit-man_at_arms",
                     ["infantry-after-upgrade"], None),
                    ((7, 19), (7, 21), "unit-sheep",
                     ["huntable-herdable-animals"], None),
                ],
                1: [
                    ((44, 28), (44, 25), "unit-archer",
                     ["archer-ranged-transition", "attack-movement"], "a"),
                    ((42, 28), (42, 25), "unit-battering_ram",
                     ["siege-composite"], None),
                    ((40, 28), (40, 25), "unit-man_at_arms",
                     ["infantry-after-upgrade"], None),
                    ((35, 12), (35, 14), "unit-sheep",
                     ["huntable-herdable-animals"], None),
                ],
            }
            evidence["catalogMovement"] = {"host": [], "join": []}
            negotiate_game_speed(host, join, 1)
            for owner in owner_order:
                actor, journey, driver, observer_actor, observer_journey, \
                    observer_driver, _ = actor_specs[int(owner)]
                for start, destination, unit_kind, catalog_ids, command_key in \
                        catalog_specs[int(owner)]:
                    evidence["catalogMovement"][actor].append(
                        exercise_catalog_movement(
                            journey, driver, actor, int(owner),
                            observer_journey, observer_driver, observer_actor,
                            host, join, actions, artifact_dir, start,
                            destination, unit_kind, catalog_ids, command_key,
                        )
                    )
            negotiate_game_speed(host, join, 0)
            evidence["rangedMovingDeaths"] = {
                "host": exercise_ranged_moving_death(
                    host_journey, host, "host", 0,
                    join_journey, join, "join", host, join, actions,
                    artifact_dir, (3, 6), (3, 12), (3, 16),
                ),
                "join": exercise_ranged_moving_death(
                    join_journey, join, "join", 1,
                    host_journey, host, "host", host, join, actions,
                    artifact_dir, (44, 25), (44, 19), (44, 15),
                ),
            }
            patrol_specs = {
                0: ((12, 25), (8, 27)),
                1: ((35, 6), (39, 8)),
            }
            evidence["patrolRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, _, _, _, _ = actor_specs[int(owner)]
                start_hint, destination = patrol_specs[int(owner)]
                evidence["patrolRoutes"][actor] = exercise_patrol_route(
                    journey, driver, actor, int(owner), host, join,
                    actions, artifact_dir, start_hint, destination,
                )
            chase_specs = {
                0: ((24, 9), (20, 9), (12, 9)),
                1: ((20, 27), (24, 27), (32, 27)),
            }
            evidence["chaseRoutes"] = {}
            for owner in owner_order:
                actor, journey, driver, target_actor, target_journey, \
                    target_driver, _ = actor_specs[int(owner)]
                attacker_hint, target_hint, target_destination = chase_specs[
                    int(owner)
                ]
                evidence["chaseRoutes"][actor] = \
                    exercise_moving_target_chase(
                        journey, driver, actor, int(owner),
                        target_journey, target_driver, target_actor,
                        1 - int(owner), host, join, actions, artifact_dir,
                        attacker_hint, target_hint, target_destination,
                    )
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
                    host_journey, host, "host", 0,
                    join_journey, join, "join", host, join, actions,
                    artifact_dir,
                ),
                "join": prepare_player_for_full_match(
                    join_journey, join, "join", 1,
                    host_journey, host, "host", host, join, actions,
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
            host_state = capture_failure_value(
                "host diagnostics", lambda: diagnostics(host)
            )
            join_state = capture_failure_value(
                "join diagnostics", lambda: diagnostics(join)
            )
            relay_blocker = relay_blocker_from_diagnostics(
                host_state, join_state
            )
            failure = {
                "error": f"{type(error).__name__}: {error}",
                "traceback": traceback.format_exc(),
                "completedEvidence": evidence,
                "relays": evidence.get("relays", []),
                "host": host_state,
                "join": join_state,
                "infrastructureBlocker": relay_blocker,
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
            if relay_blocker is not None:
                raise InfrastructureBlocked(
                    f"{error}; reliability={relay_blocker['peers']}"
                ) from error
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


def replayable_action_stream(
    actions: list[dict[str, object]],
) -> list[dict[str, object]]:
    """Add stable order and relative timing without losing raw evidence."""
    result: list[dict[str, object]] = []
    first_monotonic = None
    previous_monotonic = None
    for sequence, action in enumerate(actions):
        monotonic = action.get("monotonic")
        if isinstance(monotonic, (int, float)):
            if first_monotonic is None:
                first_monotonic = float(monotonic)
            elapsed_from_start = round(
                (float(monotonic) - first_monotonic) * 1000.0, 3
            )
            elapsed_from_previous = round(
                0.0 if previous_monotonic is None else
                (float(monotonic) - previous_monotonic) * 1000.0,
                3,
            )
            previous_monotonic = float(monotonic)
        else:
            elapsed_from_start = None
            elapsed_from_previous = None
        result.append({
            **action,
            "sequence": sequence,
            "elapsedFromStartMs": elapsed_from_start,
            "elapsedFromPreviousMs": elapsed_from_previous,
        })
    return result


def failure_bundle_evidence(evidence: dict[str, object]) -> dict[str, object]:
    """Merge completed journey evidence with final failure observations."""
    completed = evidence.get("completedEvidence")
    if not isinstance(completed, dict):
        return evidence
    final = dict(completed)
    for key in (
        "host", "join", "hostRender", "joinRender", "browser", "requests",
        "hostConsole", "joinConsole", "relays", "infrastructureBlocker",
    ):
        if key in evidence:
            final[key] = evidence[key]
    final["failureError"] = evidence.get("error")
    return final


def write_audit_bundle(root: Path, evidence: dict[str, object]) -> None:
    root.mkdir(parents=True, exist_ok=True)
    evidence = failure_bundle_evidence(evidence)
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
        ROOT / "tools/nostr_visual_pixel_oracle.py",
        ROOT / "tools/nostr_packaged_pixel_oracle.py",
        ROOT / "tools/nostr_slp_decoder.py",
        ROOT / "tools/nostr_visual_coverage.py",
        ROOT / "tools/nostr_visual_transition_oracle.py",
        ROOT / "tools/nostr_visual_route_coverage.py",
        ROOT / "tools/nostr_seeded_action_generator.py",
        ROOT / "tools/run_nostr_visual_audit.py",
        ROOT / "tools/run_nostr_visual_display_matrix.py",
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
        "actionSeed": evidence.get(
            "actionSeed", existing_ledger.get("seed")
        ),
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
    write_jsonl(root / "actions.jsonl", replayable_action_stream(actions))
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
    gather_phases: dict[str, list[dict[str, object]]] = {}
    for actor in ("host", "join"):
        player = full_gameplay.get(actor) or {}
        lifetimes = player.get("resourceLifetimes") or {}
        if lifetimes:
            for resource, lifetime in lifetimes.items():
                gather_phases[
                    f"fullGameplay{actor.title()}Gather{resource.title()}"
                ] = (lifetime or {}).get("frames", [])
        else:
            gather_phases[f"fullGameplay{actor.title()}Gather"] = \
                player.get("gatherFrames", [])
    work_phases = dict(gather_phases)
    ranged_deaths = evidence.get("rangedMovingDeaths") or {}
    for actor in ("host", "join"):
        work_phases[f"rangedMovingDeath{actor.title()}"] = \
            (ranged_deaths.get(actor) or {}).get("frames", [])
    for actor in ("host", "join"):
        player = full_gameplay.get(actor) or {}
        work_phases[f"fullGameplay{actor.title()}Construction"] = \
            player.get("constructionFrames", [])
        work_phases[f"fullGameplay{actor.title()}Repair"] = \
            player.get("repairFrames", [])
    for phase, samples in work_phases.items():
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
        "fullGameplayWork": work_phases,
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
    transition_routes = evidence.get("transitionRoutes") or {}
    for actor in ("host", "join"):
        for route_name, route in (
            (transition_routes.get(actor) or {}).get("routes", {})
        ).items():
            phase = f"transition-{actor}-{route_name}"
            for sample in route.get("frames", []):
                for peer in ("host", "join"):
                    render_state = sample.get(peer) or {}
                    provenance.append({
                        "phase": phase,
                        "peer": peer,
                        "frame": render_state.get("frame"),
                        "tick": render_state.get("tick"),
                        "entities": render_state.get("entities", []),
                    })
    formation_routes = evidence.get("formationRoutes") or {}
    obstacle_routes = evidence.get("obstacleRoutes") or {}
    narrow_routes = evidence.get("narrowPassageRoutes") or {}
    for actor in ("host", "join"):
        for sample in (obstacle_routes.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": f"obstacle-{actor}", "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
        for sample in (narrow_routes.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": f"narrow-passage-{actor}", "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
    for actor in ("host", "join"):
        for sample in (formation_routes.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": f"formation-{actor}", "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
    patrol_routes = evidence.get("patrolRoutes") or {}
    chase_routes = evidence.get("chaseRoutes") or {}
    catalog_movement = evidence.get("catalogMovement") or {}
    for actor in ("host", "join"):
        for sample in (patrol_routes.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": f"patrol-{actor}", "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
        for sample in (chase_routes.get(actor) or {}).get("frames", []):
            for peer in ("host", "join"):
                render_state = sample.get(peer) or {}
                provenance.append({
                    "phase": f"chase-{actor}", "peer": peer,
                    "frame": render_state.get("frame"),
                    "tick": render_state.get("tick"),
                    "entities": render_state.get("entities", []),
                })
        for journey in catalog_movement.get(actor, []):
            for sample in journey.get("frames", []):
                for peer in ("host", "join"):
                    render_state = sample.get(peer) or {}
                    provenance.append({
                        "phase": f"catalog-{actor}-{journey.get('catalogId')}",
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
    for phase, samples in work_phases.items():
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
        for route_name, route in (
            (transition_routes.get(actor) or {}).get("routes", {})
        ).items():
            for sample in route.get("frames", []):
                correlated_records.append({
                    "phase": f"transition-{actor}-{route_name}", **sample,
                })
        for sample in (formation_routes.get(actor) or {}).get("frames", []):
            correlated_records.append({
                "phase": f"formation-{actor}", **sample,
            })
        for sample in (patrol_routes.get(actor) or {}).get("frames", []):
            correlated_records.append({
                "phase": f"patrol-{actor}", **sample,
            })
        for sample in (chase_routes.get(actor) or {}).get("frames", []):
            correlated_records.append({
                "phase": f"chase-{actor}", **sample,
            })
        for sample in (obstacle_routes.get(actor) or {}).get("frames", []):
            correlated_records.append({
                "phase": f"obstacle-{actor}", **sample,
            })
        for sample in (narrow_routes.get(actor) or {}).get("frames", []):
            correlated_records.append({
                "phase": f"narrow-passage-{actor}", **sample,
            })
        for journey in catalog_movement.get(actor, []):
            for sample in journey.get("frames", []):
                correlated_records.append({
                    "phase": f"catalog-{actor}-{journey.get('catalogId')}",
                    **sample,
                })
    for actor in ("host", "join"):
        phase = f"allDirections{actor.title()}"
        for sample in (all_directions.get(actor) or {}).get("frames", []):
            correlated_records.append({"phase": phase, **sample})
    for phase, samples in work_phases.items():
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
    route_records: list[dict[str, object]] = []
    for actor in ("host", "join"):
        for route_name, route in (
            (transition_routes.get(actor) or {}).get("routes", {})
        ).items():
            route_records.append({
                "id": route_name, "actor": actor,
                "verdict": (route.get("renderOracle") or {}).get(
                    "verdict", "BLOCKED"
                ),
            })
        obstacle_route = obstacle_routes.get(actor) or {}
        for route_name in obstacle_route.get("routeKind", []):
            route_records.append({
                "id": route_name, "actor": actor,
                "verdict": (obstacle_route.get("renderOracle") or {}).get(
                    "verdict", "BLOCKED"
                ),
            })
        formation_route = formation_routes.get(actor) or {}
        if formation_route:
            route_records.append({
                "id": "formation-regrouping", "actor": actor,
                "verdict": (formation_route.get("renderOracle") or {}).get(
                    "verdict", "BLOCKED"
                ),
            })
        narrow_route = narrow_routes.get(actor) or {}
        if narrow_route:
            route_records.append({
                "id": "narrow-passage", "actor": actor,
                "verdict": (narrow_route.get("renderOracle") or {}).get(
                    "verdict", "BLOCKED"
                ),
            })
        chase_route = chase_routes.get(actor) or {}
        if chase_route:
            route_records.append({
                "id": "moving-target-chase", "actor": actor,
                "verdict": (chase_route.get("renderOracle") or {}).get(
                    "verdict", "BLOCKED"
                ),
            })
    route_coverage = evaluate_route_coverage(
        coverage_specification, route_records
    )
    atomic_write_json(root / "route-coverage.json", route_coverage)
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
    screenshot_failures = [
        finding for finding in screenshot_report.get("findings", [])
        if finding.get("status") == "FAIL"
    ]
    screenshot_status = str(screenshot_report.get("status", "BLOCKED"))
    status = (
        "FAIL" if failures or coverage["status"] == "FAIL" or
        route_coverage["status"] == "FAIL" or screenshot_status == "FAIL"
        else "BLOCKED" if coverage["status"] == "BLOCKED" or
        route_coverage["status"] == "BLOCKED" or
        screenshot_status == "BLOCKED"
        else "PASS"
    )
    atomic_write_json(root / "verdict.json", {
        "schemaVersion": 1,
        "status": status,
        "visualFailureCount": len(failures),
        "screenshotFailureCount": len(screenshot_failures),
        "screenshotStatus": screenshot_status,
        "visualOracleCount": len(visual_oracles),
        "coverageStatus": coverage["status"],
        "routeCoverageStatus": route_coverage["status"],
        "missingRequiredRoutes": route_coverage["missingRequiredRoutes"],
        "missingRequiredCells": coverage["missingRequiredCells"],
        "missingCatalogAssertions": coverage[
            "missingCatalogAssertions"
        ],
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
    parser.add_argument("--viewport", type=parse_viewport, default=(1280, 900))
    parser.add_argument("--dpr", type=float, choices=(1.0, 2.0), default=1.0)
    parser.add_argument("--zoom", type=float, choices=(1.0, 2.0), default=1.0)
    parser.add_argument("--browser-argument", action="append", default=[])
    parser.add_argument("--action-limit", type=int)
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
        viewport=arguments.viewport,
        dpr=arguments.dpr,
        browser_arguments=arguments.browser_argument,
        zoom=arguments.zoom,
        action_limit=arguments.action_limit,
    )
    try:
        configured_relays = (
            arguments.relays.split(",")
            if arguments.relays else list(DEFAULT_RELAYS)
        )
        relay_probe = probe_relay_pool(configured_relays)
        atomic_write_json(audit_dir / "relay-probe.json", relay_probe)
        selected_quorum = relay_probe["selectedQuorum"]
        if len(selected_quorum) < 2:
            raise Failure(
                "relay probe found fewer than two healthy configured relays"
            )
        run_ledger = json.loads(
            (audit_dir / "run.json").read_text(encoding="utf-8")
        )
        run_ledger["relayPool"] = configured_relays
        run_ledger["selectedQuorum"] = selected_quorum
        atomic_write_json(audit_dir / "run.json", run_ledger)
        evidence = run(
            ",".join(selected_quorum), arguments.headed, arguments.port,
            checkpoint=arguments.checkpoint,
            artifact_dir=audit_dir,
            seed=arguments.seed,
            viewport=arguments.viewport,
            dpr=arguments.dpr,
            zoom=arguments.zoom,
            browser_arguments=arguments.browser_argument,
            action_limit=arguments.action_limit,
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
        replay_path = audit_dir / "causal-replay-prefix.json"
        replay = causal_replay_prefix(
            list((failure.get("completedEvidence") or {}).get("actions", [])),
            failure,
        )
        atomic_write_json(replay_path, replay)
        try:
            failure["causalReplayPrefixPath"] = str(
                replay_path.relative_to(ROOT)
            )
        except ValueError:
            failure["causalReplayPrefixPath"] = str(replay_path)
        failure_path.write_text(
            json.dumps(failure, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        evidence_path.write_text(
            json.dumps(failure, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        verdict_path = audit_dir / "verdict.json"
        current_verdict = (
            json.loads(verdict_path.read_text(encoding="utf-8"))
            if verdict_path.is_file() else {}
        )
        retained_status = (
            "BLOCKED" if isinstance(error, InfrastructureBlocked)
            else "FAIL" if current_verdict.get("status") == "FAIL"
            else "BLOCKED"
        )
        if retained_status == "FAIL":
            report_summary = (
                "Retained machine oracle proved a product or audit-oracle "
                "failure before run stopped. See `first-failure.json` and "
                "nested `partialVerdict`."
            )
        else:
            report_summary = (
                "Run stopped before acceptance completed. Infrastructure "
                "versus product classification remains unproved.\n\n"
                f"Primary failure: `{failure.get('error', str(error))}`"
            )
        write_report(
            audit_dir, retained_status, report_summary,
            report_path=destination.report,
        )
        atomic_write_json(verdict_path, {
            "schemaVersion": 1, "status": retained_status,
            "failure": failure.get("error", str(error)),
            "partialVerdict": current_verdict,
        })
        print(
            f"Nostr multiplayer audit {retained_status.lower()}: {audit_dir}",
            file=sys.stderr,
        )
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

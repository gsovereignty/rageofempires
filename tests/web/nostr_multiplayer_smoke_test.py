#!/usr/bin/env python3
"""Two-browser production-path smoke test over ordinary public Nostr relays."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.actions.action_builder import ActionBuilder
from selenium.webdriver.common.actions.pointer_input import PointerInput
from selenium.webdriver.common.by import By
from selenium.common.exceptions import StaleElementReferenceException
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import Select

from browser_risk_spike_test import (
    DIST,
    Failure,
    Journey,
    make_driver,
    static_server,
    wait_until,
)


ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS = ROOT / "artifacts" / "nostr-multiplayer"
DEFAULT_RELAYS = (
    "wss://nostr-pub.wellorder.net,wss://nostr.oxtr.dev,wss://nostr.bond,"
    "wss://relay.nostr.net,wss://yabu.me,wss://relay.nostr.wirednet.jp,"
    "wss://relay.nostr.info,wss://nostr.sathoarder.com,"
    "wss://relay.wavlake.com,wss://relay.noswhere.com"
)
WAIT_SECONDS = 180.0


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
                              label: str = "motion") \
        -> list[dict[str, object]]:
    deadline = time.monotonic() + seconds
    samples: list[dict[str, object]] = []
    last_frames: tuple[int, int] | None = None
    while time.monotonic() < deadline and len(samples) < 24:
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
    click_canvas_logical(driver, logical_x, logical_y, button=button)


def analyze_render_samples(samples: list[dict[str, object]]) \
        -> dict[str, object]:
    if not samples:
        raise Failure("render oracle captured no correlated frames")
    counts = {"frames": len(samples), "entities": 0, "legacy": 0,
              "proceduralFailures": 0, "unprovenSources": 0,
              "unresolvedExpectedMappings": [],
              "animationSequenceBlocked": 0}
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
                        counts["unresolvedExpectedMappings"].append({
                            "peer": peer,
                            "frame": frame,
                            "entity": entity,
                            "reason": "expected asset is not renderable",
                        })
                        continue
                    if not expected_resources:
                        counts["unresolvedExpectedMappings"].append({
                            "peer": peer,
                            "frame": frame,
                            "entity": entity,
                            "reason": "renderable mapping has no expected IDs",
                        })
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
                    if (expected_facing is not None and
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
                (layer.get("resourceId"), layer.get("frame"),
                 layer.get("palettePlayer"),
                 layer.get("flipHorizontal"), layer.get("hotspotX"),
                 layer.get("hotspotY"), layer.get("width"),
                 layer.get("height")) for layer in host_layers
            ]
            join_assets = [
                (layer.get("resourceId"), layer.get("frame"),
                 layer.get("palettePlayer"),
                 layer.get("flipHorizontal"), layer.get("hotspotX"),
                 layer.get("hotspotY"), layer.get("width"),
                 layer.get("height")) for layer in join_layers
            ]
            if host_assets != join_assets:
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
    audited_key(driver, actions, actor, ".")
    wait_until(
        f"{actor} idle villager selection for gathering",
        lambda: int(journey.telemetry().get("selectedUnit", 0)) or None,
    )
    resource_tile = (12, 20) if owner == 0 else (35, 10)
    audited_world_pointer(
        journey, driver, actions, actor, *resource_tile,
    )
    gather_frames = capture_correlated_frames(
        host, join, seconds=2.0,
        artifact_dir=artifact_dir,
        label=f"{actor}-gather",
    )
    gather_oracle = analyze_render_samples(gather_frames)
    gathered = wait_until(
        f"{actor} gathered gold",
        lambda: (
            value if int((value := journey.telemetry())["resources"]["gold"])
            > initial_gold else None
        ),
        timeout=WAIT_SECONDS,
    )

    before_games = wait_until(
        f"{actor} pre-construction lockstep", lambda: matching_games(host, join),
        timeout=WAIT_SECONDS,
    )
    initial_building_count = len(owner_buildings(before_games[0], owner))
    audited_key(driver, actions, actor, ".")
    wait_until(
        f"{actor} idle villager selection for construction",
        lambda: int(journey.telemetry().get("selectedUnit", 0)) or None,
    )
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
        timeout=WAIT_SECONDS,
    )

    audited_pointer(journey, actions, actor, "barracks")
    wait_until(
        f"{actor} barracks selection",
        lambda: int(journey.telemetry().get("selectedBuilding", 0)) or None,
    )
    initial_military = int(journey.telemetry()["blueMilitaryCount"])
    audited_key(driver, actions, actor, "m")
    trained = wait_until(
        f"{actor} militia training",
        lambda: (
            value if int((value := journey.telemetry())["blueMilitaryCount"])
            > initial_military else None
        ),
        timeout=WAIT_SECONDS,
    )
    audited_key(driver, actions, actor, "9")
    researched = wait_until(
        f"{actor} man-at-arms research",
        lambda: (
            value if bool((value := journey.telemetry())[
                "manAtArmsResearched"
            ]) else None
        ),
        timeout=WAIT_SECONDS,
    )
    return {
        "owner": owner,
        "initialGold": initial_gold,
        "gatheredGold": int(gathered["resources"]["gold"]),
        "gatherFrames": gather_frames,
        "gatherRenderOracle": gather_oracle,
        "constructedBuildingIds": sorted(constructed_ids),
        "militaryCount": int(trained["blueMilitaryCount"]),
        "researched": bool(researched["manAtArmsResearched"]),
    }


def order_town_center_attack(
    journey: Journey,
    driver,
    actor: str,
    pan_key: str,
    actions: list[dict[str, object]],
) -> int:
    audited_key(driver, actions, actor, ",")
    wait_until(
        f"{actor} military selection",
        lambda: int(journey.telemetry().get("selectedUnit", 0)) or None,
    )
    for _ in range(48):
        target = journey.telemetry()["targets"]["enemyTownCenter"]
        x = float(target["x"])
        y = float(target["y"])
        if 80 < x < 1200 and 80 < y < 640:
            break
        audited_key(driver, actions, actor, pan_key)
        time.sleep(0.05)
    else:
        raise Failure(f"{actor} enemy town center never entered canvas")
    initial_hit_points = int(journey.telemetry()["enemyTownCenterHitPoints"])
    audited_pointer(journey, actions, actor, "enemyTownCenter", button=2)
    wait_until(
        f"{actor} town-center combat start",
        lambda: (
            hit_points if 0 <= (hit_points := int(journey.telemetry()[
                "enemyTownCenterHitPoints"
            ])) < initial_hit_points else None
        ),
        timeout=WAIT_SECONDS,
    )
    return initial_hit_points


def launch(driver, base_url: str, mode: str, relays: str,
           match_reference: str = "", allied: bool = True) -> Journey:
    driver.get(f"{base_url}/aoe_web.html")
    wait_until(
        f"{mode} browser storage",
        lambda: driver.execute_script(
            "return Module.storageReady === true && "
            "Module.HEAPU8 instanceof Uint8Array && "
            "!document.getElementById('start').hidden"
        ),
    )
    Select(driver.find_element(By.ID, "launch-mode")).select_by_value(mode)
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


def click_canvas_logical(driver, x: float, y: float,
                         button: int = 0) -> None:
    canvas = driver.find_element(By.ID, "canvas")
    rect = canvas.rect
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
    details = driver.find_element(By.ID, "relay-management")
    if not details.get_attribute("open"):
        details.find_element(By.TAG_NAME, "summary").click()
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

    for driver in (host, join):
        set_relay_enabled(driver, 1, True)
    recovered = wait_until(
        "relay EOSE backfill and lockstep recovery",
        lambda: matching_relay_state(
            host, join, disabled=relay_count - 2, status=0, eose=2,
            minimum_tick=max(stopped_ticks) + 1,
        ),
        timeout=WAIT_SECONDS,
    )
    recovery["recovered"] = {
        "host": recovered[0], "join": recovered[1]
    }
    for relay_index in range(relay_count):
        for driver in (host, join):
            set_relay_enabled(driver, relay_index, True)
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
    recovery["allRestored"] = {
        "host": diagnostics(host), "join": diagnostics(join)
    }
    return recovery


def run(relays: str, headed: bool, port: int = 8888,
        checkpoint: bool = False,
        artifact_dir: Path = ARTIFACTS) -> dict[str, object]:
    if not (DIST / "aoe_web.html").exists():
        raise Failure("packaged browser distribution is missing")
    artifact_dir.mkdir(parents=True, exist_ok=True)
    evidence: dict[str, object] = {
        "relays": relays.split(","), "actions": []
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
            host_state = require_quorum(host, "host")
            reference = str(host_state.get("matchReference", ""))
            if not reference.startswith("aoe-nostr:1:"):
                raise Failure(f"invalid host match reference: {reference!r}")

            join_journey = launch(join, base_url, "join", relays, reference)
            require_quorum(join, "join")
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
            audited_pointer(
                host_journey, actions, "host", "villager",
                button=2, logical_dx=50,
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
                "renderOracle": analyze_render_samples(host_motion_frames),
            }

            # Joiner uses local-player-relative production telemetry and sends
            # a distinct Red command through the same visible canvas path.
            red_before_games = [game_diagnostics(driver) or {}
                                for driver in (host, join)]
            red_before = [owned_unit_positions(game, 1)
                          for game in red_before_games]
            if red_before[0] != red_before[1] or not red_before[0]:
                raise Failure(f"peers lack matching red unit: {red_before}")
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
            audited_pointer(
                join_journey, actions, "join", "villager",
                button=2, logical_dx=-50,
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
                "renderOracle": analyze_render_samples(join_motion_frames),
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
                "renderOracle": analyze_render_samples(simultaneous_frames),
            }

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
            host_target_hit_points = order_town_center_attack(
                host_journey, host, "host", Keys.ARROW_RIGHT, actions,
            )
            join_target_hit_points = order_town_center_attack(
                join_journey, join, "join", Keys.ARROW_LEFT, actions,
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
                "renderOracle": analyze_render_samples(combat_frames),
            }

            # Transport chaos comes only after retained two-sided economy,
            # construction, production, research, motion, provenance, and
            # active combat evidence.
            evidence["recovery"] = exercise_relay_chaos(host, join, relays)

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
            click_canvas_logical(host, 1165, 330)
            time.sleep(0.25)
            host.save_screenshot(str(artifact_dir / "signal-armed.png"))
            host.find_element(By.ID, "canvas").click()
            wait_until(
                "public map signal delivery",
                lambda: (
                    True
                    if all(int((game_diagnostics(driver) or {}).get(
                        "signalCount", 0
                    )) >= 1 for driver in (host, join))
                    else None
                ),
                timeout=WAIT_SECONDS,
            )

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
                timeout=300.0,
            )
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
                "completedEvidence": evidence,
                "relays": relays.split(","),
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
    ]
    source_digests = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in source_paths
    }
    host = evidence.get("host", {})
    join = evidence.get("join", {})
    run_ledger = {
        "schemaVersion": 1,
        "completedUtc": datetime.now(timezone.utc).isoformat(),
        "sourceCommit": commit,
        "package": str(package.relative_to(ROOT)),
        "packageSha256": package_digests,
        "sourceFilesSha256": source_digests,
        "browser": evidence.get("browser"),
        "relays": evidence.get("relays", []),
        "hostPublicKey": host.get("publicKey") if isinstance(host, dict) else None,
        "joinPublicKey": join.get("publicKey") if isinstance(join, dict) else None,
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
    (root / "run.json").write_text(
        json.dumps(run_ledger, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
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
    (root / "console-host.json").write_text(
        json.dumps(evidence.get("hostConsole", []), indent=2) + "\n",
        encoding="utf-8",
    )
    (root / "console-join.json").write_text(
        json.dumps(evidence.get("joinConsole", []), indent=2) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--relays", default=DEFAULT_RELAYS)
    parser.add_argument("--port", type=int, default=8888)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--checkpoint", action="store_true")
    parser.add_argument(
        "--evidence", type=Path,
        default=ARTIFACTS / "production-smoke.json",
    )
    arguments = parser.parse_args()
    evidence = run(
        arguments.relays, arguments.headed, arguments.port,
        checkpoint=arguments.checkpoint,
        artifact_dir=arguments.evidence.parent,
    )
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    write_audit_bundle(arguments.evidence.parent, evidence)
    print(f"Nostr multiplayer smoke passed: {arguments.evidence}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

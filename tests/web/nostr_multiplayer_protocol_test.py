#!/usr/bin/env python3
"""Bounded two-browser production-path Nostr protocol acceptance."""

from __future__ import annotations

import argparse
import json
import re
import secrets
import subprocess
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path

from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys

from browser_risk_spike_test import DIST, Failure, Journey, make_driver, \
    static_server, wait_until
from nostr_multiplayer_smoke_test import (
    DEFAULT_RELAYS,
    ROOT,
    audited_key,
    audited_pointer,
    audited_world_pointer,
    atomic_write_json,
    capture_failure_value,
    center_camera_for_tile,
    click_canvas_logical,
    diagnostics,
    deterministic_replacement_destination,
    exercise_relay_chaos,
    game_diagnostics,
    install_publish_intent_probe,
    key_chord,
    launch,
    matching_games,
    matching_moved_owned_unit,
    owned_villager_positions,
    probe_relay_pool,
    publish_intent_probe,
    relay_blocker_from_diagnostics,
    require_canonical_relay_identity,
    require_quorum,
    select_waiting_session,
    write_jsonl,
)


ARTIFACT_ROOT = ROOT / "artifacts" / "nostr-multiplayer"
REPORT_ROOT = ROOT / "docs" / "audits"
WAIT_SECONDS = 180.0
HEX64 = re.compile(r"[0-9a-f]{64}")
PRIVATE_MARKERS = ("privatekey", "private_key", "secretkey", "secret_key", "nsec1")


def private_material_absent(value: object) -> bool:
    serialized = json.dumps(value, sort_keys=True).lower()
    return not any(marker in serialized for marker in PRIVATE_MARKERS)


def _hex64(value: object) -> bool:
    return isinstance(value, str) and HEX64.fullmatch(value) is not None


def _successful_publications(state: dict[str, object]) -> int:
    count = 0
    for publication in state.get("recentPublications", []):
        if not isinstance(publication, dict):
            continue
        results = publication.get("results", [])
        if isinstance(results, list) and sum(
            1 for result in results
            if isinstance(result, dict) and bool(result.get("ok"))
        ) >= 2:
            count += 1
    return count


def validate_identity_lobby(
    host: dict[str, object], join: dict[str, object],
) -> dict[str, object]:
    """Require independently signed peers and one exact accepted lobby."""
    host_key = host.get("publicKey")
    join_key = join.get("publicKey")
    if not _hex64(host_key) or not _hex64(join_key) or host_key == join_key:
        raise Failure("host/join ephemeral public identities are invalid or equal")
    if not private_material_absent({"host": host, "join": join}):
        raise Failure("browser diagnostics contain private signer material")

    exact_top_level = (
        "matchId", "hostPublicKey", "matchReference", "relays",
        "relayPoolDigest", "compatibilityDigest",
    )
    differing = [field for field in exact_top_level if host.get(field) != join.get(field)]
    if differing:
        raise Failure(f"peer match identity differs: {differing}")
    if not _hex64(host.get("matchId")) or host.get("hostPublicKey") != host_key:
        raise Failure("host match identity is not canonical")
    if len(host.get("eoseRelays", [])) < 2 or len(join.get("eoseRelays", [])) < 2:
        raise Failure("peer relay EOSE quorum is absent")

    games = [state.get("game") for state in (host, join)]
    if not all(isinstance(game, dict) for game in games):
        raise Failure("peer game diagnostics are absent")
    host_game, join_game = games
    assert isinstance(host_game, dict) and isinstance(join_game, dict)
    equal_game_fields = (
        "protocolVersion", "epoch", "hostPublicKey", "bluePublicKey",
        "redPublicKey", "configDigest", "lobbyRevision", "lobbyEventId",
        "blueAckEventId", "redAckEventId", "blueReadyEventId",
        "redReadyEventId",
    )
    differing = [
        field for field in equal_game_fields
        if host_game.get(field) != join_game.get(field)
    ]
    if differing:
        raise Failure(f"peer accepted-lobby diagnostics differ: {differing}")
    if host_game.get("protocolVersion") != 1 or host_game.get("epoch") != 1:
        raise Failure("unexpected Nostr match protocol version or epoch")
    if host_game.get("localSlot") != 0 or join_game.get("localSlot") != 1:
        raise Failure("local player slots do not map host=blue and join=red")
    if host_game.get("bluePublicKey") != host_key or \
            host_game.get("redPublicKey") != join_key:
        raise Failure("accepted roster pubkeys do not map to peer identities")
    if int(host_game.get("lobbyRevision", 0)) < 2:
        raise Failure("accepted lobby revision never closed the two-player roster")
    for field in (
        "lobbyEventId", "blueAckEventId", "redAckEventId",
        "blueReadyEventId", "redReadyEventId",
    ):
        if not _hex64(host_game.get(field)):
            raise Failure(f"accepted lobby lacks exact {field}")
    if not all(
        bool(game.get("blueReady")) and bool(game.get("redReady"))
        for game in (host_game, join_game)
    ):
        raise Failure("both ready events are not accepted by both peers")
    if _successful_publications(host) < 1 or _successful_publications(join) < 1:
        raise Failure("both peers lack a relay-quorum publication receipt")
    return {
        "hostPublicKey": host_key,
        "joinPublicKey": join_key,
        "matchId": host.get("matchId"),
        "matchReference": host.get("matchReference"),
        "protocolVersion": host_game.get("protocolVersion"),
        "epoch": host_game.get("epoch"),
        "lobbyRevision": host_game.get("lobbyRevision"),
        "lobbyEventId": host_game.get("lobbyEventId"),
        "blueSlotPublicKey": host_game.get("bluePublicKey"),
        "redSlotPublicKey": host_game.get("redPublicKey"),
        "privateMaterialAbsent": True,
    }


def require_matching_lockstep(host, join, minimum_tick: int = 0):
    games = matching_games(host, join)
    if games is None:
        return None
    if int(games[0].get("currentTick", -1)) < minimum_tick:
        return None
    if any(game.get("blueMissing") or game.get("redMissing") for game in games):
        return None
    return games


def issue_move(
    actor: str, owner: int, journey: Journey, driver, host, join,
    actions: list[dict[str, object]],
) -> dict[str, object]:
    before_games = [game_diagnostics(peer) or {} for peer in (host, join)]
    positions = [owned_villager_positions(game, owner) for game in before_games]
    if positions[0] != positions[1] or not positions[0]:
        raise Failure(f"{actor} lacks a matching owned villager")
    unit_id = min(positions[0])
    start = positions[0][unit_id]
    destination = (14, 22) if owner == 0 else (29, 16)
    center_camera_for_tile(journey, driver, actions, actor, *destination)
    audited_key(driver, actions, actor, ".")
    wait_until(
        f"{actor} selected owned villager",
        lambda: unit_id if int(journey.telemetry().get("selectedUnit", 0)) == unit_id else None,
        timeout=WAIT_SECONDS,
    )
    audited_world_pointer(journey, driver, actions, actor, *destination)
    replacement = None
    try:
        after = wait_until(
            f"{actor} command applied identically",
            lambda: matching_moved_owned_unit(host, join, owner, unit_id, start),
            timeout=30.0,
        )
    except Failure:
        alternate = deterministic_replacement_destination(
            start, destination, 0xA0E20260819, owner=owner
        )
        audited_world_pointer(
            journey, driver, actions, actor, *start, button=0
        )
        wait_until(
            f"{actor} replacement selected owned villager",
            lambda: unit_id if int(
                journey.telemetry().get("selectedUnit", 0)
            ) == unit_id else None,
            timeout=WAIT_SECONDS,
        )
        audited_world_pointer(journey, driver, actions, actor, *alternate)
        after = wait_until(
            f"{actor} replacement command applied identically",
            lambda: matching_moved_owned_unit(host, join, owner, unit_id, start),
            timeout=WAIT_SECONDS - 30.0,
        )
        replacement = {
            "reason": "no-authoritative-progress-for-30-seconds",
            "destination": {"x": alternate[0], "y": alternate[1]},
        }
    return {
        "actor": actor, "owner": owner, "unitId": unit_id,
        "start": {"x": start[0], "y": start[1]},
        "destination": {"x": destination[0], "y": destination[1]},
        "replacement": replacement,
        "finalTick": after[0].get("currentTick"),
        "finalStateHash": after[0].get("stateHash"),
    }


def resign(driver) -> None:
    canvas = driver.find_element(By.ID, "canvas")
    canvas.click()
    ActionChains(driver).key_down(Keys.CONTROL, canvas).key_down(
        Keys.SHIFT, canvas
    ).send_keys_to_element(canvas, "r").key_up(Keys.SHIFT, canvas).key_up(
        Keys.CONTROL, canvas
    ).perform()


def _milestone(evidence: dict[str, object], name: str, host, join) -> None:
    evidence.setdefault("milestones", []).append({
        "name": name,
        "host": diagnostics(host),
        "join": diagnostics(join),
    })


def run_protocol(
    headed: bool, port: int, artifact_dir: Path,
    browser_arguments: list[str] | None = None,
) -> dict[str, object]:
    if not (DIST / "aoe_web.html").exists():
        raise Failure("packaged browser distribution is missing")
    actions: list[dict[str, object]] = []
    evidence: dict[str, object] = {
        "scope": "bounded-protocol-only",
        "relays": [],
        "actions": actions,
        "milestones": [],
    }
    host = make_driver("chrome", headed, browser_arguments)
    join = make_driver("chrome", headed, browser_arguments)
    evidence["browser"] = {
        "host": host.capabilities, "join": join.capabilities,
        "arguments": list(browser_arguments or []),
    }
    with static_server(port) as (base_url, requests):
        try:
            host_journey = launch(host, base_url, "host")
            install_publish_intent_probe(host)
            evidence["hostRelayIdentity"] = require_canonical_relay_identity(host, "host")
            active_relays = str(host.find_element(By.ID, "relays").get_attribute("value") or "")
            evidence["relays"] = active_relays.split(",")
            host_state = require_quorum(host, "host")
            join_journey = launch(join, base_url, "join")
            install_publish_intent_probe(join)
            evidence["joinRelayIdentity"] = require_canonical_relay_identity(join, "join")
            evidence["selectedWaitingSession"] = select_waiting_session(
                join, str(host_state.get("publicKey", ""))
            )
            require_quorum(join, "join")
            wait_until(
                "canonical lobby revision 2",
                lambda: True if all(
                    int((game_diagnostics(peer) or {}).get("lobbyRevision", 0)) >= 2
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            key_chord(host, "r")
            key_chord(join, "r")
            wait_until(
                "both exact-lobby ready events",
                lambda: True if all(
                    bool((game_diagnostics(peer) or {}).get("blueReady")) and
                    bool((game_diagnostics(peer) or {}).get("redReady"))
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            click_canvas_logical(host, 842, 516)
            wait_until(
                "eight equal lockstep ticks",
                lambda: require_matching_lockstep(host, join, 8),
                timeout=WAIT_SECONDS,
            )
            evidence["identityLobby"] = validate_identity_lobby(
                diagnostics(host) or {}, diagnostics(join) or {}
            )
            _milestone(evidence, "identity-lobby-start", host, join)

            evidence["commands"] = [
                issue_move("host", 0, host_journey, host, host, join, actions),
                issue_move("join", 1, join_journey, join, host, join, actions),
            ]
            command_tick = int((game_diagnostics(host) or {}).get("currentTick", 0))
            empty_turns = wait_until(
                "explicit empty turns remain synchronized",
                lambda: require_matching_lockstep(host, join, command_tick + 3),
                timeout=WAIT_SECONDS,
            )
            evidence["emptyTurns"] = {
                "startTick": command_tick,
                "endTick": empty_turns[0].get("currentTick"),
                "stateHash": empty_turns[0].get("stateHash"),
            }
            _milestone(evidence, "ordered-input-and-empty-turns", host, join)

            key_chord(join, Keys.ENTER)
            join.find_element(By.ID, "canvas").send_keys("public relay hello")
            key_chord(join, Keys.ENTER)
            wait_until(
                "public chat delivered once",
                lambda: True if all(
                    int((game_diagnostics(peer) or {}).get("chatCount", 0)) == 1
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            key_chord(host, "f", Keys.ALT)
            audited_pointer(host_journey, actions, "host", "townCenter", button=0)
            wait_until(
                "public map signal delivered once",
                lambda: True if all(
                    int((game_diagnostics(peer) or {}).get("signalCount", 0)) == 1
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            _milestone(evidence, "chat-and-map-signal", host, join)

            prior_speed = int((game_diagnostics(host) or {}).get("gameSpeed", 0))
            key_chord(host, Keys.F8)
            wait_until(
                "committed speed control",
                lambda: speed if (
                    (speed := int((game_diagnostics(host) or {}).get("gameSpeed", prior_speed))) != prior_speed and
                    speed == int((game_diagnostics(join) or {}).get("gameSpeed", prior_speed))
                ) else None,
                timeout=WAIT_SECONDS,
            )
            key_chord(host, Keys.F7)
            paused = wait_until(
                "committed pause barrier",
                lambda: games if (
                    (games := [game_diagnostics(peer) or {} for peer in (host, join)]) and
                    all(bool(game.get("paused")) for game in games) and
                    games[0].get("currentTick") == games[1].get("currentTick")
                ) else None,
                timeout=WAIT_SECONDS,
            )
            pause_tick = int(paused[0].get("currentTick", -1))
            key_chord(host, Keys.F7)
            wait_until(
                "committed resume barrier",
                lambda: require_matching_lockstep(host, join, pause_tick + 1),
                timeout=WAIT_SECONDS,
            )
            key_chord(host, Keys.F8)
            wait_until(
                "committed slow speed for relay diagnostics",
                lambda: games if (
                    (games := [
                        game_diagnostics(peer) or {} for peer in (host, join)
                    ]) and
                    all(int(game.get("gameSpeed", 2)) == 0 for game in games)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            _milestone(evidence, "committed-controls", host, join)

            evidence["recovery"] = exercise_relay_chaos(host, join, active_relays)
            _milestone(evidence, "relay-loss-backfill-recovery", host, join)

            resign(host)
            terminal = wait_until(
                "synchronized resignation result",
                lambda: games if (
                    (games := require_matching_lockstep(host, join)) and
                    int(games[0].get("outcome", 0)) != 0 and
                    games[0].get("outcome") == games[1].get("outcome") and
                    bool(games[0].get("terminalStateHash")) and
                    games[0].get("terminalStateHash") == games[1].get("terminalStateHash") and
                    all(int(game.get("resultCount", 0)) == 2 and
                        bool(game.get("terminalResultAgreement")) for game in games)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            settled_tick = int(terminal[0].get("currentTick", -1))
            time.sleep(1.0)
            if any(
                int((game_diagnostics(peer) or {}).get("currentTick", -2)) != settled_tick
                for peer in (host, join)
            ):
                raise Failure("lockstep advanced after synchronized resignation")
            evidence["terminal"] = {
                "method": "host Ctrl+Shift+R production command",
                "tick": settled_tick,
                "outcome": terminal[0].get("outcome"),
                "stateHash": terminal[0].get("terminalStateHash"),
                "hostControllerState": terminal[0].get("blueControllerState"),
                "resultCount": terminal[0].get("resultCount"),
            }
            _milestone(evidence, "synchronized-resignation", host, join)

            # A matched checkpoint intentionally stops its match. Exercise it
            # in a fresh lobby so relay recovery and terminal agreement remain
            # reachable in the gameplay journey above.
            checkpoint_host_journey = launch(host, base_url, "host")
            # Navigation can briefly expose diagnostics from the terminating
            # match. Let the new host publish and then bind the second peer to
            # its exact reference; discovery was already accepted above.
            time.sleep(3.0)
            checkpoint_host = diagnostics(host) or {}
            if not _hex64(checkpoint_host.get("publicKey")) or \
                    not _hex64(checkpoint_host.get("matchId")) or \
                    checkpoint_host.get("matchId") == \
                        evidence["identityLobby"].get("matchId"):
                raise Failure("fresh checkpoint host identity is absent")
            require_quorum(host, "checkpoint host")
            checkpoint_relays = str(
                host.find_element(By.ID, "relays").get_attribute("value") or ""
            )
            checkpoint_reference = str(checkpoint_host.get("matchReference", ""))
            if not checkpoint_reference:
                raise Failure("fresh checkpoint host reference is absent")
            checkpoint_join_journey = launch(
                join, base_url, "join", checkpoint_reference
            )
            evidence["checkpointMatchReference"] = checkpoint_reference
            require_quorum(join, "checkpoint join")
            wait_until(
                "checkpoint lobby revision 2",
                lambda: True if all(
                    int((game_diagnostics(peer) or {}).get("lobbyRevision", 0)) >= 2
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            click_canvas_logical(host, 438, 516)
            click_canvas_logical(join, 438, 516)
            wait_until(
                "checkpoint lobby ready events",
                lambda: True if all(
                    bool((game_diagnostics(peer) or {}).get("blueReady")) and
                    bool((game_diagnostics(peer) or {}).get("redReady"))
                    for peer in (host, join)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            click_canvas_logical(host, 842, 516)
            wait_until(
                "checkpoint lobby lockstep start",
                lambda: require_matching_lockstep(host, join, 8),
                timeout=WAIT_SECONDS,
            )
            checkpoint_identity = validate_identity_lobby(
                diagnostics(host) or {}, diagnostics(join) or {}
            )
            if checkpoint_identity.get("matchId") == \
                    evidence["identityLobby"].get("matchId"):
                raise Failure("checkpoint journey reused terminal match identity")
            if checkpoint_relays != active_relays:
                raise Failure("checkpoint journey changed configured relay pool")
            key_chord(host, Keys.F6)
            checkpoint = wait_until(
                "matched public checkpoint digest",
                lambda: games if (
                    (games := require_matching_lockstep(host, join)) and
                    all(int(game.get("stateHashStatus", 0)) == 2 for game in games)
                ) else None,
                timeout=WAIT_SECONDS,
            )
            checkpoint_tick = int(checkpoint[0].get("currentTick", -1))
            time.sleep(1.0)
            if any(
                int((game_diagnostics(peer) or {}).get("currentTick", -2)) !=
                    checkpoint_tick
                for peer in (host, join)
            ):
                raise Failure("matched checkpoint did not stop lockstep")
            evidence["checkpoint"] = {
                "identity": checkpoint_identity,
                "tick": checkpoint_tick,
                "stateHash": checkpoint[0].get("stateHash"),
                "status": checkpoint[0].get("stateHashStatus"),
                "stopped": True,
                "hostJourney": checkpoint_host_journey.telemetry(),
                "joinJourney": checkpoint_join_journey.telemetry(),
            }
            _milestone(evidence, "separate-matched-checkpoint", host, join)
            evidence.update({
                "host": diagnostics(host) or {},
                "join": diagnostics(join) or {},
                "requests": list(requests),
                "hostConsole": host.get_log("browser"),
                "joinConsole": join.get_log("browser"),
            })
            if not private_material_absent({
                "host": evidence["host"], "join": evidence["join"],
                "hostConsole": evidence["hostConsole"],
                "joinConsole": evidence["joinConsole"],
            }):
                raise Failure("final diagnostics or console contain private signer material")
            return evidence
        except Exception as error:
            host_state = capture_failure_value("host diagnostics", lambda: diagnostics(host))
            join_state = capture_failure_value("join diagnostics", lambda: diagnostics(join))
            failure = {
                "error": f"{type(error).__name__}: {error}",
                "traceback": traceback.format_exc(),
                "completedEvidence": evidence,
                "host": host_state, "join": join_state,
                "requests": list(requests),
                "hostConsole": capture_failure_value("host console", lambda: host.get_log("browser")),
                "joinConsole": capture_failure_value("join console", lambda: join.get_log("browser")),
                "publishIntents": {
                    "host": capture_failure_value(
                        "host publish intents", lambda: publish_intent_probe(host)
                    ),
                    "join": capture_failure_value(
                        "join publish intents", lambda: publish_intent_probe(join)
                    ),
                },
                "infrastructureBlocker": relay_blocker_from_diagnostics(host_state, join_state),
            }
            atomic_write_json(artifact_dir / "first-failure.json", failure)
            raise
        finally:
            host.quit()
            join.quit()


def allocate_destination(
    artifact_root: Path, report_root: Path,
) -> tuple[Path, Path, str]:
    now = datetime.now(timezone.utc)
    run_id = f"{now.strftime('%Y%m%dT%H%M%SZ')}-{secrets.token_hex(6)}"
    artifact_dir = artifact_root / run_id
    artifact_dir.mkdir(parents=True)
    report_root.mkdir(parents=True, exist_ok=True)
    report = report_root / f"{now.strftime('%Y-%m-%d')}-NOSTR-MULTIPLAYER.md"
    if report.exists():
        report = report_root / f"{now.strftime('%Y-%m-%d')}-NOSTR-MULTIPLAYER-{run_id}.md"
    report.touch(exist_ok=False)
    return artifact_dir, report, run_id


def write_report(
    report: Path, artifact_dir: Path, status: str,
    evidence: dict[str, object], detail: str,
) -> None:
    identity = evidence.get("identityLobby", {})
    terminal = evidence.get("terminal", {})
    milestones = evidence.get("milestones", [])
    rows = "\n".join(
        f"| {item.get('name')} | PASS |"
        for item in milestones if isinstance(item, dict)
    ) or "| none completed | BLOCKED |"
    report.write_text(
        "# Public Nostr multiplayer audit\n\n"
        f"- Verdict: **{status}**\n"
        f"- Evidence: `{artifact_dir.relative_to(ROOT)}`\n"
        f"- Commit: `{subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()}`\n\n"
        "## Identity ledger\n\n"
        f"- Host public key: `{identity.get('hostPublicKey', 'unproved')}`\n"
        f"- Join public key: `{identity.get('joinPublicKey', 'unproved')}`\n"
        f"- Distinct: `{bool(identity and identity.get('hostPublicKey') != identity.get('joinPublicKey'))}`\n"
        f"- Private material absent: `{identity.get('privateMaterialAbsent', 'unproved')}`\n\n"
        "## Journey ledger\n\n| Milestone | Status |\n|---|---|\n"
        f"{rows}\n\n"
        "## Terminal\n\n"
        f"- Method: `{terminal.get('method', 'not reached')}`\n"
        f"- Tick: `{terminal.get('tick', 'unproved')}`\n"
        f"- State hash: `{terminal.get('stateHash', 'unproved')}`\n\n"
        "## Result\n\n"
        f"{detail}\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8892)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--audit-root", type=Path, default=ARTIFACT_ROOT)
    parser.add_argument("--report-root", type=Path, default=REPORT_ROOT)
    parser.add_argument("--browser-argument", action="append", default=[])
    arguments = parser.parse_args()
    artifact_dir, report, run_id = allocate_destination(
        arguments.audit_root, arguments.report_root
    )
    ledger = {
        "schemaVersion": 1,
        "status": "RUNNING",
        "runId": run_id,
        "startedUtc": datetime.now(timezone.utc).isoformat(),
        "artifactPath": str(artifact_dir.relative_to(ROOT)),
        "reportPath": str(report.relative_to(ROOT)),
        "sourceCommit": subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
            capture_output=True, text=True,
        ).stdout.strip(),
        "scope": "identity lobby commands checkpoint relay recovery resignation",
        "visualAcceptance": False,
        "privateKeysRetained": False,
    }
    atomic_write_json(artifact_dir / "run.json", ledger)
    write_report(report, artifact_dir, "RUNNING", {}, "Destinations allocated before launch.")
    try:
        probe = probe_relay_pool(list(DEFAULT_RELAYS))
        atomic_write_json(artifact_dir / "relay-probe.json", probe)
        if len(probe.get("selectedQuorum", [])) < 2:
            raise Failure("relay probe found fewer than two healthy configured relays")
        evidence = run_protocol(
            arguments.headed, arguments.port, artifact_dir,
            arguments.browser_argument,
        )
        atomic_write_json(artifact_dir / "evidence.json", evidence)
        write_jsonl(artifact_dir / "actions.jsonl", evidence.get("actions", []))
        ledger.update({
            "status": "PASS",
            "completedUtc": datetime.now(timezone.utc).isoformat(),
            "hostPublicKey": evidence["identityLobby"]["hostPublicKey"],
            "joinPublicKey": evidence["identityLobby"]["joinPublicKey"],
        })
        atomic_write_json(artifact_dir / "run.json", ledger)
        atomic_write_json(artifact_dir / "verdict.json", {
            "schemaVersion": 1, "status": "PASS",
            "visualAcceptance": False,
            "completedMilestones": [item["name"] for item in evidence["milestones"]],
        })
        write_report(
            report, artifact_dir, "PASS", evidence,
            "Bounded protocol acceptance passed without visual or full-match criteria.",
        )
        print(f"Nostr protocol acceptance passed: {artifact_dir}")
        return 0
    except Exception as error:
        failure_path = artifact_dir / "first-failure.json"
        failure = json.loads(failure_path.read_text(encoding="utf-8")) if failure_path.exists() else {
            "error": f"{type(error).__name__}: {error}",
            "traceback": traceback.format_exc(),
        }
        completed = failure.get("completedEvidence", {})
        status = "PARTIAL" if completed.get("milestones") else "BLOCKED"
        ledger.update({
            "status": status,
            "completedUtc": datetime.now(timezone.utc).isoformat(),
            "failure": failure.get("error"),
        })
        atomic_write_json(artifact_dir / "run.json", ledger)
        atomic_write_json(artifact_dir / "verdict.json", {
            "schemaVersion": 1, "status": status,
            "failure": failure.get("error"),
        })
        write_report(
            report, artifact_dir, status, completed,
            f"Primary failure: `{failure.get('error')}`",
        )
        print(f"Nostr protocol acceptance {status.lower()}: {artifact_dir}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

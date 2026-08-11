#!/usr/bin/env python3
"""Two-browser production-path smoke test over ordinary public Nostr relays."""

from __future__ import annotations

import argparse
import json
import time
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
    "wss://relay.damus.io,wss://nos.lol,wss://relay.primal.net"
)
WAIT_SECONDS = 180.0


def diagnostics(driver) -> dict[str, object] | None:
    value = driver.execute_script(
        "return Module.browserNostrDiagnostics "
        "? Module.browserNostrDiagnostics() : null"
    )
    return value if isinstance(value, dict) else None


def game_diagnostics(driver) -> dict[str, object] | None:
    value = diagnostics(driver)
    game = value.get("game") if value else None
    return game if isinstance(game, dict) else None


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


def click_canvas_logical(driver, x: float, y: float) -> None:
    canvas = driver.find_element(By.ID, "canvas")
    rect = canvas.rect
    mouse = PointerInput("mouse", "multiplayer UI")
    actions = ActionBuilder(driver, mouse=mouse)
    actions.pointer_action.move_to_location(
        round(rect["x"] + x * rect["width"] / 1280.0),
        round(rect["y"] + y * rect["height"] / 720.0),
    )
    actions.pointer_action.click()
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


def run(relays: str, headed: bool, port: int = 8888,
        checkpoint: bool = False) -> dict[str, object]:
    if not (DIST / "aoe_web.html").exists():
        raise Failure("packaged browser distribution is missing")
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    evidence: dict[str, object] = {"relays": relays.split(",")}
    host = make_driver("chrome", headed)
    join = make_driver("chrome", headed)
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

            recovery: dict[str, object] = {
                "before": {
                    "host": diagnostics(host),
                    "join": diagnostics(join),
                }
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
                "host": one_relay_loss[0],
                "join": one_relay_loss[1],
            }

            for driver in (host, join):
                set_relay_enabled(driver, 1, False)
            quorum_loss = wait_until(
                "quorum-loss suspension",
                lambda: matching_relay_state(
                    host, join, disabled=2, status=2, reason=5,
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
                "host": quorum_loss[0],
                "join": quorum_loss[1],
                "stableTicks": stopped_ticks,
            }

            for driver in (host, join):
                set_relay_enabled(driver, 1, True)
            recovered = wait_until(
                "relay EOSE backfill and lockstep recovery",
                lambda: matching_relay_state(
                    host, join, disabled=1, status=0, eose=2,
                    minimum_tick=max(stopped_ticks) + 1,
                ),
                timeout=WAIT_SECONDS,
            )
            recovery["recovered"] = {
                "host": recovered[0],
                "join": recovered[1],
            }
            for driver in (host, join):
                set_relay_enabled(driver, 0, True)
            wait_until(
                "all configured relays restored through EOSE",
                lambda: (
                    True
                    if all(
                        not (state := diagnostics(driver) or {}).get(
                            "disabledRelays"
                        ) and len(state.get("eoseRelays", [])) >= 3
                        for driver in (host, join)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
            recovery["allRestored"] = {
                "host": diagnostics(host),
                "join": diagnostics(join),
            }
            evidence["recovery"] = recovery

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
            host_journey.pointer("villager")
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
            host_journey.pointer("villager", button=2, logical_dx=50)
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
            }

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
            host.save_screenshot(str(ARTIFACTS / "signal-armed.png"))
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
                host.save_screenshot(str(ARTIFACTS / "checkpoint-host.png"))
                join.save_screenshot(str(ARTIFACTS / "checkpoint-join.png"))
                return evidence
            # Visible production control submits resignation through lockstep.
            click_canvas_logical(host, 1165, 388)
            terminal = wait_until(
                "agreed terminal result",
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
                        int(game.get("blueControllerState", -1)) == 1 and
                        int(game.get("redControllerState", -1)) == 0 and
                        int(game.get("resultCount", 0)) == 2 and
                        bool(game.get("terminalResultAgreement"))
                        for game in (host_game, join_game)
                    )
                    else None
                ),
                timeout=WAIT_SECONDS,
            )
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
                "requests": list(requests),
                "hostConsole": host.get_log("browser"),
                "joinConsole": join.get_log("browser"),
            })
            host.save_screenshot(str(ARTIFACTS / "host.png"))
            join.save_screenshot(str(ARTIFACTS / "join.png"))
            return evidence
        except Exception as error:
            failure = {
                "error": f"{type(error).__name__}: {error}",
                "completedEvidence": evidence,
                "relays": relays.split(","),
                "host": diagnostics(host),
                "join": diagnostics(join),
                "requests": list(requests),
                "hostConsole": host.get_log("browser"),
                "joinConsole": join.get_log("browser"),
            }
            (ARTIFACTS / "last-failure.json").write_text(
                json.dumps(failure, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            host.save_screenshot(str(ARTIFACTS / "last-failure-host.png"))
            join.save_screenshot(str(ARTIFACTS / "last-failure-join.png"))
            raise
        finally:
            if host_journey is not None:
                host_journey.evidence.clear()
            if join_journey is not None:
                join_journey.evidence.clear()
            host.quit()
            join.quit()


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
    )
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"Nostr multiplayer smoke passed: {arguments.evidence}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

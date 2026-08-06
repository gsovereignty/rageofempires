#!/usr/bin/env python3
"""Production-browser acceptance journey for the fixed WebAssembly spike."""

from __future__ import annotations

import argparse
import contextlib
import json
import threading
import time
from dataclasses import dataclass
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Callable

from selenium import webdriver
from selenium.common.exceptions import JavascriptException
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.actions.action_builder import ActionBuilder
from selenium.webdriver.common.actions.pointer_input import PointerInput
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys


ROOT = Path(__file__).resolve().parents[2]
DIST = ROOT / "build-web" / "dist"
ARTIFACTS = ROOT / "artifacts" / "browser-risk-spike"
PAGE = "aoe_web.html"
WAIT_SECONDS = 30.0
MAXIMUM_RESUME_SECONDS = 2.0
MAXIMUM_HEAP_BYTES = 256 * 1024 * 1024
MAXIMUM_SECOND_VICTORY_GROWTH = 16 * 1024 * 1024
MAXIMUM_RESTART_DELTA = 16 * 1024 * 1024


class Failure(RuntimeError):
    pass


class RequestLogHandler(SimpleHTTPRequestHandler):
    requests: list[dict[str, object]] = []

    def log_message(self, format: str, *args: object) -> None:
        return

    def send_response(self, code: int, message: str | None = None) -> None:
        self.requests.append({"path": self.path, "status": code})
        super().send_response(code, message)

    def copyfile(self, source, outputfile) -> None:
        try:
            super().copyfile(source, outputfile)
        except BrokenPipeError:
            pass


@contextlib.contextmanager
def static_server() -> tuple[str, list[dict[str, object]]]:
    RequestLogHandler.requests = []
    handler = lambda *args, **kwargs: RequestLogHandler(
        *args, directory=str(DIST), **kwargs
    )
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield (
            f"http://127.0.0.1:{server.server_address[1]}",
            RequestLogHandler.requests,
        )
    finally:
        server.shutdown()
        thread.join(timeout=5)


def make_driver(browser: str, headed: bool) -> webdriver.Remote:
    if browser == "chrome":
        options = webdriver.ChromeOptions()
        if not headed:
            options.add_argument("--headless=new")
        options.add_argument("--window-size=1280,900")
        options.add_argument("--autoplay-policy=user-gesture-required")
        options.set_capability("goog:loggingPrefs", {"browser": "ALL"})
        return webdriver.Chrome(options=options)
    if browser == "firefox":
        options = webdriver.FirefoxOptions()
        if not headed:
            options.add_argument("-headless")
        return webdriver.Firefox(options=options)
    if browser == "safari":
        if not headed:
            raise Failure("Safari acceptance requires a headed session")
        return webdriver.Safari()
    raise Failure(f"unsupported browser: {browser}")


def wait_until(
    description: str,
    predicate: Callable[[], object],
    timeout: float = WAIT_SECONDS,
) -> object:
    deadline = time.monotonic() + timeout
    last: object = None
    while time.monotonic() < deadline:
        try:
            last = predicate()
        except (JavascriptException, KeyError, TypeError):
            last = None
        if last:
            return last
        time.sleep(0.05)
    raise Failure(f"timed out waiting for {description}; last={last!r}")


@dataclass
class Journey:
    driver: webdriver.Remote
    base_url: str
    evidence: dict[str, object]

    def script(self, source: str) -> object:
        return self.driver.execute_script(source)

    def telemetry(self) -> dict[str, object]:
        value = self.script("return Module.browserTelemetry || null")
        if not isinstance(value, dict):
            raise Failure("browser telemetry is unavailable")
        return value

    def wait_telemetry(self, minimum_tick: int = 1) -> dict[str, object]:
        self.driver.set_script_timeout(WAIT_SECONDS + 5.0)
        value = self.driver.execute_async_script(
            """
            const minimumTick = arguments[0];
            const done = arguments[arguments.length - 1];
            const deadline = performance.now() + arguments[1];
            const poll = () => {
              const value = Module.browserTelemetry || null;
              if (value && Number(value.tick) >= minimumTick) {
                done(value);
              } else if (performance.now() >= deadline) {
                done(null);
              } else {
                setTimeout(poll, 20);
              }
            };
            poll();
            """,
            minimum_tick,
            WAIT_SECONDS * 1000,
        )
        if not isinstance(value, dict):
            raise Failure(f"timed out waiting for telemetry tick {minimum_tick}")
        return value

    def record(self, name: str, screenshot: bool = False) -> dict[str, object]:
        state = self.telemetry()
        if int(state["fallbackCount"]) != 0:
            raise Failure(f"runtime fallback at {name}: {state}")
        if int(state["wasmHeapBytes"]) > MAXIMUM_HEAP_BYTES:
            raise Failure(f"Wasm heap budget exceeded at {name}: {state}")
        checkpoint = {
            "telemetry": state,
            "audio": self.script(
                "return Module.browserAudioTelemetry || null"
            ),
            "lifecycle": self.script(
                "return Module.browserLifecycle || null"
            ),
            "display": self.script("return Module.browserDisplayMetrics()"),
            "persistence": self.script(
                "return {status: Module.persistenceSyncStatus || null, "
                "error: Module.persistenceSyncError || null}"
            ),
            "performance": self.performance_metrics(),
        }
        self.evidence.setdefault("checkpoints", {})[name] = checkpoint
        if screenshot:
            self.driver.save_screenshot(str(ARTIFACTS / f"{name}.png"))
        return state

    def performance_metrics(self) -> dict[str, object]:
        if not hasattr(self.driver, "execute_cdp_cmd"):
            return {}
        try:
            result = self.driver.execute_cdp_cmd(
                "Performance.getMetrics", {}
            )
        except Exception:
            return {}
        return {
            entry["name"]: entry["value"]
            for entry in result.get("metrics", [])
            if entry["name"] in {
                "JSHeapUsedSize",
                "JSHeapTotalSize",
                "Nodes",
                "Documents",
            }
        }

    def open(self, clear_storage: bool = False) -> None:
        self.driver.get(f"{self.base_url}/{PAGE}")
        wait_until(
            "storage initialization",
            lambda: self.script(
                "return Module.storageReady === true && "
                "!document.getElementById('start').hidden"
            ),
        )
        if clear_storage:
            self.driver.execute_async_script(
                """
                const done = arguments[0];
                try {
                  if (self.IDBFS && IDBFS.dbs) {
                    for (const database of Object.values(IDBFS.dbs)) {
                      database.close();
                    }
                    IDBFS.dbs = {};
                  }
                  indexedDB.databases().then(async databases => {
                    await Promise.all(databases.filter(value => value.name)
                      .map(value => new Promise(resolve => {
                        const request = indexedDB.deleteDatabase(value.name);
                        request.onsuccess = request.onerror =
                          request.onblocked = resolve;
                      })));
                    done(true);
                  }, error => done(String(error)));
                } catch (error) { done(String(error)); }
                """
            )
            self.driver.get(f"{self.base_url}/{PAGE}")
            wait_until(
                "clean storage initialization",
                lambda: self.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )

    def start(self) -> dict[str, object]:
        self.driver.find_element(By.ID, "start").click()
        # Let the newly registered Emscripten main-loop callback win one
        # animation frame before synchronous WebDriver polling begins.
        time.sleep(0.1)
        state = self.wait_telemetry()
        wait_until(
            "streaming music",
            lambda: self.script(
                "return Module.audioState && "
                "!Module.audioState.music.paused && "
                "Module.audioState.music.currentTime > 0"
            ),
        )
        return state

    def canvas(self):
        return self.driver.find_element(By.ID, "canvas")

    def target_page_point(
        self, name: str, logical_dx: float = 0, logical_dy: float = 0
    ) -> tuple[float, float]:
        state = self.telemetry()
        target = state["targets"][name]
        rect = self.canvas().rect
        x = rect["x"] + (
            float(target["x"]) + logical_dx
        ) * rect["width"] / float(state["logicalWidth"])
        y = rect["y"] + (
            float(target["y"]) + logical_dy
        ) * rect["height"] / float(state["logicalHeight"])
        return x, y

    def pointer(
        self,
        target: str,
        button: int = 0,
        logical_dx: float = 0,
        logical_dy: float = 0,
    ) -> None:
        x, y = self.target_page_point(target, logical_dx, logical_dy)
        mouse = PointerInput("mouse", "acceptance mouse")
        actions = ActionBuilder(self.driver, mouse=mouse)
        actions.pointer_action.move_to_location(round(x), round(y))
        actions.pointer_action.pointer_down(button=button)
        actions.pointer_action.pointer_up(button=button)
        actions.perform()

    def key(self, value: str) -> None:
        self.canvas().send_keys(value)

    def freeze(self, seconds: float, name: str) -> None:
        if not hasattr(self.driver, "execute_cdp_cmd"):
            self.evidence.setdefault("suspension", {})[name] = "unsupported"
            return
        before = self.telemetry()
        self.driver.execute_cdp_cmd(
            "Page.setWebLifecycleState", {"state": "frozen"}
        )
        time.sleep(seconds)
        resume_started = time.monotonic()
        self.driver.execute_cdp_cmd(
            "Page.setWebLifecycleState", {"state": "active"}
        )
        deadline = time.monotonic() + MAXIMUM_RESUME_SECONDS
        after = before
        while time.monotonic() < deadline:
            time.sleep(0.25)
            after = self.telemetry()
            if int(after["tick"]) > int(before["tick"]):
                break
        else:
            raise Failure(f"timed out waiting for {name} resume")
        result = {
            "before_tick": before["tick"],
            "after_tick": after["tick"],
            "resume_seconds": time.monotonic() - resume_started,
        }
        if int(after["tick"]) - int(before["tick"]) > 4:
            raise Failure(f"giant simulation catch-up after {name}: {result}")
        self.evidence.setdefault("suspension", {})[name] = result

    def play_to_victory(self, label: str) -> dict[str, object]:
        self.pointer("villager")
        wait_until(
            "villager selection",
            lambda: int(self.telemetry()["selectedUnit"]) != 0,
        )
        self.pointer("resource", button=2)
        wait_until(
            "gold gathering",
            lambda: int(self.telemetry()["resources"]["gold"]) >= 60,
        )
        self.record(f"{label}-gathering", screenshot=True)
        self.freeze(1.0, f"{label}-gathering")

        self.pointer("barracks")
        wait_until(
            "barracks selection",
            lambda: int(self.telemetry()["selectedBuilding"]) != 0,
        )
        self.key("m")
        wait_until(
            "militia training",
            lambda: int(self.telemetry()["blueMilitaryCount"]) >= 1,
        )
        self.key("9")
        wait_until(
            "man-at-arms research",
            lambda: bool(self.telemetry()["manAtArmsResearched"]),
        )
        self.record(f"{label}-research", screenshot=True)

        self.pointer("military")
        wait_until(
            "military pointer selection",
            lambda: int(self.telemetry()["selectedUnit"]) != 0,
        )
        before_move = self.telemetry()["targets"]["military"]
        self.pointer("resource", button=2, logical_dx=80)
        wait_until(
            "military pointer move",
            lambda: abs(
                float(self.telemetry()["targets"]["military"]["x"])
                - float(before_move["x"])
            ) > 20,
        )
        for _ in range(14):
            target_x = float(
                self.telemetry()["targets"]["enemyBuilding"]["x"]
            )
            logical_width = float(self.telemetry()["logicalWidth"])
            if 80 < target_x < logical_width - 80:
                break
            self.key(Keys.ARROW_RIGHT)
            time.sleep(0.05)
        wait_until(
            "enemy building inside canvas",
            lambda: 80
            < float(self.telemetry()["targets"]["enemyBuilding"]["x"])
            < float(self.telemetry()["logicalWidth"]) - 80,
        )
        initial_hit_points = int(self.telemetry()["enemyBuildingHitPoints"])
        self.pointer("enemyBuilding", button=2)
        wait_until(
            "combat start",
            lambda: int(self.telemetry()["enemyBuildingHitPoints"])
            < initial_hit_points,
            30.0,
        )
        self.record(f"{label}-combat", screenshot=True)
        self.freeze(1.0, f"{label}-combat")
        victory_deadline = time.monotonic() + 180.0
        outcome = self.telemetry()
        while time.monotonic() < victory_deadline:
            outcome = self.telemetry()
            if int(outcome["fallbackCount"]) != 0:
                raise Failure(f"runtime fallback during combat: {outcome}")
            if int(outcome["outcome"]) == 1:
                break
            if int(outcome["blueMilitaryCount"]) == 0:
                raise Failure(f"attacking military unit died: {outcome}")
            time.sleep(0.1)
        else:
            raise Failure(f"timed out waiting for blue victory: {outcome}")
        wait_until(
            "autosave synchronization",
            lambda: self.script(
                "return Module.persistenceSyncStatus === 'succeeded'"
            ),
        )
        self.record(f"{label}-victory", screenshot=True)
        return outcome  # type: ignore[return-value]

    def assert_audio(self) -> None:
        audio = self.script("return Module.browserAudioTelemetry")
        if audio["errors"]:
            raise Failure(f"browser audio errors: {audio['errors']}")
        if int(audio["liveMusicInstances"]) != 1:
            raise Failure(f"unexpected live music instances: {audio}")
        if int(audio["liveEffectInstances"]) != 1:
            raise Failure(f"unexpected live effect instances: {audio}")


def run(browser: str, headed: bool) -> dict[str, object]:
    if not (DIST / PAGE).is_file():
        raise Failure(f"browser distribution is missing: {DIST / PAGE}")
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    evidence: dict[str, object] = {"browser": browser}
    with static_server() as (base_url, requests):
        driver = make_driver(browser, headed)
        journey = Journey(driver, base_url, evidence)
        try:
            journey.open(clear_storage=True)
            journey.driver.save_screenshot(str(ARTIFACTS / "loading-ready.png"))
            journey.start()
            journey.record("loading-complete")
            first = journey.play_to_victory("first")
            first_heap = int(first["wasmHeapBytes"])
            journey.assert_audio()

            saved = {
                key: first[key]
                for key in (
                    "buildingCount",
                    "unitCount",
                    "outcome",
                    "resources",
                    "manAtArmsResearched",
                )
            }
            journey.driver.get(f"{base_url}/{PAGE}?restore=1")
            wait_until(
                "reload storage initialization",
                lambda: journey.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )
            journey.start()
            restored = journey.wait_telemetry()
            actual = {key: restored[key] for key in saved}
            if actual != saved:
                raise Failure(
                    f"restored autosave mismatch: expected={saved}, actual={actual}"
                )
            journey.record("restored-save", screenshot=True)

            journey.key("r")
            restart_deadline = time.monotonic() + WAIT_SECONDS
            restart: object = None
            while time.monotonic() < restart_deadline:
                time.sleep(0.25)
                value = journey.script(
                    "return Module.browserLifecycle || null"
                )
                if (
                    isinstance(value, dict)
                    and int(value["initializations"]) == 2
                    and int(value["shutdowns"]) == 1
                    and int(value["restarts"]) == 1
                ):
                    restart = value
                    break
            if not isinstance(restart, dict):
                raise Failure("timed out waiting for full application restart")
            fresh = wait_until(
                "fresh scenario after restart",
                lambda: (
                    state
                    if int((state := journey.telemetry())["outcome"]) == 0
                    and int(state["tick"]) < 20
                    else None
                ),
            )
            restart_heap = int(fresh["wasmHeapBytes"])
            if restart_heap - first_heap > MAXIMUM_RESTART_DELTA:
                raise Failure(
                    f"restart heap delta exceeded: {restart_heap - first_heap}"
                )
            evidence["restart_lifecycle"] = restart
            journey.record("restart-complete", screenshot=True)
            journey.assert_audio()

            second = journey.play_to_victory("second")
            second_heap = int(second["wasmHeapBytes"])
            if second_heap - first_heap > MAXIMUM_SECOND_VICTORY_GROWTH:
                raise Failure(
                    "second-victory heap growth exceeded: "
                    f"{second_heap - first_heap}"
                )
            journey.assert_audio()
            evidence["memory"] = {
                "first_victory": first_heap,
                "restart": restart_heap,
                "second_victory": second_heap,
            }
        except Exception as error:
            evidence["failure"] = str(error)
            try:
                evidence["failure_state"] = journey.script(
                    "return {body: document.body.innerText, "
                    "storageReady: Module.storageReady || false, "
                    "persistence: Module.persistenceSyncStatus || null, "
                    "syncError: Module.persistenceSyncError || null, "
                    "uncaught: Module.browserUncaughtErrors || [], "
                    "resources: performance.getEntriesByType('resource')"
                    ".map(value => ({name: value.name, "
                    "transferSize: value.transferSize, "
                    "duration: value.duration})), "
                    "runtimeCalled: Module.calledRun || false}"
                )
                driver.save_screenshot(str(ARTIFACTS / "failure.png"))
            except Exception as state_error:
                evidence["failure_state_error"] = str(state_error)
            raise
        finally:
            try:
                evidence["console"] = driver.get_log("browser")
            except Exception:
                evidence["console"] = []
            if "failure" in evidence:
                evidence["requests"] = list(requests)
                (ARTIFACTS / "failure-evidence.json").write_text(
                    json.dumps(evidence, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8",
                )
            driver.quit()

        evidence["requests"] = list(requests)
        failures = [request for request in requests if int(request["status"]) >= 400]
        if failures:
            raise Failure(f"static HTTP failures: {failures}")
        severe = [
            entry
            for entry in evidence["console"]
            if "Uncaught" in entry.get("message", "")
            or "WARNING:" in entry.get("message", "")
        ]
        if severe:
            raise Failure(f"browser console failures: {severe}")
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--browser", choices=("chrome", "firefox", "safari"), default="chrome"
    )
    parser.add_argument("--headed", action="store_true")
    parser.add_argument(
        "--evidence", type=Path, default=ARTIFACTS / "evidence.json"
    )
    arguments = parser.parse_args()
    evidence = run(arguments.browser, arguments.headed)
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"browser risk spike passed: {arguments.evidence}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

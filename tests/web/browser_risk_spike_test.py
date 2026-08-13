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
PAGE = "aoe_web.html?scenario=risk-spike"
SKIRMISH_PAGE = "aoe_web.html"
WAIT_SECONDS = 30.0
MAXIMUM_RESUME_SECONDS = 2.0
MAXIMUM_HEAP_BYTES = 256 * 1024 * 1024
MAXIMUM_SECOND_VICTORY_GROWTH = 16 * 1024 * 1024
MAXIMUM_RESTART_DELTA = 16 * 1024 * 1024
POINTER_EDGE_OFFSETS = (
    (0.0, 0.0),
    (-4.0, 0.0),
    (4.0, 0.0),
    (0.0, -4.0),
    (0.0, 4.0),
    (0.0, 0.0),
    (0.0, 0.0),
)


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
def static_server(port: int = 0) -> tuple[str, list[dict[str, object]]]:
    RequestLogHandler.requests = []
    handler = lambda *args, **kwargs: RequestLogHandler(
        *args, directory=str(DIST), **kwargs
    )
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
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


def make_driver(
    browser: str, headed: bool,
    browser_arguments: list[str] | None = None,
) -> webdriver.Remote:
    if browser == "chrome":
        options = webdriver.ChromeOptions()
        if not headed:
            options.add_argument("--headless=new")
        options.add_argument("--window-size=1280,900")
        options.add_argument("--autoplay-policy=user-gesture-required")
        for argument in browser_arguments or []:
            options.add_argument(argument)
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
        before = self.telemetry()
        if hasattr(self.driver, "execute_cdp_cmd"):
            self.driver.execute_cdp_cmd(
                "Page.setWebLifecycleState", {"state": "frozen"}
            )
            time.sleep(seconds)
            resume_started = time.monotonic()
            self.driver.execute_cdp_cmd(
                "Page.setWebLifecycleState", {"state": "active"}
            )
        else:
            original = self.driver.current_window_handle
            self.driver.switch_to.new_window("tab")
            try:
                time.sleep(seconds)
            finally:
                self.driver.close()
                self.driver.switch_to.window(original)
            resume_started = time.monotonic()
        resumed = self.telemetry()
        deadline = time.monotonic() + MAXIMUM_RESUME_SECONDS
        after = resumed
        while time.monotonic() < deadline:
            time.sleep(0.01)
            after = self.telemetry()
            if int(after["tick"]) > int(resumed["tick"]):
                break
        else:
            raise Failure(f"timed out waiting for {name} resume")
        result = {
            "before_tick": before["tick"],
            "resume_tick": resumed["tick"],
            "after_tick": after["tick"],
            "resume_seconds": time.monotonic() - resume_started,
        }
        if int(after["tick"]) - int(resumed["tick"]) > 4:
            raise Failure(f"giant simulation catch-up after {name}: {result}")
        self.evidence.setdefault("suspension", {})[name] = result

    def play_to_victory(self, label: str) -> dict[str, object]:
        before_town_center = self.telemetry()
        self.pointer("townCenter")
        wait_until(
            "town center selection",
            lambda: int(self.telemetry()["selectedBuilding"]) != 0,
        )
        wait_until(
            "town center selection keeps simulation running",
            lambda: int(self.telemetry()["tick"])
            > int(before_town_center["tick"]),
        )
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

    def assert_audio(self, require_effect: bool = False) -> None:
        audio = self.script("return Module.browserAudioTelemetry")
        streaming = self.script(
            "return Module.audioState && "
            "Module.audioState.music instanceof HTMLMediaElement && "
            "Module.audioState.music.preload === 'metadata'"
        )
        if not streaming:
            raise Failure("music is not using metadata-only media streaming")
        if audio["errors"]:
            raise Failure(f"browser audio errors: {audio['errors']}")
        if int(audio["liveMusicInstances"]) != 1:
            raise Failure(f"unexpected live music instances: {audio}")
        if int(audio["liveEffectInstances"]) > 32:
            raise Failure(f"too many live effect instances: {audio}")
        if int(audio["musicPlayAttempts"]) > int(audio["starts"]) * 4:
            raise Failure(f"duplicated music playback attempts: {audio}")
        if require_effect and int(audio["effectPlayAttempts"]) == 0:
            raise Failure(f"no mapped production effect played: {audio}")

    def prepare_pointer_matrix(self) -> None:
        self.pointer("villager")
        self.pointer("resource", button=2)
        wait_until(
            "pointer-matrix gold",
            lambda: int(self.telemetry()["resources"]["gold"]) >= 60,
        )
        villager_before = float(
            self.telemetry()["targets"]["villager"]["x"]
        )
        self.pointer("resource", button=2, logical_dx=80)
        wait_until(
            "pointer-matrix villager parking",
            lambda: abs(
                float(self.telemetry()["targets"]["villager"]["x"])
                - villager_before
            )
            > 20,
        )
        self.pointer("barracks")
        self.key("m")
        wait_until(
            "pointer-matrix military",
            lambda: int(self.telemetry()["blueMilitaryCount"]) >= 1,
        )
        time.sleep(1.0)

    def pointer_case(
        self, name: str, offset: tuple[float, float]
    ) -> dict[str, object]:
        dx, dy = offset
        self.driver.save_screenshot(str(ARTIFACTS / f"pointer-{name}-pre.png"))
        self.pointer("villager", logical_dx=dx, logical_dy=dy)
        villager = wait_until(
            f"villager pointer in {name}",
            lambda: (
                value
                if int((value := self.telemetry())["selectedUnit"]) != 0
                else None
            ),
        )

        gold_before = int(villager["resources"]["gold"])
        self.pointer("resource", button=2, logical_dx=dx, logical_dy=dy)
        wait_until(
            f"resource target in {name}",
            lambda: int(self.telemetry()["resources"]["gold"])
            > gold_before
            and int(self.telemetry()["resources"]["gold"]) >= 60,
        )

        self.pointer("barracks")
        self.key("m")
        wait_until(
            f"military training in {name}",
            lambda: int(self.telemetry()["blueMilitaryCount"]) >= 1,
        )
        time.sleep(1.0)
        self.pointer("barracks")
        self.pointer("military", logical_dx=dx, logical_dy=dy)
        military = wait_until(
            f"military pointer in {name}",
            lambda: (
                value
                if int((value := self.telemetry())["selectedUnit"]) != 0
                else None
            ),
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
        hit_points = int(self.telemetry()["enemyBuildingHitPoints"])
        self.pointer(
            "enemyBuilding", button=2, logical_dx=dx, logical_dy=dy
        )
        wait_until(
            f"enemy target in {name}",
            lambda: int(self.telemetry()["enemyBuildingHitPoints"])
            < hit_points,
        )
        self.pointer("resource", button=2)
        state = self.record(f"pointer-{name}", screenshot=True)
        return {
            "offset": {"x": dx, "y": dy},
            "display": self.script(
                "return {...Module.browserDisplayMetrics(), "
                "visualViewportScale: window.visualViewport?.scale || 1}"
            ),
            "logical": {
                "width": state["logicalWidth"],
                "height": state["logicalHeight"],
            },
            "targets": state["targets"],
        }


def run(browser: str, headed: bool) -> dict[str, object]:
    distribution = DIST / "aoe_web.html"
    if not distribution.is_file():
        raise Failure(f"browser distribution is missing: {distribution}")
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
            journey.assert_audio(require_effect=True)

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
            journey.driver.get(f"{base_url}/{PAGE}&restore=1")
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
            journey.assert_audio(require_effect=True)
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
        effect_paths = {
            str(request["path"])
            for request in requests
            if str(request["path"]).startswith(
                "/game_data/Sound/effects/"
            )
        }
        if len(effect_paths) < 2:
            raise Failure(f"gameplay effects did not resolve distinctly: {effect_paths}")
        if any("Food" in str(request["path"]) for request in requests):
            raise Failure("browser requested obsolete Food, please codec probe")
        severe = [
            entry
            for entry in evidence["console"]
            if "Uncaught" in entry.get("message", "")
            or "WARNING:" in entry.get("message", "")
        ]
        if severe:
            raise Failure(f"browser console failures: {severe}")
        return evidence


def run_display_matrix(browser: str, headed: bool) -> dict[str, object]:
    evidence: dict[str, object] = {"browser": browser, "cases": {}}
    with static_server() as (base_url, requests):
        driver = make_driver(browser, headed)
        journey = Journey(driver, base_url, evidence)
        try:
            def metrics(width: int, height: int, dpr: float) -> None:
                if not hasattr(driver, "execute_cdp_cmd"):
                    raise Failure("display emulation requires Chrome CDP")
                driver.execute_cdp_cmd(
                    "Emulation.setDeviceMetricsOverride",
                    {
                        "width": width,
                        "height": height,
                        "deviceScaleFactor": dpr,
                        "mobile": False,
                    },
                )
                time.sleep(0.5)

            def clear_metrics() -> None:
                if hasattr(driver, "execute_cdp_cmd"):
                    driver.execute_cdp_cmd(
                        "Emulation.clearDeviceMetricsOverride", {}
                    )
                    driver.execute_cdp_cmd(
                        "Emulation.setPageScaleFactor",
                        {"pageScaleFactor": 1},
                    )

            cases: list[tuple[str, Callable[[], None]]] = [
                ("dpr1", lambda: None),
                ("dpr2", lambda: metrics(1280, 900, 2)),
                (
                    "zoom125",
                    lambda: (
                        metrics(1280, 900, 1),
                        driver.execute_cdp_cmd(
                            "Emulation.setPageScaleFactor",
                            {"pageScaleFactor": 1.25},
                        ),
                    ),
                ),
                (
                    "resize",
                    lambda: (clear_metrics(), driver.set_window_size(1100, 760)),
                ),
                (
                    "fullscreen",
                    lambda: driver.find_element(By.ID, "fullscreen").click(),
                ),
                (
                    "fullscreen-exit",
                    lambda: (
                        driver.find_element(By.ID, "fullscreen").click(),
                        time.sleep(0.5),
                        driver.find_element(By.ID, "fullscreen").click(),
                    ),
                ),
                (
                    "letterbox",
                    lambda: (clear_metrics(), driver.set_window_size(1000, 1000)),
                ),
            ]
            for index, (name, apply_case) in enumerate(cases):
                if index:
                    journey.open()
                else:
                    journey.open(clear_storage=True)
                journey.start()
                apply_case()
                time.sleep(0.5)
                evidence["cases"][name] = journey.pointer_case(
                    name, POINTER_EDGE_OFFSETS[index]
                )
        finally:
            try:
                evidence["console"] = driver.get_log("browser")
            except Exception:
                evidence["console"] = []
            evidence["requests"] = list(requests)
            driver.quit()
    output = ARTIFACTS / "pointer-display-evidence.json"
    output.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return evidence


def run_skirmish_smoke(browser: str, headed: bool) -> dict[str, object]:
    evidence: dict[str, object] = {}
    with static_server() as (base_url, requests):
        driver = make_driver(browser, headed)
        journey = Journey(driver, base_url, evidence)
        try:
            driver.get(f"{base_url}/{SKIRMISH_PAGE}")
            wait_until(
                "skirmish storage initialization",
                lambda: journey.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )
            driver.find_element(By.ID, "start").click()
            initial = journey.wait_telemetry()
            if int(initial["unitCount"]) != 16:
                raise Failure(f"unexpected skirmish unit roster: {initial}")
            if int(initial["buildingCount"]) != 8:
                raise Failure(f"unexpected skirmish bases: {initial}")
            if int(initial["fallbackCount"]) != 0:
                raise Failure(f"skirmish render fallback: {initial}")
            final = wait_until(
                "computer AI unit production",
                lambda: (
                    state
                    if int((state := journey.telemetry())["unitCount"])
                    > int(initial["unitCount"])
                    else None
                ),
            )
            if int(final["fallbackCount"]) != 0:
                raise Failure(f"skirmish render fallback after AI turn: {final}")
            evidence["initial"] = initial
            evidence["after_computer_turn"] = final
            evidence["requests"] = list(requests)
            driver.save_screenshot(str(ARTIFACTS / "skirmish-smoke.png"))
        finally:
            try:
                evidence["console"] = driver.get_log("browser")
            except Exception:
                evidence["console"] = []
            driver.quit()
    return evidence


def run_persistence_checks(browser: str, headed: bool) -> dict[str, object]:
    if browser != "chrome":
        raise Failure("persistence fault injection currently requires Chrome CDP")
    evidence: dict[str, object] = {"browser": browser}
    with static_server() as (base_url, requests):
        driver = make_driver(browser, headed)
        journey = Journey(driver, base_url, evidence)
        def toggle_minimap_setting() -> None:
            journey.canvas().click()
            journey.canvas().send_keys(Keys.ESCAPE)
            time.sleep(0.1)
            journey.canvas().send_keys("n")
            time.sleep(0.1)
            journey.canvas().send_keys("s")

        delay_script = driver.execute_cdp_cmd(
            "Page.addScriptToEvaluateOnNewDocument",
            {
                "source": """
                window.Module = {preRun: [function () {
                  const realSync = FS.syncfs.bind(FS);
                  FS.syncfs = function (populate, callback) {
                    if (populate) {
                      setTimeout(() => realSync(populate, callback), 1000);
                    } else {
                      realSync(populate, callback);
                    }
                  };
                }]};
                """
            },
        )["identifier"]
        try:
            started = time.monotonic()
            driver.get(f"{base_url}/{PAGE}")
            wait_until(
                "delayed sync begins",
                lambda: journey.script(
                    "return typeof Module !== 'undefined' && "
                    "Module.calledRun !== true && "
                    "document.getElementById('start').hidden"
                ),
            )
            evidence["start_hidden_during_initial_sync"] = True
            wait_until(
                "delayed initial sync completes",
                lambda: journey.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )
            delay_seconds = time.monotonic() - started
            if delay_seconds < 0.75:
                raise Failure(f"initial sync delay not observed: {delay_seconds}")
            evidence["initial_sync_seconds"] = delay_seconds
            journey.start()

            toggle_minimap_setting()
            wait_until(
                "settings synchronization",
                lambda: journey.script(
                    "return Module.persistenceSyncStatus === 'succeeded'"
                ),
            )
            settings_before = journey.script(
                "return FS.readFile('/user/settings/"
                "reconstruction-settings.txt', {encoding: 'utf8'})"
            )
            if "minimap" not in str(settings_before).lower():
                raise Failure("production minimap setting was not serialized")

            driver.get(f"{base_url}/{PAGE}&settings-restore=1")
            wait_until(
                "settings reload sync",
                lambda: journey.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )
            settings_after = journey.script(
                "return FS.readFile('/user/settings/"
                "reconstruction-settings.txt', {encoding: 'utf8'})"
            )
            if settings_after != settings_before:
                raise Failure("settings file changed across IndexedDB reload")
            evidence["setting_survived_reload"] = True
            evidence["settings_bytes"] = len(str(settings_after).encode())

            driver.execute_cdp_cmd(
                "Page.removeScriptToEvaluateOnNewDocument",
                {"identifier": delay_script},
            )
            driver.execute_cdp_cmd(
                "Page.addScriptToEvaluateOnNewDocument",
                {
                    "source": """
                    window.Module = {preRun: [function () {
                      const realSync = FS.syncfs.bind(FS);
                      FS.syncfs = function (populate, callback) {
                        if (populate) realSync(populate, callback);
                        else setTimeout(() => callback(
                          new Error('forced browser test storage failure')
                        ), 0);
                      };
                    }]};
                    """
                },
            )
            driver.get(f"{base_url}/{PAGE}&forced-sync-failure=1")
            wait_until(
                "failure-injection startup",
                lambda: journey.script(
                    "return Module.storageReady === true && "
                    "!document.getElementById('start').hidden"
                ),
            )
            journey.start()
            toggle_minimap_setting()
            wait_until(
                "forced settings sync failure",
                lambda: journey.script(
                    "return Module.persistenceSyncStatus === 'failed'"
                ),
            )
            failure = journey.script(
                "return {status: Module.persistenceSyncStatus, "
                "error: Module.persistenceSyncError}"
            )
            if failure["status"] == "succeeded":
                raise Failure("forced storage failure reported success")
            evidence["forced_failure"] = failure
        finally:
            evidence["requests"] = list(requests)
            try:
                evidence["console"] = driver.get_log("browser")
            except Exception:
                evidence["console"] = []
            driver.quit()
    output = ARTIFACTS / "persistence-evidence.json"
    output.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--browser", choices=("chrome", "firefox", "safari"), default="chrome"
    )
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--display-matrix", action="store_true")
    parser.add_argument("--persistence-checks", action="store_true")
    parser.add_argument("--skirmish-smoke", action="store_true")
    parser.add_argument(
        "--evidence", type=Path, default=ARTIFACTS / "evidence.json"
    )
    arguments = parser.parse_args()
    if arguments.skirmish_smoke:
        evidence = run_skirmish_smoke(arguments.browser, arguments.headed)
    elif arguments.display_matrix:
        evidence = run_display_matrix(arguments.browser, arguments.headed)
    elif arguments.persistence_checks:
        evidence = run_persistence_checks(arguments.browser, arguments.headed)
    else:
        evidence = run(arguments.browser, arguments.headed)
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"browser acceptance passed: {arguments.evidence}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

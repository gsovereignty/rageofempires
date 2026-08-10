# Browser risk spike

This directory contains the static browser shell. Its default launch is a
48x32 single-player skirmish against the production computer AI, with normal
fog, two bases, villagers, livestock, military production, and terminal
victory/defeat objectives. Browser support is disabled unless both Emscripten and
`AOE_BUILD_WEB=ON` are selected.

The original fixed risk-spike fixture remains available for automated
acceptance at `aoe_web.html?scenario=risk-spike`.

Run default skirmish smoke with:

```sh
python3 tests/web/browser_risk_spike_test.py --skirmish-smoke
```

Pinned dependencies:

- Emscripten SDK: `4.0.10` (`web/emscripten-version.txt`)
- SDL3: `SDL-3.4.12-release-3.4.12`, from the tracked
  `third_party/SDL-3.4.12-f87239e.tar.gz` archive with its SHA-256 checked by
  CMake

Bootstrap installs only the recorded tag and verifies emsdk commit
`62a853cd3b3134398ce85cde8bb5cbb2ef0194cb`:

```sh
./web/bootstrap_emsdk.sh
source build-web/emsdk/emsdk_env.sh
```

Then configure only in `build-web/`:

```sh
emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DAOE_BUILD_WEB=ON \
  -DAOE_ENABLE_MPG123=OFF
cmake --build build-web --target aoe_web
cmake --build build-web --target web_risk_spike
```

`web_risk_spike` runs the deterministic asset-pack checks, complete Chrome
journey, pointer/display matrix, and persistence fault checks against the
production bundle. Cross-browser journey commands are:

```sh
MOZ_HEADLESS=1 python3 tests/web/browser_risk_spike_test.py \
  --browser firefox \
  --evidence artifacts/browser-risk-spike/evidence-firefox.json
python3 tests/web/browser_risk_spike_test.py \
  --browser safari --headed \
  --evidence artifacts/browser-risk-spike/evidence-safari.json
```

Safari requires **Allow remote automation** in Safari's Developer settings.
The build and tests remain frontend-only: they serve static files locally and
do not provide or exercise an application server.

Generated browser files belong only in `build-web/dist/`; they are not native
runtime or package inputs. Serve that directory through static HTTP rather
than opening its HTML through `file://`.

## Agent-readable console capture

Capture production-page console output without Selenium or Playwright:

```sh
python3 tools/capture_browser_console.py
```

The collector serves `build-web/dist`, launches an isolated headless Chrome,
activates the production Start control, and writes console calls, JavaScript
exceptions, page failure state, and Chrome diagnostics to
`artifacts/browser-console/console.json`. Use `--seconds`, `--chrome`,
`--dist`, or `--output` when defaults do not fit the local environment.

For failures in a real gameplay session, reproduce the problem and select
**Download diagnostics**. The resulting `aoe-browser-diagnostics-*.json` file
contains a bounded rolling log of console errors and warnings, uncaught errors,
runtime failure reports, display/resize/fullscreen history, telemetry, and
loaded resources. Give that file to the agent so it can inspect evidence from
the affected session.

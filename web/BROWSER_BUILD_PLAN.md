# Browser Risk Spike Plan

## Goal

Build one minimal, production-path browser playthrough that validates every
known browser-port risk before broader browser work begins.

This plan delivers only a technical risk spike. It does not deliver a general
browser edition.

Native targets, behavior, resources, packaging, and default build remain
unchanged. Browser support is a separate, disabled-by-default Emscripten target.

## Frontend-only deployment contract

The browser spike is entirely frontend-only. The production output is a set of
static HTML, JavaScript, WebAssembly, data, CSS, and media files that may be
served by any ordinary static HTTP host. It must not require or implement an
application server, API server, server-side rendering, database service,
WebSocket service, multiplayer relay, or other runtime backend.

All simulation, AI, rendering, input handling, save serialization, and restore
logic execute locally in the browser. Settings and the single autosave persist
locally through IndexedDB. Runtime network requests are limited to fetching the
static production files, including the directly streamed MP3; after those
files are delivered, no server computation or mutable server state may be
required. Browser automation may launch a local static file server solely to
provide HTTP delivery and must prove that no dynamic endpoint is used.

## Fixed product slice

The browser exposes exactly this flow:

```text
Loading
  Start Risk Scenario
    Play fixed scenario
      Victory or Defeat
        Restart Scenario
```

No other menu or game mode is reachable.

### Fixed scenario

Add one tracked scenario fixture dedicated to this spike. It must contain only:

- One human player.
- One AI opponent.
- One explored, fixed-size map with no random generation.
- Human starting units: one villager and one town center.
- Human starting resources sufficient only for the required flow.
- One nearby gatherable resource node.
- One trainable military unit type.
- One reachable technology affecting that military unit.
- One enemy military unit and one enemy building.
- One deterministic AI script that advances, then attacks.
- One victory condition: destroy the enemy building.
- One defeat condition: lose the human town center.
- One short MP3 effect and one long MP3 music track already licensed and
  tracked within reconstruction inputs.

Scenario settings, civilization IDs, entity IDs, technology ID, starting
resources, positions, AI timing, and victory condition must be recorded beside
the fixture. No runtime choice is permitted.

### Required player journey

The production browser build must support this exact journey:

1. Load the page from static HTTP hosting.
2. Show loading state while the virtual filesystem and IndexedDB initialize.
3. Show `Start Risk Scenario` only after initialization succeeds.
4. Start from an intentional pointer gesture and unlock browser audio.
5. Play the long MP3 music track by streaming, not whole-track decode.
6. Select the villager with a real canvas pointer event.
7. Command the villager to gather from the resource node.
8. Accumulate enough resources to train the single military unit.
9. Train that unit through the normal production UI.
10. Research the single technology through the normal research UI.
11. Observe the deterministic AI advance and attack.
12. Select and move the military unit with real canvas input.
13. Destroy the enemy building and reach the normal victory state.
14. Save one autosave and wait for confirmed IndexedDB synchronization.
15. Reload the page and restore that autosave through normal startup.
16. Restart the scenario through the victory screen.
17. Confirm music and effects have only one active playback instance.
18. Confirm memory returns to the defined restart tolerance.

Automation may drive real browser input, but acceptance must use normal product
inputs and the production build. Test-only runtime branches, forced audit modes,
mocks, alternate renderers, or injected simulation commands do not satisfy this
journey.

## Risks validated

The slice must prove all seven risks below. Passing only compilation, unit tests,
or first-frame rendering is insufficient.

### 1. SDL3 and Emscripten compatibility

Pin one Emscripten SDK version and one SDL3 revision. Record both in the browser
build documentation and CI/bootstrap configuration.

The production browser target must prove:

- SDL initialization and canvas creation.
- WebGL 2 renderer creation.
- Texture creation and destruction.
- `SDL_RenderGeometry`.
- Clipping and render targets.
- Text rendering.
- Keyboard, pointer, wheel, and right-click events used by the journey.
- Fullscreen entry from a user gesture and fullscreen exit.
- Clean browser-target compilation with no native TCP transport.

No unpinned dependency download or silent renderer substitution is allowed.

### 2. Non-blocking application lifecycle

Extract reusable lifecycle operations from the native blocking loop:

```cpp
class SdlApp {
public:
    void initialize();
    bool frame();
    void shutdown();
    int run();
};
```

Native `run()` retains native delay and ordering. Browser entry registers
`frame()` with the Emscripten main loop.

Prove production browser behavior for:

- Initialization once per scenario run.
- Repeated event, simulation, and render frames without blocking the page.
- Victory transition.
- Shutdown during restart.
- Second initialization after restart.
- No duplicated callbacks after restart.
- Exception and failure reporting without a hung loading screen.

### 3. Minimal asset closure

Create a deterministic asset-pack generator for only the fixed scenario.

It must traverse and include:

- Scenario and AI data.
- Two civilizations and referenced entities.
- Villager, town center, military unit, enemy unit, and enemy building data.
- Gather, train, research, attack, death, and victory dependencies.
- Selected technology and its upgrade dependencies.
- Used terrain, projectiles, effects, and animations.
- Required UI textures, icons, cursor, fonts, and localization strings.
- Required short effect and long music MP3.

Output only under `build-web/web-assets/`:

```text
build-web/web-assets/
├── resources/
├── game_data/
└── web_asset_manifest.json
```

Every manifest record must contain source-relative path, SHA-256 hash, byte
size, inclusion reason, and dependency parent. Missing dependencies fail the
build. Two clean generator runs must produce byte-identical manifests and asset
sets.

During the required journey, any missing-file request, placeholder asset,
fallback asset, decode failure, or HTTP 404 fails the spike.

### 4. Browser memory use

Instrument production build memory at these checkpoints:

1. Loading complete.
2. Scenario start.
3. Music playing.
4. First combat.
5. Victory.
6. Restart complete.
7. Second victory.

Record WASM heap size and browser process metrics available to the test runner.
Define numeric budgets before acceptance:

- Maximum packaged asset bytes.
- Maximum WASM heap at any checkpoint.
- Maximum heap growth between first and second victory.
- Maximum live audio instances after restart.

Long music must stream. Short effect may decode and cache once. Second victory
must not show unbounded heap growth or duplicate audio playback.

### 5. Pointer and display conversion

SDL logical game coordinates remain authoritative. Browser shell must account
for CSS canvas size, backing size, device pixel ratio, SDL output size,
letterboxing, resize, zoom, and fullscreen.

Repeat villager selection, resource targeting, military-unit selection, and
enemy-building targeting under every required display case:

- Device pixel ratio 1 at 100% browser zoom.
- Device pixel ratio 2 at 100% browser zoom.
- Device pixel ratio 1 at 125% browser zoom.
- Window resize while scenario is active.
- Fullscreen entered from user gesture.
- Return from fullscreen.
- One letterboxed aspect ratio.

Each pointer action must hit the visible target at its rendered center and near
each target edge. Any systematic offset, resolution-dependent target change, or
wrong minimap/world mapping fails the spike.

### 6. IndexedDB synchronization

Mount browser persistence at:

```text
/user/settings
/user/autosave
```

Startup order is mandatory:

1. Mount storage.
2. Await initial synchronization.
3. Read settings and autosave metadata.
4. Enable start action.

Save order is mandatory:

1. Write through existing C++ file API.
2. Request IndexedDB synchronization.
3. Await completion.
4. Report save success.

Prove:

- One setting survives full page reload.
- One autosave survives full page reload.
- Restored scenario state matches saved state.
- Delayed initial sync cannot be mistaken for missing data.
- Forced storage failure reports failure and never reports success.

Failure injection belongs in browser test infrastructure, not game runtime.

### 7. Tab suspension and timing

During active gathering, hide the page long enough for browser throttling, then
restore it. Repeat during combat.

On restore, prove:

- No giant simulation catch-up step.
- No permanent simulation stall.
- No burst of duplicated commands or audio.
- Input resumes.
- Rendering resumes.
- AI continues from valid state.
- Victory remains reachable.

Define maximum accepted simulation delta and resume latency in test constants.
Production timing code must clamp or otherwise handle browser suspension through
shared timing policy or a narrow platform adapter, not scattered browser guards.

## Implementation boundary

Expected new or changed areas:

```text
include/aoe/
├── application_loop.hpp
├── runtime_paths.hpp
└── audio_system.hpp

src/
├── sdl_app.cpp
├── application_loop.cpp
├── runtime_paths_native.cpp
├── runtime_paths_web.cpp
├── audio_system.cpp
├── audio_system_web.cpp
└── web_main.cpp

web/
├── BROWSER_BUILD_PLAN.md
├── shell.html
├── browser_runtime.js
├── styles.css
└── README.md

cmake/
└── BrowserBuild.cmake

tools/
└── build_web_asset_pack.py

tests/web/
└── browser_risk_spike_test.*
```

Exact names may change after code inspection. Platform implementations must be
selected through target source lists. Shared simulation and game rules must not
gain browser-specific branches.

## Build contract

Add disabled option:

```cmake
option(AOE_BUILD_WEB "Build browser WebAssembly risk spike" OFF)

if(EMSCRIPTEN AND AOE_BUILD_WEB)
    include(cmake/BrowserBuild.cmake)
endif()
```

Configure and build only in `build-web/`:

```sh
emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DAOE_BUILD_WEB=ON \
  -DAOE_ENABLE_MPG123=OFF
cmake --build build-web --target aoe_web
cmake --build build-web --target web_risk_spike
```

Initial Emscripten settings:

```text
ALLOW_MEMORY_GROWTH=1
FORCE_FILESYSTEM=1
MIN_WEBGL_VERSION=2
MAX_WEBGL_VERSION=2
EXIT_RUNTIME=0
```

Package assets at `/resources` and `/game_data`. Generate distribution only
under `build-web/dist/`. Browser build must not read any parent or sibling
workspace directory.

Native build must require no Emscripten installation and retain existing target
names, target graph, default options, TCP multiplayer, resource lookup, audio
backends, save paths, and macOS packaging.

## Native macOS non-regression contract

Browser work must not break, weaken, replace, or conditionally bypass the native
macOS product. A browser result is unacceptable if any native macOS gate fails.

### Protected macOS invariants

Every implementation and review must preserve all of these facts:

- Running plain `make` on macOS still configures and builds the native product.
- `AOE_BUILD_WEB` defaults to `OFF` and plain `make` never invokes Emscripten,
  Node, a browser, or browser asset generation.
- Existing native targets keep their names, output names, and meanings,
  including `aoe_core`, `aoe_reconstruction`, and `aoe_reconstruction_app`.
- `build/AoE Archaeology.app` remains the Finder-launchable development bundle.
- Native macOS continues using the pinned vendored SDL3 build.
- Native macOS remains Universal 2: `arm64` plus `x86_64`.
- Native deployment target remains macOS 11.0.
- Native executable entry point remains `src/main.cpp`; `src/web_main.cpp` is
  compiled only into `aoe_web`.
- Native `SdlApp::run()` remains blocking and retains its existing event,
  simulation, render, delay, exception, shutdown, and return-value behavior.
- `SDL_Delay(8)` remains native-only behavior inside the native loop wrapper;
  extracting `frame()` must not change native frame ordering or timing policy.
- Native runtime paths retain current `SDL_GetBasePath()`, bundle resource,
  `game_data`, and `SDL_GetPrefPath()` behavior and lookup precedence.
- Browser paths `/resources`, `/game_data`, and `/user` are never compiled into
  or selected by native targets.
- Native audio continues using current Apple AudioToolbox or mpg123 sources and
  behavior. Browser Web Audio code is never linked into native targets.
- Native TCP multiplayer sources, executables, and tests remain present and
  unchanged unless a portable-core split requires link-only restructuring.
  Such restructuring must keep the same native TCP implementation and tests.
- Native save and settings formats and locations remain unchanged.
- Native resources remain sourced, copied, and bundled exactly through native
  CMake rules. Browser asset pruning must never alter or replace native resource
  manifests or `game_data`.
- macOS bundle still embeds SDL3 under `Contents/Frameworks`, has no Homebrew
  runtime dependency, and passes existing signing, resource, architecture,
  deployment-target, launch, render, and exit checks.
- Browser outputs exist only below `build-web/`; no browser-generated file may
  enter `build/`, `build-release/`, native app bundles, or native install rules.

### Required CMake isolation

Browser target creation must use this outer guard:

```cmake
option(AOE_BUILD_WEB "Build browser WebAssembly risk spike" OFF)

if(EMSCRIPTEN AND AOE_BUILD_WEB)
    include(cmake/BrowserBuild.cmake)
endif()
```

Native target definitions must remain outside that guard. Browser and native
platform implementations must be selected with explicit target source lists:

```text
aoe_reconstruction / aoe_reconstruction_app
  src/main.cpp
  src/runtime_paths_native.cpp
  src/audio_system.cpp

aoe_web
  src/web_main.cpp
  src/runtime_paths_web.cpp
  src/audio_system_web.cpp
```

Do not use global compiler flags, global linker flags, global include paths, or
global compile definitions for browser requirements. Attach them only to
`aoe_web` or browser-only libraries. Do not add `__EMSCRIPTEN__` branches to
shared game rules or simulation. Do not change native source lists merely to
make the disabled browser option configure.

### Baseline before first game-code change

Before changing game code, record:

```sh
git status --short
make
```

Also record native target names and confirm `build/AoE Archaeology.app` exists.
This creates comparison evidence; it does not permit accepting later failures
as pre-existing without matching evidence.

### Gate after every game-code commit candidate

Before committing any browser-spike change that alters C++, headers, CMake,
native resource wiring, or shared tooling, run from repository root:

```sh
make
```

Then run focused native tests for every shared behavior changed by that commit.
For frame-lifecycle changes, exercise native startup, repeated frames, normal
quit, and shutdown. For runtime-path changes, exercise development executable
and app-bundle resource and preference paths. For portable-core changes, run
native simulation and multiplayer tests. For audio-interface changes, run
native audio checks. Existing tests may be extended, but browser-only tests
cannot substitute for these native checks.

If `make` or any relevant native check fails, do not commit, do not describe the
browser step as complete, and do not continue stacking browser work on top of
the failure. Fix the in-scope regression first. If unrelated existing work is
the proven cause and must be preserved, stop that commit and report the exact
blocker.

### Final clean macOS gate

Before declaring the spike complete, validate a fresh native Release bundle,
separate from `build-web/`:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./scripts/verify_macos_bundle.sh "build-release/AoE Archaeology.app"
./scripts/test_isolated_build.sh .
```

Retain evidence that:

- Emscripten is neither discovered nor required by native configuration.
- Native tests pass without browser files in runtime or package paths.
- Bundle executable and embedded SDL3 each contain `arm64` and `x86_64`.
- Executable, SDL3, and `Info.plist` retain macOS 11.0 minimum version.
- Bundle resource manifest and required game data validate.
- Bundle has no direct Homebrew linkage.
- Code signature verification passes.
- Bundled executable starts, renders an 800x600 frame, and exits cleanly.
- Isolated build and runtime do not read browser output or parent workspace
  inputs.

Deleting or renaming an existing native test, disabling a native check,
loosening the bundle verifier, or excluding failing native behavior is not an
acceptable way to satisfy this gate.

## Browser matrix

Chrome is development browser. Full journey must pass there first.

Before declaring all risks validated, run same production bundle in current
stable releases of:

- Chrome on macOS.
- Firefox on macOS.
- Safari on macOS.

Firefox and Safari must complete loading, audio unlock, required play journey,
autosave reload, display cases supported by their automation, tab suspension,
victory, and restart. Browser-specific failures remain open risks; Chrome
success alone does not complete spike.

## Acceptance evidence

One spike run must retain:

- Emscripten and SDL version identifiers.
- Clean configure and build logs.
- Asset manifest and determinism comparison.
- Static-server request log with no missing assets.
- Browser console log with no uncaught errors.
- Step-by-step journey result.
- Screenshots for loading, gathering, research, combat, victory, restored save,
  and restarted scenario.
- Pointer/display case results.
- IndexedDB persistence and failure results.
- Tab suspension results.
- Memory checkpoint measurements.
- Audio instance measurements.
- Native `make` result for every completion commit containing game-code changes.

Evidence belongs under disposable `artifacts/` unless a specific compact report
or fixture must be tracked.

## Completion gate

Spike is complete only when all conditions hold:

- Clean browser build produces separate `aoe_web` package.
- Required journey passes through production path in Chrome, Firefox, and
  Safari.
- Every named risk has retained evidence described above.
- Asset pack is deterministic and complete for journey.
- Pointer targeting passes all display cases.
- Audio unlocks, streams, and does not duplicate after restart.
- Settings and autosave survive reload after confirmed synchronization.
- Storage failure is reported accurately.
- Tab suspension recovers within defined timing limits.
- Memory stays within predefined numeric budgets across two playthroughs.
- Native targets and packaging remain unchanged.
- Native `make` and relevant native regression checks pass before each
  completion commit containing game-code or build-system changes.
- Fresh Release `ctest`, macOS bundle verification, and isolated-build gate pass.
- Native macOS bundle remains Universal 2, targets macOS 11.0, contains its
  pinned SDL3 framework and full native resources, and has no browser runtime or
  build dependency.

Any missing item means spike remains incomplete and corresponding risk remains
open.

## Explicit exclusions

Do not implement during this spike:

- Campaigns.
- Random maps.
- Scenario selection.
- Civilization or settings selection.
- Scenario editor.
- Multiplayer or browser networking.
- Replays.
- Legacy imports.
- General save browser or multiple save slots.
- More than one autosave.
- General browser menus.
- Service worker, offline mode, install prompt, or PWA caching.
- Mobile or touch controls.
- Broad asset compatibility.
- Performance optimization beyond meeting declared spike budgets.
- Deployment automation beyond local static HTTP hosting.

## Commit sequence

1. `build: add disabled browser risk-spike target`
2. `refactor: expose non-blocking SDL frame lifecycle`
3. `refactor: isolate native and browser runtime paths`
4. `tools: generate fixed-scenario browser asset closure`
5. `feat: run fixed scenario in SDL WebAssembly target`
6. `feat: add browser MP3 unlock and streaming`
7. `feat: normalize browser canvas input and display`
8. `feat: persist one browser autosave and settings`
9. `test: automate cross-browser risk-spike journey`

Every game-code or build-system commit requires successful native `make` plus
relevant native regression checks before commit. Browser commits also require
the browser build or risk check appropriate to the capability introduced. Final
completion additionally requires the clean Release, bundle-verifier, and
isolated-build commands in the native macOS non-regression contract.

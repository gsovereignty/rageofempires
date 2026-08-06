# Single-Player Browser Build Plan

## Goal

Add a browser package as a separate Emscripten target while preserving native
behavior.

```text
Shared code
├── aoe_core
├── game rules
├── simulation
├── AI
├── rendering logic
└── scenario/content code

Platform targets
├── aoe_reconstruction          native executable
├── aoe_reconstruction_app      macOS bundle
└── aoe_web                     browser WASM package
```

Native build remains default. Browser behavior enters only through narrow
platform adapters and guarded CMake branches.

## Initial scope

- One packaged scenario.
- Single player versus AI.
- Fixed map, civilizations, teams, victory condition, and game settings.
- Loading, start, victory/defeat, and restart screens.
- Browser-native MP3 playback after user interaction.

Defer campaigns, random maps, scenario editor, multiplayer, replays, legacy
imports, and full save browser.

## Non-regression rules

- Keep existing native target names and defaults unchanged.
- Keep Emscripten optional and absent from native build requirements.
- Select platform implementations through target source lists.
- Avoid scattered `#ifdef __EMSCRIPTEN__` branches in shared game logic.
- Keep browser output under `build-web/`, never native runtime directories.
- Preserve native resources and macOS bundle packaging.
- Leave native TCP multiplayer unchanged initially.
- Add regression coverage for every changed shared behavior.
- Run native `make` before every completion commit containing game-code
  changes.

## Proposed structure

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
├── main.cpp
└── web_main.cpp

web/
├── BROWSER_BUILD_PLAN.md
├── shell.html
├── browser_runtime.js
├── styles.css
├── manifest.webmanifest
└── README.md

cmake/
└── BrowserBuild.cmake

tools/
└── build_web_asset_pack.py

tests/
├── application_loop_tests.cpp
├── runtime_paths_tests.cpp
└── web/
    └── browser_smoke_test.*
```

Exact files may change after implementation inspection. Platform differences
must remain behind interfaces selected by each target.

## Phase 1: Establish native baseline

1. Record working-tree state and preserve unrelated changes.
2. Run current required `make` gate.
3. Record native executable and tests produced.
4. Select browser scenario.
5. Record native startup, scenario loading, audio, input, save/settings,
   victory, and restart behavior for that scenario.

### Gate

- Native `make` passes.
- Selected scenario works through native production path.
- Exact browser scenario and settings are recorded.

## Phase 2: Add inert browser build option

Add disabled-by-default option:

```cmake
option(AOE_BUILD_WEB "Build browser WebAssembly target" OFF)

if(EMSCRIPTEN AND AOE_BUILD_WEB)
    include(cmake/BrowserBuild.cmake)
endif()
```

Do not modify existing native target source lists during this phase.

Expected browser configuration:

```sh
emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DAOE_BUILD_WEB=ON \
  -DAOE_ENABLE_MPG123=OFF
```

Native commands remain unchanged.

### Gate

- Native configuration requires no Emscripten installation.
- Native target graph remains unchanged apart from disabled option.
- Native `make` passes.
- Emscripten configuration recognizes separate browser target.

## Phase 3: Prove `aoe_core` under Emscripten

Compile shared non-SDL core first. Exclude native TCP, socket executables, and
native-only audit paths from browser target where required. Do not remove them
from native build.

If transport sources prevent portable build, split narrowly:

```text
aoe_core
  portable simulation and domain code

aoe_multiplayer_tcp
  native multiplayer transport
```

Add small Emscripten proof that constructs a simulation, loads minimal
scenario data, executes deterministic commands, advances ticks, and verifies
result.

### Gate

- Existing native multiplayer tests link same TCP implementation.
- Native `make` passes.
- Emscripten core probe passes.
- Simulation logic contains no browser-only branches.

## Phase 4: Extract frame lifecycle

Refactor blocking `SdlApp::run()` into reusable lifecycle:

```cpp
class SdlApp {
public:
    void initialize();
    bool frame();
    void shutdown();
    int run();
};
```

Native wrapper retains current loop and delay:

```cpp
int SdlApp::run() {
    initialize();
    while (frame()) {
        SDL_Delay(8);
    }
    shutdown();
    return 0;
}
```

Browser wrapper registers `frame()` with Emscripten browser main loop.
Preserve event-processing, simulation timing, rendering, shutdown, and
exception order. Keep `SDL_Delay(8)` in native wrapper only.

### Gate

- Existing native UI and gameplay smoke checks pass.
- Native `make` passes.
- Browser renders repeated frames without blocking page.

## Phase 5: Add runtime-path interface

Create shared contract:

```cpp
struct RuntimePaths {
    std::filesystem::path resources;
    std::filesystem::path game_data;
    std::filesystem::path user_data;
};

RuntimePaths runtime_paths();
```

Native implementation preserves existing `SDL_GetBasePath()`,
`SDL_GetPrefPath()`, bundle resource, and `game_data` behavior. Browser
implementation returns:

```text
/resources
/game_data
/user
```

Select implementation through target source lists, not pervasive preprocessor
branches.

### Gate

- Native path tests prove existing lookup precedence.
- Native application loads same packaged resources.
- Browser loads from virtual filesystem.
- Native `make` passes.

## Phase 6: Generate minimal scenario asset closure

Complete `game_data/` is roughly 1.1 GB. Generate deterministic browser pack
containing only content reachable from selected scenario.

```sh
python3 tools/build_web_asset_pack.py \
  --scenario resources/<selected>.scenario \
  --output build-web/web-assets
```

Traverse:

- Scenario map and roster.
- Civilizations and starting entities.
- Producible units and constructible buildings.
- Reachable technologies and upgrades.
- Terrain, projectiles, effects, and animations.
- UI icons, fonts, localization, sounds, and MP3 files.

Output:

```text
build-web/web-assets/
├── resources/
├── game_data/
└── web_asset_manifest.json
```

Manifest records source-relative path, hash, size, inclusion reason, and
dependency parent. Missing required assets fail build. Everything comes from
tracked reconstruction inputs. Keep full engine code initially; reduce assets
first.

### Gate

- Repeated generator runs produce identical hashes.
- Complete browser playthrough reports no missing assets.
- Native package remains unchanged.
- Native `make` passes after shared-code changes.

## Phase 7: Add `aoe_web` target

```cmake
add_executable(aoe_web
    src/web_main.cpp
    src/sdl_app.cpp
    src/audio_system_web.cpp
    src/runtime_paths_web.cpp
)
```

Link portable `aoe_core`, pinned Emscripten-compatible SDL3, and WASM Zlib.
Start with:

```text
ALLOW_MEMORY_GROWTH=1
FORCE_FILESYSTEM=1
MIN_WEBGL_VERSION=2
MAX_WEBGL_VERSION=2
EXIT_RUNTIME=0
```

Package assets at `/resources` and `/game_data`. Generate only into
`build-web/dist/`.

Validate canvas creation, `SDL_RenderGeometry`, textures, clipping, render
targets, text, events, fullscreen, and high-DPI output. Do not silently
substitute an unpinned SDL dependency.

### Gate

- Browser reaches first menu.
- Bundle builds from clean `build-web/`.
- Native targets and packaging remain unchanged.
- Native `make` passes.

## Phase 8: Add browser-native MP3 audio

Keep packaged MP3 files. Browser provides decoding and playback; native Apple
AudioToolbox and mpg123 remain native-only.

Preserve shared `AudioSystem` interface:

```text
Native:  audio_system.cpp
Browser: audio_system_web.cpp
```

Browser behavior:

1. Start with suspended audio context.
2. Unlock after first intentional user interaction.
3. Decode and reuse short effects.
4. Stream long MP3 music where practical.
5. Preserve volume and settings behavior.
6. Stop and release audio during restart.
7. Report muted or unavailable state when playback is blocked.

### Gate

- Native audio checks pass.
- Browser MP3 plays after user interaction.
- Long music does not require decoding whole soundtrack into memory.
- Restart does not duplicate playback.
- Native `make` passes.

## Phase 9: Define browser input and display behavior

- Focus canvas only after intentional interaction.
- Prevent gameplay keys scrolling page only while canvas is focused.
- Leave browser-reserved shortcuts untouched.
- Handle right-click deliberately.
- Use `ResizeObserver` for presentation changes.
- Keep SDL logical coordinates authoritative.
- Request fullscreen only from user gesture.
- Pause or throttle safely when page becomes hidden.

Test coordinate spaces:

```text
CSS canvas dimensions
canvas backing dimensions
device pixel ratio
SDL output dimensions
SDL logical game coordinates
```

Cover standard density, Retina/high-DPI, browser zoom, resize, fullscreen,
letterboxing, HUD controls, unit selection, drag selection, minimap targeting,
and edge scrolling.

### Gate

- Equivalent native and browser input produces same game command.
- Pointer behavior remains correct through resize and fullscreen.
- Native pointer behavior remains unchanged.
- Native `make` passes.

## Phase 10: Add persistence

Mount browser storage at:

```text
/user/settings
/user/autosave
```

At startup, mount IndexedDB, finish initial asynchronous sync, then start game
and use existing C++ file APIs. After save, report success only after IndexedDB
sync finishes.

Initial scope:

- Settings persistence.
- One autosave.
- Restart scenario.
- Optional clear-local-data control.

Keep native save and settings paths unchanged.

### Gate

- Browser autosave survives full page reload.
- Failed storage synchronization reports failure.
- Native save files remain compatible and unchanged.
- Native save/load tests and `make` pass.

## Phase 11: Restrict browser frontend

Expose only:

```text
Loading
  Start Game
    Fixed Scenario
      Victory / Defeat
        Play Again
```

Represent availability through shared capabilities:

```cpp
struct ProductCapabilities {
    bool campaigns;
    bool random_maps;
    bool scenario_editor;
    bool multiplayer;
    bool imports;
    bool replay_browser;
};
```

Native capabilities retain current behavior. Browser capabilities disable
features absent from package.

### Gate

- Native menus remain unchanged.
- Browser cannot enter unshipped screens.
- Capability tests and native `make` pass.

## Phase 12: Verify both products

Add browser checks without replacing native tests:

- Clean Emscripten configure and build.
- Static bundle completeness and correct WASM MIME behavior.
- Browser launch and loading completion.
- Scenario start, selection, movement, AI progress, victory, and restart.
- Autosave and reload.
- Console error collection.
- Missing-resource detection.
- Memory sampling across restart.

Suggested explicit targets:

```sh
cmake --build build-web --target aoe_web
cmake --build build-web --target web_smoke
```

Browser checks remain outside default native `make` unless browser build is
explicitly enabled. Complete a production-path playthrough through gathering,
construction, training, research, combat, victory/defeat, restart, and reload.
Test Chrome, Firefox, and Safari.

### Gate

- Browser production-path checks pass.
- Clean isolated native `make` passes.
- Native packaging uses no browser files or tools.

## Phase 13: Package and deploy

Produce static directory:

```text
dist/
├── index.html
├── aoe.js
├── aoe.wasm
├── aoe.data
├── styles.css
├── manifest.webmanifest
└── assets/
```

Hosting needs:

- HTTPS.
- `application/wasm` for WASM.
- Brotli or gzip compression.
- Immutable caching for hashed assets.
- Short-lived caching for HTML.
- No WASM thread requirement initially.

Add service worker only after ordinary loading works. Cache a complete,
version-matched engine and asset set atomically.

### Gate

- Generic static HTTPS hosting works in fresh browser profile.
- Cache updates preserve matching WASM and assets.
- Offline start works only after complete successful cache.
- Native packaging remains unchanged.

## Commit sequence

1. `build: add disabled browser target scaffold`
2. `refactor: separate SDL application frame lifecycle`
3. `refactor: isolate platform runtime paths`
4. `build: compile portable core with Emscripten`
5. `tools: generate fixed-scenario browser asset pack`
6. `feat: add SDL WebAssembly target`
7. `feat: add browser MP3 audio backend`
8. `feat: add browser canvas input and display shell`
9. `feat: persist browser settings and autosave`
10. `test: add browser production-path smoke coverage`
11. `build: add static browser distribution packaging`

Each code commit gets native `make`. Browser-specific commits also get browser
build or smoke gate appropriate to introduced capability.

## Main risks

1. Pinned SDL3 compatibility with selected Emscripten SDK.
2. Extracting large `SdlApp::run()` loop without native behavior drift.
3. Computing complete reachable asset closure.
4. Browser memory use from decoded graphics and audio.
5. High-DPI pointer conversion.
6. Asynchronous IndexedDB startup and save synchronization.
7. Browser timing after tab suspension.

Address in this order: SDL toolchain spike, frame-loop extraction, minimal
assets, playable scenario, persistence and audio, then cross-browser hardening.

## First milestone

- Separate `aoe_web` target.
- Native targets unchanged.
- One fixed scenario and minimal asset pack.
- Browser canvas rendering.
- Mouse and keyboard commands.
- AI and victory condition.
- Browser-native MP3 after user gesture.
- No saves yet.
- Chrome proof first.
- Native `make` green.

Then add IndexedDB, Firefox/Safari hardening, and offline packaging.

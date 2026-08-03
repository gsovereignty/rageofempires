# Pointer coordinate audit — 2026-08-04

## Result

Bounded audit confirmed one production pointer bug. A click in blank minimap
panel space outside the rendered map diamond is accepted, clamped to a distant
map edge, and moves the camera. Reproduced in two of two clean launches.

Coverage totals: **9 passed, 1 bug, 3 blocked, 12 not-tested**. This was not an
exhaustive audit. Main gaps are non-native real-window sizes, fullscreen and
resize pointer stability, drag endpoints, hover transitions, overlays,
multiplayer screens, map-size coverage, and a second physical display.

## Build and launch configuration

- Revision: `f6114b5e280effb798014d7dcaedadc6848c5a2f`
- OS: macOS 26.3.1 (25D2128), Apple M2 Max
- Display: built-in Liquid Retina XDR, 3456x2234 physical pixels, Retina 2x
- Game window: windowed, 1280x752 logical window points
- Renderer: 1280x752 logical, 2560x1504 drawable pixels, density 2.00
- World viewport: 1280x577 logical; HUD: 175 logical pixels
- Scenario: visible Single Player Random Map setup, Arabia, Maximum, seed 1
- Game processes: one long-lived process per reproduction, exact window IDs
  20193 and 20210; both terminated after capture
- Completion build: `make` passed; all configured targets built successfully
- Automation directories and raw evidence:
  `artifacts/pointer-coordinate-audits/2026-08-04T001043/`

## Coordinate contract

Production pointer path is:

1. macOS global logical screen point (only foreground checks).
2. SDL window event position in window logical points.
3. `SDL_ConvertEventToRenderCoordinates(renderer, &event)` once in
   `SdlApp::run`, before any pointer consumer.
4. Renderer-logical position. Gameplay consumers use it directly. Frontend
   menus additionally apply `FrontendLogicalTransform::window_to_logical` into
   the cropped 800x600 frontend canvas.
5. Surface-local resolution:
   - menu: half-open `FrontendMenuRect`;
   - HUD: anchored logical rectangles;
   - world: `pick_world_tile` using camera, isometric origin, zoom, and
     elevation;
   - minimap: anchored panel, isometric inverse, scaling-row lookup;
   - drag: converted down/motion/up positions in one logical space.
6. Semantic state: menu command, selection, world tile, minimap tile, or
   camera center.

Resize/fullscreen events refresh canonical logical and drawable extents before
later pointer events. Static inspection found no second renderer conversion.

## Independent oracle

Control bounds came from declared half-open rectangles and rendered borders.
World-picking expectations came from independently projecting known tile
centers in `world_tile_picker_tests.cpp`, including elevation diamonds.

For minimap checks, visible geometry was the rendered dark isometric diamond,
not `minimap_tile_at`'s panel formula. At 1280x752, panel bounds are
`x=944..1270`, `y=581..747` in renderer-logical content coordinates. Point
`(950,590)` is visibly inside the rectangular frame but northwest of the map
diamond. Expected tolerance is zero for hit classification: blank frame space
must not resolve any tile. Point `(1070,647)` lies on the visible player/start
marker and is the adjacent passing control.

## Matrix

| Cell | Result | Evidence / reason |
|---|---|---|
| Renderer conversion occurs once | passed | Single conversion site at `src/sdl_app.cpp:17554`; all pointer branches follow it |
| Frontend native 800x600 transform | passed | `frontend_menu_tests` completed successfully |
| Frontend 16:9 cover transform | passed | Independent transform assertions for 1920x1080 |
| Frontend ultrawide cover transform | passed | Independent transform assertions for 3440x1440 |
| Menu center activation | passed | Real 1280x752 navigation reached Single Player and Random Map |
| Disabled menu entries | passed | Pure activation assertions reject Regicide, Death Match, and disabled Learn to Play |
| World tile centers and elevation | passed | Source oracle covers elevations 0..7 and overlapping frontmost diamonds |
| Minimap valid diamond point | passed | `(1070,647)` restored camera to represented player/start area |
| Minimap panel miss outside diamond | bug | `(950,590)` moved camera to clamped map edge, 2/2 clean launches |
| Retina window/capture coordinate declaration | passed | Desktop capture reported 1280x752 logical; renderer logged 2560x1504 drawable and density 2.00 |
| Pure window-mode test | blocked | Test process stalled before completion; terminated and retained in raw logs |
| Remaining focused pure tests | blocked | Test binaries stalled before completion on this host; no pass inferred |
| Focused SDL smoke batch | blocked | Batch could not advance reliably after stalled test; no pass inferred |
| Minimum supported real window | not-tested | Minimum not established during bounded run |
| Odd-size real window | not-tested | No deterministic real-window injection trace |
| 16:10 real window | not-tested | No deterministic real-window injection trace |
| Ultrawide real window | not-tested | No attached ultrawide display |
| Fullscreen enter/exit pointer stability | not-tested | Transition smoke blocked; no real pointer trace |
| Live resize pointer stability | not-tested | Transition smoke blocked; no real pointer trace |
| Second display / density transition | not-tested | Only one display available |
| Hover miss-to-hit / hit-to-miss / A-to-B | not-tested | No semantic hover proof hook |
| Four-direction selection drag | not-tested | No correlated down/motion/up semantic trace captured |
| Technology-tree drag and wheel | not-tested | No correlated pointer trace captured |
| Multiplayer/setup/save/statistics pointer surfaces | not-tested | Not reached in bounded run |
| Every supported map size and rectangular map | not-tested | Runtime proof covered only Maximum square random map |

## PTR-001: Blank minimap frame click jumps camera to map edge

- Status: confirmed
- Product impact: minimap / camera targeting
- Affected matrix cells: windowed 1280x752 logical, Retina 2x, Maximum Arabia
  random map; other modes and map sizes not established
- Duplicate manifestations: none
- Expected: point visibly outside minimap diamond resolves no map tile and does
  not change camera
- Actual: blank northwest panel point is accepted, clamped to map boundary, and
  moves camera from player start to dark map edge
- Reproduction:
  1. Launch `build/aoe_reconstruction` through gameplay automation at 1280x752.
  2. Keyboard-open Single Player, Random Map; generate and start Maximum Arabia.
  3. Click `(950,590)` in logical-window coordinates.
  4. Observe camera jump while minimap viewport rectangle moves to left edge.
- Coordinate trace:
  - OS/global: window origin `(224,163)` plus window-relative logical point;
    exact title-bar-adjusted global event not logged
  - SDL window event: `(950,590)` logical point supplied by window-relative tool
  - renderer logical: `(950,590)` after one SDL conversion
  - surface-local: panel-local `(6,9)`, visibly outside diamond
  - resolved target: clamped northwest map edge (exact tile not logged)
  - resulting state: camera centered at boundary clamp; world view becomes dark
    edge terrain
- Adjacent passing control: `(1070,647)` on visible player/start marker restored
  camera to represented start area
- Reproducibility: 2/2 clean launches, window IDs 20193 and 20210
- Evidence:
  - `runtime1-before.png`
  - `runtime1-outside-diamond.png`
  - `runtime2-before.png`
  - interactive second-run observation recorded by desktop tool
- Relevant production path: `src/sdl_app.cpp`, `SdlApp::run` to
  `minimap_tile_at` to `center_camera_on`
- Original evidence: no matching original minimap hit-test symbol identified;
  decompiled cursor surface creation near `FUN_004dd250` was inspected but does
  not establish minimap behavior
- Likely cause: **hypothesis** — `minimap_tile_at` validates only inclusive
  rectangular panel bounds, then clamps inverse-projected coordinates instead
  of rejecting points outside the rendered isometric diamond
- Security classification: not security-classified

## Passing controls and raw commands

- Frontend native/wide/ultrawide conversion assertions passed in
  `frontend_menu_tests`.
- Real keyboard activation reached intended menus without pointer dependence.
- Valid minimap point and invalid blank-panel point produced distinct, visible,
  semantically consistent camera results.
- Screenshot SHA-256 values and focused test logs are retained under raw
  evidence directory.

## Blockers and remaining uncertainty

Several test executables stalled before completion, including
`window_mode_tests` and `world_tile_picker_tests`; an LLDB launch also stalled
before entering target code. Owned processes were terminated. Therefore source
assertions from those binaries support test design only, not a passing result
for this run. Exact minimap target tile was not exposed by production semantic
API, so visible camera and viewport movement prove wrong activation but not the
numeric clamped tile.

## Skill improvement

- Lesson: existing `AOE_MINIMAP_CLICK_PROOF` exercises one valid hard-coded
  point only. It cannot inject an outside-diamond negative control or emit full
  coordinate stages, forcing a short foreground interval.
- Decision: rejected skill or script change in this run. Evidence identifies a
  repository test-hook gap, but adding a fixture-specific instruction or audit
  script without product-side semantic tracing would reuse the production
  oracle and would not make future audits reliable.
- Files changed: audit report only.
- Validation: no skill files changed; skill-creator validation and skill diff
  checks therefore not applicable.
- Remaining uncertainty: a future product test hook should accept explicit
  logical coordinates and log panel-local point, hit/miss, resolved tile, and
  camera target before this can become background-only deterministic coverage.

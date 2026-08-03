# Pointer coordinate test matrix

Run every applicable combination. Discover supported values from current code and UI; never assume this list is exhaustive.

## Presentation dimensions

- Window modes: windowed, fullscreen; borderless if supported.
- Sizes: minimum supported; native 800x600; 16:9; 16:10; ultrawide; current display size; one odd width and height.
- Density: 1x and every available high-DPI scale. If hardware exposes one density, cover others with pure conversion tests and mark real-window cells `not-tested`.
- Transitions: launch at size; resize smaller/larger; fullscreen enter/exit; move between displays when available.

Record window points, drawable pixels, renderer output, renderer logical size, viewport height, HUD height, presentation scale, and letterbox offsets after every transition.

## Surfaces

- Main menu, each submenu, close controls, disabled entries, gaps, background, cropped/letterbox regions.
- Gameplay HUD command buttons, resource/status regions, minimap frame, minimap diamond, area inside frame but outside diamond.
- World viewport center, corners, tile boundaries, units/buildings, empty terrain, viewport/HUD boundary.
- Selection drag in every direction, including release outside start region.
- Technology tree or other overlays using pointer motion, drag, or wheel.
- Multiplayer/setup/save/statistics screens when reachable.

## Point sets

For every rectangular control: center; four points one logical unit inside corners; edge midpoints; points one logical unit outside each edge; gap midpoint; distant miss.

For every hover control: miss-to-hit, hit-to-miss, hit-A-to-hit-B, outside-window/focus-loss when testable, resize while hovered.

For minimap: four map corners; center; quarter points; both diagonals; known player/unit markers; points outside diamond but inside panel; points outside panel. Cover every supported map size and at least one rectangular synthetic map when engine supports it.

For world tiles: known tile centers across camera offsets; four viewport corners; adjacent tiles; map boundaries; HUD boundary; post-camera-pan and post-resize.

## Required invariants

- Each physical event undergoes renderer conversion once.
- Hover state equals current hit, never last successful hit.
- Hover and click resolve same control at same point.
- Outside point activates nothing.
- Rendered minimap point and inverse click agree within declared quantization tolerance.
- Minimap background outside diamond does not silently clamp to distant map edge unless documented contract requires it.
- Camera target after minimap click matches resolved tile within camera boundary clamp.
- Same normalized logical point resolves consistently across presentation modes.
- Down, motion, and up coordinates share coordinate space.
- Screenshot evidence uses declared logical/window/pixel coordinate space.

# Options and reconstruction settings

Open Options with `O` from the reconstruction main menu or `Esc` during a
single-player game. Letter keys shown beside each setting cycle its value.
`A` applies without writing, `S` saves and applies, and `Esc` cancels the
draft. `H` opens the hotkey reference.

Settings are stored as `reconstruction-settings.txt` below SDL's user-data
directory. Format version 2 is validated before use and written through a
sibling temporary file followed by an atomic rename. Version 1 files migrate
the three new category volumes from the old effects volume; malformed and
unsupported files leave safe defaults active.

Single-player cadence, fullscreen state, camera and edge scrolling, fog
presentation, and minimap visibility apply at runtime. Multiplayer cadence is
never changed by local settings. Current audio backend has one startup gain
and no category mixer, so music/effects/category values persist for later
audio startup and panel labels this limit. Only windowed/fullscreen modes are
offered. Panel is procedural because no matching archive options artwork has
been proven.

The saved fullscreen choice is applied when the SDL window is created.
`F11` and `Alt+Enter` change the live, active, and draft fullscreen value
together; a later `S` persists the actual live value. `A` still applies
without writing. Failed fullscreen transitions remain windowed and roll the
panel values back.

The window is resizable down to 640x360 window-coordinate units. Each drawable
pixel-size or display-scale change replaces the canonical render extent, so a
wider or taller window exposes more world rather than scaling a fixed 16:9
canvas. The fixed-height HUD remains attached to the current bottom edge.
Renderer-coordinate conversion keeps high-DPI pointer input in that same
adaptive pixel space.

Verification status: implementation and automated SDL/policy checks pass, but
fullscreen, live dragging, geometry restoration, high-DPI input, and Options
state synchronization have not yet been tested by a human. Manual desktop
acceptance remains required before calling this behavior fully verified.

Proof:

- `settings_tests`: round trip, atomic replacement residue, v1 migration, and
  invalid-file rejection.
- `window_mode_tests`: drawable validation and fullscreen state/geometry
  synchronization.
- `window_mode_sdl_smoke`: live resize capture and fullscreen round-trip or
  documented dummy-driver rollback.
- `/tmp/aoe-options-panel.png`: deterministic panel capture using
  `AOE_OPTIONS_PANEL=1`.

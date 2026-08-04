# Options and reconstruction settings

Open Options with `O` from the reconstruction main menu or `Esc` during a
single-player game. Letter keys shown beside each setting cycle its value.
`A` applies without writing, `S` saves and applies, and `Esc` cancels the
draft. `H` opens the hotkey reference.

Settings are stored as `reconstruction-settings.txt` below SDL's user-data
directory. Format version 3 is validated before use and written through a
sibling temporary file followed by an atomic rename. Versions 1 and 2 migrate
their linear loudness percentages into original 0..99 attenuation direction;
obsolete category values are discarded. Malformed and unsupported files leave
recovered defaults active.

Single-player cadence, fullscreen state, camera and edge scrolling, fog
presentation, and minimap visibility apply at runtime. Multiplayer cadence is
never changed by local settings. Music and Sound use original attenuation
sliders: 0 is loudest and 99 is quietest. Sound controls combat, interface,
and ambient playback together. Both persist and apply live. Only
windowed/fullscreen modes are offered. Panel is procedural because no matching
archive options artwork has been proven.

The saved fullscreen choice is applied when the SDL window is created.
`F11` and `Alt+Enter` change the live, active, and draft fullscreen value
together; a later `S` persists the actual live value. `A` still applies
without writing. Failed fullscreen transitions remain windowed and roll the
panel values back.

The window is resizable down to 640x360 window-coordinate units. Each window
size or display-scale change refreshes the canonical render extent, so a
wider or taller window exposes more world rather than scaling a fixed 16:9
canvas. The fixed-height HUD remains attached to the current bottom edge.
Canonical UI dimensions stay in window-coordinate units while SDL scales them
to the drawable. This prevents Retina/high-DPI output pixels from shrinking
menus and HUD text. Renderer-coordinate conversion keeps pointer input in that
same adaptive coordinate space.

Verification status: initial human testing found Retina drawable pixels were
incorrectly used as UI coordinates, making everything too small. Canonical UI
sizing now uses window-coordinate units; automated SDL/policy checks pass.
Human retesting of sizing, fullscreen, live dragging, geometry restoration,
high-DPI input, and Options synchronization was accepted on 2026-07-31.

Proof:

- `settings_tests`: round trip, atomic replacement residue, v1 migration, and
  invalid-file rejection.
- `window_mode_tests`: drawable validation and fullscreen state/geometry
  synchronization.
- `window_mode_sdl_smoke`: live resize capture and fullscreen round-trip or
  documented dummy-driver rollback.
- `/tmp/aoe-options-panel.png`: deterministic panel capture using
  `AOE_OPTIONS_PANEL=1`.

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

Proof:

- `settings_tests`: round trip, atomic replacement residue, v1 migration, and
  invalid-file rejection.
- `/tmp/aoe-options-panel.png`: deterministic panel capture using
  `AOE_OPTIONS_PANEL=1`.

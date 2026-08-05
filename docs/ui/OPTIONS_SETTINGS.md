# Options and display workflow

Options opens from main menu or `Esc` in single-player. It keeps separate
active and draft values. `A` applies draft values without writing, `S` applies
and atomically saves, and `Esc` discards draft changes. Validation or SDL mode
transition failures leave live state intact and show an error in panel.

## Controls

- `G`: slow, normal, or fast single-player cadence.
- `D`: current display's enumerated modes at 800x600 or larger.
- `M` / `E`: original-direction music/sound attenuation, 0 loudest, 99 quietest.
- `F`: windowed or fullscreen.
- `R` / `C`: scroll and mouse speed.
- `H`: editable hotkey page. `1`-`8` chooses action, next key binds it, `D`
  restores defaults. Duplicate bindings are rejected with visible feedback.

Settings use validated format version 5 under SDL user-data. Versions 1-4
migrate, including old volume direction. Resolution, mode, audio, speed,
presentation, locale, and hotkeys round-trip. Writes use sibling temporary
file plus atomic rename.

## Display contract

Original display selection enumerates platform modes, then renders chosen
resolution as fixed canvas. Reconstruction filters current SDL display modes,
deduplicates and sorts them, and uses same fixed-canvas rule. Resizing window
changes scale/letterbox only; it never reveals extra world. Fullscreen and
windowed transitions retain last window geometry. SDL logical presentation
performs letterboxing and maps input back into canvas. Logical size never
follows Retina drawable pixels, preserving high-DPI size and hit testing.

Read-only decompiled evidence: `AoK-HD-patched.c` functions `FUN_005ff420`,
`FUN_006031e0`, `FUN_00603360`, and `TRIBE_Screen_Options::draw` recover named
fullscreen artwork, enumerated display modes, tab controls, defaults/apply
actions, and options drawing. Persistent keys include `Music Volume`,
`Sound Volume`, `Game Speed`, `Scroll Speed`, and `FullScreen`.

Proof: `settings_tests`, `window_mode_tests`, `window_mode_sdl_smoke`, and
`options_settings_sdl_smoke` cover migration, conflict rejection, persistence,
mode filtering, fixed high-DPI canvas, resize letterboxing, fullscreen rollback,
and rendered options controls.

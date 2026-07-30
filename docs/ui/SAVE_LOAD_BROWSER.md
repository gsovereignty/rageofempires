# Save, load, and replay browser

Open browser with `L` from main menu or `F4` in game. It lists only direct,
regular files in SDL's application user-data directory. Symlinks,
subdirectories, arbitrary paths, deletion, and filename traversal are not
available.

Project-native `Save110` and `Replay64` files receive format/version,
modification time, tick, civilization, outcome, and replay-command metadata
where stored. Compatible saves load into play; compatible replays reset the
base scenario and start deterministic playback. Corrupt files show parser
diagnostics. Other project versions are labeled incompatible instead of being
opened.

Commercial `.mgz`, `.mgl`, `.aoe2record`, and `.sav` files are visibly
inspect-only. Browser does not claim project-native compatibility with them.

Save109 is first native eight-slot save schema. It stores all eight roster
slots, controller IDs and kinds, teams, cooperative-control flags, diplomacy
rules and every directed occupied-slot stance. Each slot carries economy,
age, civilization, formation, controller state, technologies, exploration,
farm-reseed state, civilization arithmetic remainders, and victory countdown
state. Entity/effect owners use stable IDs 0-7, with 8 reserved for neutral.
All eight cumulative-statistics rows and timeline lanes round-trip. Save108
and older files still load through exact blue/red migration; native state is
never silently written back in a legacy schema.

Replay63 writes one `source` record for every scheduled command. Values 0-7
are stable roster slots; `-1` explicitly preserves unresolved legacy entity
commands whose owner must be resolved when applied. Replay62 and older files
retain their existing migration: player-valued commands infer blue/red source,
while entity-valued commands remain unresolved until application. Missing,
duplicate, neutral, or out-of-range Replay63 sources are rejected.

Press `N`, type a slot containing 1-32 letters, digits, hyphens, or
underscores, then press Enter to save. Existing names require a second
explicit Enter confirmation. Writes use a sibling temporary file and atomic
rename; failures remove the temporary file. Up/Down selects entries and Enter
loads or starts playback.

Panel is procedural because no matching archive browser artwork has been
proven.

Proof:

- `save_browser_tests`: atomic save/replay writes, overwrite confirmation,
  traversal rejection, temporary cleanup, compatible metadata, corruption,
  incompatible versions, and commercial inspect-only classification.
- `/tmp/aoe-save-browser.png`: inspected diagnostics/legacy capture.

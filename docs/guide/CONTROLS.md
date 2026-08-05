## Main menu

Ordinary random-map roster controls are documented in
[`../ui/ORDINARY_MATCH_SETUP.md`](../ui/ORDINARY_MATCH_SETUP.md).

Set `AOE_MAIN_MENU=1` to open main menu, or
`AOE_MENU_REFERENCE=1` to open Single Player flyout with Campaigns focused.
Mouse motion changes focus; left click activates. Up/Down wraps focus,
Enter/Space activates, and Escape or flyout `X` returns to main menu.

Main choices retain supported Campaign, Random Map, multiplayer, editor,
Options, Custom Scenario, and Saved Game routes. Learn to Play launches guided
objectives. Regicide and Death Match open setup, then launch their distinct
King-survival or post-Imperial/high-resource rules. Zone opens a retired-service
screen, names its historical URL and supported Multiplayer alternative, and
makes no network request.

`Arabia vs AI` opens six map-size choices: Tiny (120), Small (144), Medium
(168), Normal (200), Large (220), and Giant (240 tiles per side). Each starts
one human against one Moderate AI in Dark Age Arabia with standard resources,
normal fog, and Conquest victory; current match remains 1v1.

## Scenario editing

Direct gameplay starts on a deterministic generated random map. Campaign mission
"Foundations" is [foundations.scenario](../../resources/foundations.scenario). Records describe
map size, economy, terrain rectangles, individual terrain overrides, units, and
buildings. Lines beginning with `#` are comments. Edit file, rebuild, and app
bundle receives updated scenario under `Contents/Resources`.
Building records may end with `rally X Y` to assign a starting rally point.
Scenario v28 farm records may include `resource_amount N` from 0 through 250,
covering base and Horse Collar depletion states for render audits.
Unit records may end with `attack_move X Y` to start advancing and engaging.
They may instead end with `patrol X Y` to start a repeating combat patrol.
`guard_unit X Y` or `guard_building X Y` assigns a friendly protection target.
One or more trailing `waypoint X Y` markers append queued route legs.
`stance aggressive|defensive|stand_ground|passive` sets initial behavior.
- `save_game`: explicit versioned persistence boundary.
- `SdlApp`: macOS window, input translation, timing, and rendering only.

This separation permits simulation testing without graphics and replaces the
recovered Win32/Direct3D 9 boundary with SDL3. Names describe domain intent;
binary addresses stay in the separate archaeology corpus.

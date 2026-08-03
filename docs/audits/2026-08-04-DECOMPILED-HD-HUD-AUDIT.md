# Decompiled HD HUD audit — 2026-08-04

## Scope and result

Targeted audit of `resources/visual-audit.scenario` at 1280×1024 after
replacing reconstruction HUD geometry with the recovered `FUN_005e7cb0` and
`FUN_005f37c0` contracts. Production captures used deployed `game_data`; no
legacy-asset disable flag was set.

A final 1920×1080 no-selection production capture additionally verifies native
wide-screen tiling and screen-relative minimap/ornament anchors.

Status: **passed after fixes**. One scenario audited, three HUD states passed,
two duplicated presentation defects fixed, no blocked states.

## Evidence

Runtime logs prove packaged terrain, interface archives, loose
`game_b%d.slp`, menu/statistics SLPs, and gameplay sprites loaded. The fixture
reported 24×16 tiles. Captures began in gameplay, never frontend:

- tick 0: no selection;
- tick 100: battering ram 7 selected after semantic selection at tick 28;
- tick 100: Town Center 27 selected after semantic selection at tick 65.
- final 1920×1080: no selection, native production assets.

Disposable evidence lives under
`artifacts/scenario-audits/visual-audit/captures/production-*`. Every owned
capture process exited successfully and was reaped.
Final post-fix captures are under `artifacts/hud-production/final-*`.

## Decompiled comparison

Observed HUD starts at y=849, exactly 175 pixels above 1024-pixel screen
bottom. Native frames cover expected roles: frame 0 top tiling, frame 6 origin
overlay, alternating frames 2/3, left/right caps 1/4, centered frame 5, and
sibling-relative frame 7. Frame 5 intrudes upward by half its height, matching
`bottom-frame5.height/2`; it occupies blank center space without covering
commands, portrait, entity facts, or minimap.

Command slots now use recovered 5×3 operands `(37 + 41*column,
bottom + 31 + 41*row, 40, 40)`. Frame 7 carries centered current-Age text.
Compact resource, population, idle-Villager, and idle-military values remain
inside the proved 420×16 status strip.

## Online comparison

The [2013 ModDB HD Edition screenshot](https://www.moddb.com/games/age-of-empires-ii-hd/images/screenshot)
and [GameStar HD Edition gallery](https://www.gamestar.de/galerien/age_of_empires_2_hd_edition%2C96326.html)
show the same hierarchy: compact top resources, full-width civilization skin,
left command grid, parchment information region, and right minimap. Exact
pixel comparison is inappropriate because reference matches use different
civilizations, resolutions, and selection states; structural comparison and
decompiled operands agree.

## Fixed findings

- `VF-HUD-OVERLAY-01`: removed reconstruction-only `YOU` and `ENEMY` text
  from minimap.
- `VF-HUD-TEXT-01`: selection labels now render title-cased rather than raw
  mechanical lowercase names.

## Functional coverage

Unit and building selections update portrait, HP/status text, and command
icons. Command-grid hit testing shares recovered draw rectangles. Focused
command, selection, minimap, icon, HUD-contract, and SDL screenshot tests cover
dispatch and presentation. Full repository `make` remains completion gate.

## Remaining bounded gaps

- Online comparison is structural, not pixel-aligned.
- Exact commercial font metrics and exact internal resource-field partitions
  remain unrecovered.
- This targeted audit did not exercise every civilization skin or every
  command-page state; installed-asset tests separately validate all 18 HUD
  files and every frame.

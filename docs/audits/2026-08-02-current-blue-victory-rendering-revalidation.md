# Blue-victory rendering revalidation — 2026-08-02

## Result

Blue won Random Map seed `42` by Conquest at tick 5008. Testing covered menu,
Dark Age gameplay, combat, victory, and all five postgame tabs using semantic
gameplay plus window captures and real tab clicks.

This run found eight current defects:

1. Continue button sprite renders as orange/white noise.
2. Rematch button sprite renders as orange/white noise.
3. Back to Menu button sprite renders as orange/white noise.
4. Timeline omits all postgame action buttons.
5. Timeline omits match title, result, and victory cause.
6. Overlapping Blue/Red Timeline traces are indistinguishable.
7. Timeline chart lacks axis labels and tick values.
8. Timeline summary uses an unaligned text run.

Original evidence loads dedicated `AchDecal.slp`, `PNBnr1.slp`,
`sat_tabs.slp`, `sat_btn.slp`, `AchTeam.slp`, and `tml_bck.slp` statistics
assets (`decompiled/AoK-HD-patched.c`, around lines 317735–317875). No security
classification is claimed.

## Coverage

Observed: menu, Dark Age Town Center, villager idle/walk/melee, scout
idle/walk, sheep idle, terrain, shroud, HUD, minimap, and statistics tabs.

Not verified: later ages, every unit/building/civilization, construction,
projectiles, deaths, naval, siege, monks, wonders, every direction, or every
animation frame. This playthrough cannot prove universal sprite correctness.

## Remaining matrix

- Modes: Random Map, Custom Scenario, Campaign, Saved Game, replay, local
  multiplayer, and Scenario Editor playtest.
- Random Map: 4 maps (Arabia, Black Forest, Islands, Rivers) × 3 victories
  (Conquest, Wonder, Relic) × 5 AI difficulties = **60 baseline runs**.
- Campaign: `briefing-demo.campaign` contains Foundations and The High Ground.
- Fixtures: all **70** `resources/*.scenario` files cover ages, terrain,
  animation, workers, economy, monks, relics, naval combat, civilizations,
  upgrades, buildings, and gates. Directory contents are source of truth.
- Disabled/incomplete: Regicide, Death Match, Learn to Play, and Zone.

Next: run 70 fixtures at documented capture ticks, then 60 Random Map cases,
both campaign missions, and representative save/replay/multiplayer paths.

## Reproduction

Start seed `42`; attack-move Blue units 1, 2, 3, and 7 to tile 191,127;
advance 5000 ticks; capture Economy; click Military, Society, Technology, then
Timeline.

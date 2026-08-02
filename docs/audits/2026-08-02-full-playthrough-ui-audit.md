# Full-playthrough UI and rendering audit — 2026-08-02

> Remediation status: **closed**. All findings listed here were addressed by
> commits `eab2ff1`, `dc0cf17`, `8be5316`, `cec443f`, `505c670`, and the final
> camera/scouting follow-up recorded after this report. Exact statistics
> interface SLPs now load from tracked `game_data/Data/interfac.drs`; a
> reconstruction-native fallback remains available when legacy assets are
> explicitly disabled. No parent-workspace runtime dependency was added.

## Result and method

Blue won a complete deterministic Random Map match (seed `42`) by Conquest at
tick `5422`. The run started from the visible main menu, used the opt-in local
gameplay API for repetitive simulation, used non-activating window captures at
menu, opening-world, selection, movement/combat, and terminal checkpoints, and
then exercised real pointer/keyboard input on the terminal screen. A separate
seed `99` run reproduced construction, hunting, pathing, and population
blockers before seed `42` supplied a completable match.

Build under test: current Release app bundle produced by `make`, 1280 by 752
logical window. Security classification is not claimed.

This audit counts independently actionable presentation defects separately.
It does not count an untested sprite as a bug and does not claim exhaustive
asset coverage from one Dark Age match.

## 100 confirmed UI and rendering bugs

### Main menu (UIR-001 through UIR-020)

1. **UIR-001 — Single Player label is detached from its blank beveled bar.**
2. **UIR-002 — Multiplayer label is detached from its blank beveled bar.**
3. **UIR-003 — Map Editor label is detached from its blank beveled bar.**
4. **UIR-004 — Options label is detached from its blank beveled bar.**
5. **UIR-005 — Zone label is detached from its blank beveled bar.**
6. **UIR-006 — History label is detached from its blank beveled bar.**
7. **UIR-007 — Exit label is detached from its blank beveled bar.**
8. **UIR-008 — Learn to Play label has no matching active-control treatment.**
9. **UIR-009 — Selected Single Player label uses yellow text while its associated bar remains empty.**
10. **UIR-010 — Main-menu labels do not share one alignment axis.**
11. **UIR-011 — Main-menu controls have inconsistent widths without content-based reason.**
12. **UIR-012 — Main-menu controls have inconsistent vertical spacing.**
13. **UIR-013 — Multiple empty bars look interactive despite containing no text or icon.**
14. **UIR-014 — Large lower parchment panel is blank.**
15. **UIR-015 — Parchment panel clips off bottom edge instead of ending inside frame.**
16. **UIR-016 — Right-side artwork is abruptly cut by black window margin.**
17. **UIR-017 — Left-side artwork begins after a solid black gutter.**
18. **UIR-018 — Background is not centered in usable content area.**
19. **UIR-019 — Menu lacks visible keyboard focus indicator distinct from hover color.**
20. **UIR-020 — Real click and Enter on highlighted Single Player produce no visible transition.**

### In-match top bar and world viewport (UIR-021 through UIR-040)

21. **UIR-021 — Top resource bar is only about 26 logical pixels high, making text cramped.**
22. **UIR-022 — Wood icon touches left window edge.**
23. **UIR-023 — Wood label/value lack padding from icon.**
24. **UIR-024 — Food label/value use different apparent spacing from Wood.**
25. **UIR-025 — Gold label/value crowd their compartment border.**
26. **UIR-026 — Stone label/value crowd their compartment border.**
27. **UIR-027 — Population and idle counts are compressed into one ambiguous field.**
28. **UIR-028 — Top-bar compartment widths do not follow label/value width.**
29. **UIR-029 — Decorative strip continues across unused right side with no controls.**
30. **UIR-030 — Top bar and world viewport have no clean separating border.**
31. **UIR-031 — Unexplored space renders as flat pure black instead of fog/shroud texture.**
32. **UIR-032 — Explored-map boundary has coarse staircase teeth.**
33. **UIR-033 — Boundary teeth vary irregularly between adjacent isometric rows.**
34. **UIR-034 — Terrain tiles show repeated square/diamond texture motifs.**
35. **UIR-035 — World viewport has no visible camera-edge affordance.**
36. **UIR-036 — Blue Town Center overlaps nearby unit silhouettes at default camera framing.**
37. **UIR-037 — Berry bushes are partly hidden behind HUD frame at initial camera.**
38. **UIR-038 — Default framing places important gather targets under lower HUD.**
39. **UIR-039 — Selection diamond is oversized relative to scout footprint.**
40. **UIR-040 — Selection diamond intersects berry-bush pixels instead of reading above ground.**

### Bottom HUD and command panel (UIR-041 through UIR-065)

41. **UIR-041 — Empty left panel is a large unused textured block.**
42. **UIR-042 — No-selection prompt is rendered inside a small dark rectangle over parchment.**
43. **UIR-043 — Prompt rectangle is not aligned to center-panel borders.**
44. **UIR-044 — Prompt text wraps into two lines despite ample panel width.**
45. **UIR-045 — Prompt text has too little top padding.**
46. **UIR-046 — Prompt text has too little bottom padding.**
47. **UIR-047 — Civilization/age/status debug line has extremely low contrast.**
48. **UIR-048 — Debug line is partially obscured by prompt rectangle.**
49. **UIR-049 — Shortcut/help line has extremely low contrast.**
50. **UIR-050 — Shortcut/help line extends beyond right edge beneath minimap.**
51. **UIR-051 — Shortcut/help line exposes internal condensed control text instead of structured UI.**
52. **UIR-052 — Selected-unit portrait is tiny relative to available center-panel space.**
53. **UIR-053 — Portrait has flat black surround inconsistent with parchment.**
54. **UIR-054 — Unit name is not visually aligned with portrait top.**
55. **UIR-055 — HP and activity state are concatenated on one undifferentiated line.**
56. **UIR-056 — Command icons touch neighboring icon borders.**
57. **UIR-057 — Command icons use inconsistent internal padding.**
58. **UIR-058 — Shortcut letters overlap icon artwork.**
59. **UIR-059 — Shortcut letters sit on different baselines.**
60. **UIR-060 — Red disabled-state slash obscures underlying icon.**
61. **UIR-061 — Disabled and unavailable commands are not visually distinguishable.**
62. **UIR-062 — Command grid leaves most left panel unused while icons remain cramped.**
63. **UIR-063 — Final command row is incomplete without empty-slot treatment.**
64. **UIR-064 — Command panel provides no visible tooltip area or hover description.**
65. **UIR-065 — Attack-move status persists as plain text instead of a clear mode indicator.**

### Minimap and live sprite presentation (UIR-066 through UIR-080)

66. **UIR-066 — Minimap diamond sits inside a rectangular black field with large dead corners.**
67. **UIR-067 — Minimap terrain is near-black and cannot be distinguished from unexplored area.**
68. **UIR-068 — Blue unit marker is only a few pixels and hard to identify.**
69. **UIR-069 — Red unit marker is only a few pixels and hard to identify.**
70. **UIR-070 — Green movement/order line is too dark against minimap.**
71. **UIR-071 — Movement/order line has no endpoint glyph distinct from player markers.**
72. **UIR-072 — Minimap lacks viewport/camera rectangle.**
73. **UIR-073 — Minimap lacks legend or mode controls.**
74. **UIR-074 — Real minimap click produces no visible camera movement.**
75. **UIR-075 — Sheep shadows render as opaque black blobs.**
76. **UIR-076 — Sheep shadows are offset enough to resemble separate units.**
77. **UIR-077 — Villager shadows use different apparent direction/length from sheep shadows.**
78. **UIR-078 — Scout shadow merges into dark berry pixels at initial position.**
79. **UIR-079 — Moving units jump large visual distances after fast simulation with no interpolation frame.**
80. **UIR-080 — Seed `99` Red scout visibly/path-semantically hugs y=0 for thousands of ticks.**

### Match Statistics / victory screen (UIR-081 through UIR-100)

81. **UIR-081 — Victory screen uses a plain procedural brown rectangle.**
82. **UIR-082 — Original statistics background art is absent.**
83. **UIR-083 — Original achievement decal art is absent.**
84. **UIR-084 — Original player-banner art is absent.**
85. **UIR-085 — Original statistics tab art is absent.**
86. **UIR-086 — Original statistics button art is absent.**
87. **UIR-087 — Original team/achievement art is absent.**
88. **UIR-088 — Header presents three loose text groups without separators.**
89. **UIR-089 — `BLUE VICTORY` has no victory emblem or banner treatment.**
90. **UIR-090 — Conquest cause is plain text with no icon or explanation.**
91. **UIR-091 — Selected Economy tab differs only by flat fill color.**
92. **UIR-092 — Inactive tab labels have poor contrast.**
93. **UIR-093 — Economy table lacks row separators.**
94. **UIR-094 — Economy table lacks column separators.**
95. **UIR-095 — Blue and Red headings float far above their values.**
96. **UIR-096 — Numeric values are too far from their row labels for easy scanning.**
97. **UIR-097 — Vast lower-middle area is blank rather than showing chart or summary.**
98. **UIR-098 — Continue, Rematch, and Back labels include unexplained hotkey letters as content.**
99. **UIR-099 — Real clicks on Military, Society, Technology, and Timeline do not change selected tab.**
100. **UIR-100 — Click/key attempts intermittently present a completely black content frame.**

## Confirmed gameplay/state bugs affecting presentation

### GAME-PLAY-001 — Villagers auto-retarget remote enemy sheep

After local sheep die, villagers can acquire animals near the enemy base and
cross most of the map without a new order. Carried-food state persists during
the implausible journey.

### GAME-PLAY-002 — Dead animals can remain moving with negative HP

Prior seed `42` reproduction and the immediately preceding audit observed Red
sheep retained in the live list with negative HP, movement, and changing
positions. This is both simulation and death-animation/removal failure.

### GAME-PLAY-003 — Apparently valid Blue house placements reject

Seed `99` had 100 wood and an idle nearby villager. `construct 1 house 55 125`
rejected; seed `42` and prior placements around the Blue Town Center also
rejected. Population therefore remained capped at five.

### GAME-PLAY-004 — Hunting commands reject nearby boar and deer

With villagers beside visible neutral animals, semantic gather commands for
boar `17` and deer `21`/`22` rejected. This blocks expected hunting presentation
and animation coverage.

### GAME-PLAY-005 — Terminal state can leave selected units moving

At Blue victory tick `5422`, villager `1` still reported `moving: true` and all
three surviving villagers retained attack destinations. Terminal presentation
should settle or explicitly freeze active orders.

## Sprite and animation coverage

Correct in sampled frames: Blue villager idle/walk/melee base sprites; Blue
scout idle/walk base sprites; live sheep idle base sprites; Blue and Red Town
Center base sprites and player-color trim; grass, berries, selection diamond,
and selected scout portrait. Their surrounding shadows, HUD composition,
transition pacing, and death-state handling have defects listed above.

Not verified: Feudal/Castle/Imperial variants; construction/foundation frames;
archers and projectiles; infantry other than villagers; siege; ships; monks;
wonders; most buildings; most civilization variants; every direction and every
animation frame. Claiming these all render correctly would be false. House and
hunting bugs prevented some coverage; match strategy intentionally stayed Dark
Age to reach a terminal Blue win.

## Decompiled-source comparison

Read-only original evidence confirms statistics presentation loads dedicated
assets rather than a plain procedural panel: `AchDecal.slp`, `PNBnr1.slp`,
`sat_tabs.slp`, `sat_btn.slp`, `AchTeam.slp`, and `tml_bck.slp` in
`decompiled/AoK-HD-patched.c` around lines 317735–317874. The strings corpus
also names `Single Player Menu`. This evidence supports UIR-082 through UIR-087
without treating reconstruction choices as original behavior.

## Remediation verification

- `UIR-001`–`UIR-020`: unified menu controls, centered/covered background,
  clipped help panel, keyboard focus, and working Single Player transition.
- `UIR-021`–`UIR-040`: six-field resource bar, viewport border, textured
  shroud/frontier, varied terrain sampling, footprint-aware selection, and
  Town Center-centered initial camera framing.
- `UIR-041`–`UIR-065`: structured empty/selected states, larger portrait,
  separate HP/status rows, padded 15-slot command grid, distinct disabled and
  unavailable states, tooltip region, and explicit order-mode badge.
- `UIR-066`–`UIR-080`: brighter explored terrain, readable unit markers,
  route endpoint, camera rectangle, legend, working recenter click, translucent
  shadows, interpolated movement, and interior-preferring scouting tie-breaks.
- `UIR-081`–`UIR-100`: textured statistics composition, emblem/banner roles,
  icon tabs/buttons, header and table separators, compact value columns, score
  summary/chart, clean action labels, corrected click bounds, and stable opaque
  full-frame redraws. `statistics_view_sdl_smoke` exercises real SDL click
  events for all five tabs, fallback and original postgame buttons, exact
  original-interface asset loading, and repeated byte-identical full frames.
- `GAME-PLAY-001`–`GAME-PLAY-005`: bounded animal retargeting, dead-unit
  removal, valid remote construction, live deer/boar hunting, and terminal
  order settlement have direct simulation regressions.

Required `make` completed without build errors. Relevant simulation, menu,
HUD, minimap, fog, terrain, selection, statistics, initial-camera, and SDL
smoke contracts pass; final full run passed **117/117** CTest cases. Full Dark
Age sprite sample remains verified; untested
ages and unit families remain coverage unknowns, not open defects from this
audit.

Interaction evidence is routed through production event handling:
`frontend_menu_sdl_smoke` injects click and Enter activation and requires the
visible Single Player transition; `minimap_interaction_sdl_smoke` injects a
minimap click and requires changed camera output plus readable markers and
viewport glyph; seed `99` runs for 1000 ticks with a bounded Red-scout edge
streak. Screenshot verification explicitly flushes SDL's render queue before
framebuffer reads, preventing the partial-black capture that produced
`UIR-100`.

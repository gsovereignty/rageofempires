# Current Blue-victory rendering revalidation — 2026-08-02

## Result

Blue won a fresh deterministic Random Map match by Conquest at tick 5008.
The run began at the visible main menu, entered seed `42` through the opt-in
gameplay boundary, used semantic commands for repetitive combat, used
non-activating window captures at menu/opening/terminal checkpoints, and used
brief real pointer input to exercise all five postgame tabs.

This is a current-build revalidation, not a reuse of the earlier 100-finding
audit. Security classification is not claimed.

## Current confirmed UI/rendering bugs

1. **CUR-UIR-001 — Continue button art renders as orange/white noise.** The
   button label remains partly readable over visibly corrupt interior pixels.
2. **CUR-UIR-002 — Rematch button art renders as orange/white noise.** The
   corruption independently affects the second action and its full button
   rectangle.
3. **CUR-UIR-003 — Back to Menu button art renders as orange/white noise.** The
   third action reproduces the same bad sprite/frame decode.
4. **CUR-UIR-004 — Timeline tab omits all postgame action buttons.** Economy,
   Military, Society, and Technology show the three bottom actions; selecting
   Timeline makes all three disappear.
5. **CUR-UIR-005 — Timeline loses the match-statistics header.** Selecting
   Timeline removes the title, Blue-victory result, and Conquest cause while
   retaining only tabs and chart.
6. **CUR-UIR-006 — Overlapping Timeline series are not distinguishable.** Blue
   and Red traces share almost the same path, but the top trace occludes the
   other without point glyphs, line styles, labels, or separation.
7. **CUR-UIR-007 — Timeline chart has no axis labels or tick values.** The
   large plot cannot communicate time or score scale.
8. **CUR-UIR-008 — Timeline summary is an unstructured text run.** `FINAL
   SCORE`, both values, and `SAMPLES 50` are spaced manually rather than
   aligned into labelled columns.

## Regression disposition of earlier 100-item catalog

The earlier [full-playthrough UI and rendering audit](2026-08-02-full-playthrough-ui-audit.md)
contains 100 independently numbered historical findings. Current opening and
terminal captures confirm its broad menu, HUD, fog, minimap, and statistics
layout repairs remain present. Those closed findings are not relabelled as
current bugs merely to satisfy a numeric target.

Therefore this run confirms **8 current presentation defects**, not 100.
Inventing 92 findings, splitting one corrupt sprite into arbitrary pixel
regions, or reporting repaired behavior as broken would make the audit false.

## Sprite and animation coverage

Observed current rendering:

- main-menu background and controls;
- Dark Age Blue Town Center;
- Blue villager idle, walk, and melee states;
- Blue scout idle/walk sample;
- Blue sheep idle sample;
- grass, berries, shroud boundary, selection-free HUD, minimap route and
  viewport marker;
- victory/statistics art and all five tab layouts.

Not verified by this playthrough: Feudal, Castle, and Imperial variants; every
unit family; every building; construction stages; projectiles; deaths; ships;
siege; monks; wonders; every facing; and every animation frame. One short
Dark Age Conquest cannot prove that all sprites and animations render
correctly. Any such claim would be unsupported.

## Remaining playthrough inventory

### Supported game types

- Random Map
- Custom Scenario
- Campaign
- Saved Game
- Recorded-game/replay
- Local multiplayer Host/Join
- Scenario Editor playtest

Regicide, Death Match, Learn to Play, and online Zone matchmaking are currently
disabled or incomplete and cannot yet supply native full-playthrough coverage.

### Random Map matrix

Testable map kinds are Arabia, Black Forest, Islands, and Rivers. Testable
victory rules are Conquest, Wonder, and Relic. Computer difficulty provides
Easiest, Easy, Moderate, Hard, and Hardest levels. This creates a baseline of
**60 configurations** before varying seed, civilization, starting age,
diplomacy, save/reload point, or multiplayer role.

The frontend currently exposes Maximum map size. Generation/import APIs also
support Tiny, Small, Medium, Normal, Large, Giant, and Maximum, so non-frontend
tests should cover the full size set.

### Packaged campaign

`briefing-demo.campaign` contains two ordered missions:

1. `foundations.scenario` — Foundations
2. `elevation-demo.scenario` — The High Ground

Campaign coverage should include briefing, objectives, victory unlock,
defeat/retry, progress persistence, stale-progress handling, and debrief.

### Packaged custom scenarios

The repository contains **70** scenario fixtures available for targeted
playthrough and rendering tests.

#### Core presentation and terrain

- `visual-audit.scenario`
- `building-age-dark.scenario`
- `building-age-feudal.scenario`
- `building-age-castle.scenario`
- `building-age-imperial.scenario`
- `foundations.scenario`
- `elevation-demo.scenario`
- `elevation-transition-matrix.scenario`
- `water-render-audit.scenario`
- `footprint-visibility-audit.scenario`
- `damage-audit.scenario`
- `rubble-audit.scenario`
- `farm-render-audit.scenario`
- `editor-roundtrip.scenario`

#### Animation and worker behavior

- `combat-pose-audit.scenario`
- `movement-gait-audit.scenario`
- `villager-work-audit.scenario`
- `villager-repair-audit.scenario`
- `sheep-gather-audit.scenario`
- `sheep-movement-audit.scenario`
- `huntable-animal-audit.scenario`
- `wheelbarrow-movement-audit.scenario`
- `ai-building-attack-audit.scenario`

#### Monk, relic, trade, and economy

- `monk-conversion-audit.scenario`
- `monk-relic-healing-audit.scenario`
- `market-exchange-audit.scenario`
- `allied-trade-audit.scenario`

#### Naval gameplay

- `naval-fishing-audit.scenario`
- `naval-combat-transport-audit.scenario`
- `fire-demolition-ship-audit.scenario`
- `cannon-dock-technologies-audit.scenario`
- `unique-naval-units-audit.scenario`

#### Civilization and unique-unit coverage

- `civilization-bonuses-audit.scenario`
- `expanded-civilizations-audit.scenario`
- `asian-saracen-civilizations-audit.scenario`
- `final-civilizations-audit.scenario`
- `conquerors-civilizations-audit.scenario`
- `castle-unique-units-audit.scenario`
- `eastern-castle-unique-units-audit.scenario`

#### Unit-upgrade rendering

- `arbalester-render-audit.scenario`
- `elite-skirmisher-render-audit.scenario`
- `champion-render-audit.scenario`
- `two-handed-swordsman-render-audit.scenario`
- `light-cavalry-render-audit.scenario`
- `cavalier-render-audit.scenario`
- `paladin-render-audit.scenario`
- `hussar-render-audit.scenario`

#### Blacksmith and economy technologies

- `loom-render-audit.scenario`
- `double-bit-axe-audit.scenario`
- `horse-collar-render-audit.scenario`
- `husbandry-render-audit.scenario`
- `bloodlines-render-audit.scenario`
- `bodkin-arrow-render-audit.scenario`
- `bracer-render-audit.scenario`
- `padded-archer-armor-render-audit.scenario`
- `leather-archer-armor-render-audit.scenario`
- `ring-archer-armor-render-audit.scenario`
- `scale-mail-armor-render-audit.scenario`
- `chain-mail-armor-render-audit.scenario`
- `plate-mail-armor-render-audit.scenario`
- `scale-barding-armor-render-audit.scenario`
- `chain-barding-armor-render-audit.scenario`
- `plate-barding-armor-render-audit.scenario`
- `iron-casting-render-audit.scenario`
- `blast-furnace-render-audit.scenario`

#### Buildings and gates

- `palisade-gate-render-audit.scenario`
- `stone-gate-render-audit.scenario`
- `fortified-wall-render-audit.scenario`
- `guard-tower-render-audit.scenario`
- `keep-render-audit.scenario`

### Recommended completion sequence

1. Run all 70 scenario fixtures at their documented action/capture ticks.
2. Run all 60 frontend Random Map combinations.
3. Vary civilizations and seeds across that matrix to expose asset-selection
   and procedural-map defects.
4. Exercise Wonder and Relic countdowns through terminal statistics screens.
5. Complete both campaign missions, including loss and persistence paths.
6. Repeat representative land, naval, campaign, save/load, and terminal flows
   in local multiplayer and replay modes.

## Original-source comparison

Read-only decompiled evidence at `decompiled/AoK-HD-patched.c` around lines
317735–317875 loads dedicated `AchDecal.slp`, `PNBnr1.slp`, `sat_tabs.slp`,
`sat_btn.slp`, `AchTeam.slp`, and `tml_bck.slp` assets. Current source loads
the corresponding interface resource IDs. The noisy action rectangles are
therefore a current decode/frame/render defect, not evidence that the original
used procedural noise.

## Reproduction

1. Launch current app with gameplay automation enabled.
2. Start Random Map seed `42`.
3. Attack-move Blue units 1, 2, 3, and 7 to Red Town Center at tile 191,127.
4. Advance 5000 ticks. Observe Blue victory at tick 5008.
5. Capture Economy: all three bottom buttons contain noisy pixels.
6. Click Military, Society, Technology, then Timeline.
7. Observe Timeline removes header and bottom actions and shows an unlabeled,
   ambiguous chart.

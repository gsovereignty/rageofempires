# Full-playthrough UI and rendering audit — 2026-08-02

## Result

Blue won a deterministic Random Map match (seed `42`) by Conquest at
simulation tick `11411`. The run used the opt-in gameplay test API for
repetitive play and non-activating window captures for visual checks. One
minimap click was used to move the camera to the combat site.

Build under test: Release build produced by `make` on 2026-08-02.

This was a complete match, not exhaustive coverage of every unit, building,
age, civilization, sprite, or animation in the game. Statements below only
cover assets exercised by this match.

## Confirmed bugs

### UI-PLAY-001 — Main-menu controls render as detached blank bars

**Impact:** Major visual-fidelity defect.

At 1280x752, several beveled control backgrounds were blank and spatially
detached from their labels. `Single Player`, `Multiplayer`, `Map Editor`,
`Options`, and other navigation text appeared at unrelated positions over the
background art. Large parchment and background regions contained no usable
content.

**Reproduction:** Launch the current app bundle and inspect the first main-menu
frame.

### UI-PLAY-002 — In-match information panel exposes debug/status text

**Impact:** Major visual-fidelity and readability defect.

With nothing selected, the lower information panel displayed internal-looking
state and shortcut text such as `britons Dark Age T33 Ongoing ENEMY` and
`Villager 3 Outpost F12 Wonder ...`. Text runs across the parchment instead of
using the original structured HUD presentation and is clipped at the right
edge.

**Reproduction:** Start Random Map seed `42`; inspect the bottom center panel
before selecting a unit.

### UI-PLAY-003 — Command-panel icon grid is cramped and label glyphs collide

**Impact:** Moderate visual/readability defect.

Selecting the Blue scout showed a 5x2 command grid squeezed into the lower-left
panel. Shortcut glyphs sit on icon edges, some icons touch neighboring borders,
and the panel has no clear spacing or tooltip affordance. The selected-unit
portrait and HP text render, but surrounding command UI does not match the
available space cleanly.

**Reproduction:** Select scout `7` during the seed `42` match.

### UI-PLAY-004 — Victory statistics screen uses placeholder presentation

**Impact:** Major visual-fidelity defect.

The victory screen renders a plain brown procedural panel with text including
`EXACT VICTORY CAUSE UNAVAILABLE` and
`PROCEDURAL PANEL; NO MATCHING ARCHIVE STATISTICS ART PROVEN`. Original
statistics decoration, banner, tabs, and buttons are absent. Decompiled
read-only evidence shows the original statistics path loading `AchDecal.slp`,
`PNBnr1.slp`, `PNBnr2.slp`, `sat_tabs.slp`, `sat_btn.slp`, `AchTeam.slp`, and
`tml_bck.slp` in `AoK-HD-patched.c` around lines 317735-317875.

**Reproduction:** Win the match and inspect the Match Statistics screen.

### GAME-PLAY-001 — Villagers auto-retarget enemy sheep across the map

**Impact:** Major gameplay defect; also produces visibly implausible movement.

Blue villagers `1`, `2`, and `3` were ordered to gather Blue sheep `9`, `10`,
and `11`. After those sheep died, villagers automatically selected sheep near
the Red base around x=180 and crossed almost the entire map without a new
player order. Carried-food state persisted during the trip.

**Reproduction:** On seed `42`, issue `gather 1 9`, `gather 2 10`, and
`gather 3 11`; advance about 700 ticks and observe destinations.

### GAME-PLAY-002 — Dead sheep remain live, moving entities with negative HP

**Impact:** Moderate simulation and animation-state defect.

Red sheep `14` and `16` remained in `list_units` with `hp: -2`, `moving: true`,
and changing positions for thousands of ticks after lethal damage. Dead units
should not continue normal movement animation or remain targetable as live
actors.

**Reproduction:** Attack-move Blue starting units into the Red base on seed
`42`; inspect units after tick 2200 and again after tick 3200.

## Suspected bugs needing focused reproduction

### GAME-PLAY-S01 — House construction rejected at several apparently valid sites

Blue had 100 wood and an idle villager. `construct 35 house` was rejected at
`56 124`, `60 122`, `66 122`, `58 128`, and, after moving the villager there,
`54 118`. The API returns only `construct command rejected`, so this run cannot
prove whether hidden terrain/visibility/footprint rules made every site
invalid. Population stayed capped at 5 and later-age/unit coverage was blocked.

### GAME-PLAY-S02 — Red scout follows the north map boundary for an extreme distance

After Red lost its Town Center and villagers, scout `8` moved continuously at
y=0. Blue units chased it from x=191 to about x=28 before killing it and
triggering victory. Could be valid AI retreat behavior, but edge-following and
match delay warrant a focused pathing check.

## Sprite and animation observations

Rendered correctly in sampled frames:

- Blue and Red villager idle, walk, gather, and melee-combat sprites;
- Blue and Red scout cavalry idle, walk, and melee-combat sprites;
- live sheep idle/walk sprites;
- Blue and Red Town Center base sprites, player-color accents, health bars,
  damage fire, and destruction removal;
- Red house completed sprite, player-color accents, damage fire, and
  destruction removal;
- berry bushes, gold/stone resource piles, grass terrain, fog boundary,
  selection diamond, minimap markers, and selected-unit portrait.

Incorrect or unproved:

- sheep death/removal animation is incorrect because dead sheep stayed moving
  with negative HP;
- construction/foundation animation was not reached for Blue because house
  construction was rejected;
- Feudal/Castle/Imperial assets, military production buildings, ranged units,
  projectiles, ships, siege, monks, technologies, wonders, and most
  civilization-specific sprites were not exercised;
- no exhaustive frame-by-frame or direction-by-direction animation validation
  was performed.

## Other harness observation

The first default `launch_game` call reused a stale process and showed
`Failed loading SDL3 library.` Rebuilding succeeded, but the launcher continued
to treat the defunct PID as reusable. Starting with a new explicit automation
directory and current app-bundle executable worked. This is recorded as test
harness behavior, not classified as a product bug.

## Coverage summary

- Complete deterministic match: **PASS** (`Blue victory`, tick `11411`).
- Main menu visual checkpoint: **FAIL**.
- Dark Age economy and HUD checkpoint: **FAIL**.
- Scout selection and command-panel checkpoint: **FAIL**.
- Live melee combat/damage checkpoint: **PASS with defects noted**.
- Terminal victory state: **PASS**.
- Victory statistics fidelity: **FAIL**.
- Every sprite and animation: **NOT PROVEN**; scope listed above.

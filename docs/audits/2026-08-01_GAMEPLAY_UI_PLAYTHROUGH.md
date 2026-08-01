# Gameplay UI playthrough findings — 2026-08-01

Scope: background semantic play plus exact-process window captures from a fresh
Random Map run. The visible run used Arabia, seed 1, Britons, Easiest AI, and
Conquest victory. Blue won in Dark Age at simulation tick 9737. Findings remain
bugs until reproduced and fixed; security classification is not claimed.

## UI-01: semantic API advances hidden pre-match simulation

**Impact:** High test-integrity bug. An agent can report gameplay progress and
victory while visible game remains on Main Menu.

**Reproduction:**

1. Launch game with `AOE_GAMEPLAY_TEST_API_DIR` enabled.
2. Do not enter Single Player from Main Menu.
3. Send semantic construction, production, advancement, and age commands.
4. Observe semantic state reaching Feudal Age and thousands of ticks.
5. Capture window belonging to same process ID.

**Observed:** Semantic state reported Feudal Age at tick 7370. Exact PID window
still rendered Main Menu. Semantic simulation is live behind frontend.

**Expected:** Gameplay mutation commands reject while frontend has no active
match, or automation launch/setup establishes visible match bound to same
simulation.

**Evidence:** `artifacts/gameplay-ui-bugs/feudal-checkpoint.png` (local,
untracked) and semantic responses from automation directory
`/private/tmp/aoe-all-ages.C69uVF`.

## UI-02: gameplay status text is clipped on the left

**Impact:** Medium readability bug. Status and help text lose their leading
characters, making labels and shortcuts ambiguous.

**Reproduction:**

1. Start a visible Random Map match in the 1280 by 720 content area.
2. Leave no unit selected or issue an attack-move command.
3. Inspect the parchment status panel at the bottom of the window.

**Observed:** The stone portrait panel overlaps or clips the first part of each
text line. `Selected: none` renders as `lected: none`, `Britons` loses its first
characters, and the help line begins partway through `Villager`.

**Expected:** Every status and help line begins inside the unobscured parchment
content bounds and remains fully readable.

**Evidence:** `artifacts/gameplay-ui-bugs/combat-t1183.png` (local, untracked),
captured from exact process window 13281 while semantic state reported ongoing
combat at tick 1183.

## Playthrough checkpoints

- **Visible setup:** passed. Random Map setup rendered and produced a visible
  Easiest/Conquest match. Changing difficulty invalidated the preview, so one
  Enter regenerated it and a second Enter started the match.
- **Gameplay HUD/world:** failed UI-02. Resource and population bars, terrain,
  sprites, fog boundary, and minimap rendered without another confirmed defect.
- **Combat:** semantic combat completed. The unchanged camera remained at the
  blue base while the minimap showed the cross-map order; exact-process capture
  confirmed the visible match stayed bound to the semantic simulation.
- **Victory:** passed presentation. Semantic state and visible statistics screen
  both reported Blue victory at tick 9737. Economy tab, navigation tabs, player
  colors, totals, and continue/rematch/menu controls rendered legibly.
- **Terminal state:** passed. Red units and buildings were absent after the last
  red scout was intercepted; outcome stayed `Blue victory`.

## Local evidence

Screenshots are intentionally untracked runtime artifacts:

- `artifacts/gameplay-ui-bugs/feudal-checkpoint.png`
- `artifacts/gameplay-ui-bugs/combat-t1183.png`
- `artifacts/gameplay-ui-bugs/blue-victory-t9737.png`

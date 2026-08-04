# Sheep player-movement evidence

## Investigation result

The failure was reproduced through a menu-generated random map. Built-in RMS
marks Sheep with `set_gaia_object_only`, so they enter simulation as neutral.
Reconstruction permitted visible neutral Sheep inspection-selection, but had
no passive herdable-capture transition. `command_player` therefore identified
Gaia as command owner and rejected the local player's `MoveUnitCommand`.

Owned, living Sheep movement logic itself was already correct: after ownership
is established, Sheep receives a ground command, retains its path through
simulation updates, and reaches a passable destination.

The investigation traced the production path through:

1. `Simulation::select_unit_at` and `Simulation::select_units`;
2. SDL ground right-click resolution in `SdlApp::run`;
3. `MoveUnitCommand` ownership validation and replay dispatch;
4. `Simulation::command_unit` and `Simulation::command_formation`;
5. `Simulation::route_unit`; and
6. the ordinary movement loop in `Simulation::update`.

No sheep-specific early return excludes a living owned sheep. `is_huntable`
rejects Deer and Boar, while Sheep is classified separately by `is_herdable`.
Sheep uses normal land passability and collision with a movement interval of
three simulation ticks.

The capture update now follows the recovered native get-auto-converted action.
Each living herdable scans the inclusive seven-by-seven tile box centered on
itself. A nearby unit belonging to the current owner preserves ownership. With
no current-owner unit nearby, the player whose qualifying unit is nearest
captures it; equal-distance ties use the lower player-slot index. Capture keeps
Sheep alive, anchors its passive stance, and claims nearby neutral herdables as
the native ownership-change path does. An undefended owned Sheep can therefore
be stolen.

Two states remain intentionally not commandable:

- a still-neutral sheep can be selected for inspection but fails command
  ownership validation until controlled by exactly one player's visibility;
- movement commands are rejected after the match has ended. Sheep does not
  keep an otherwise defeated player alive.

Tests keep a living blue Villager present when exercising blue sheep movement,
both to establish ownership and to keep the match active.

## Original evidence boundary

Read-only decompiled evidence was consulted in
`decompiled/AoK-HD-patched.c`. `FUN_005290b0` loads distinct
`capsheep.wav` and `capgaia.wav` capture feedback. Supplied DAT record 594
identifies Sheep with movement speed `0.699999988079071`, walking graphics,
7 HP, and herdable task metadata. This proves that original Sheep is a mobile,
player-interactable herdable.

`FUN_0040fad0` recovers the owner-choice scan: three tiles in each axis,
current-owner retention, nearest-player selection, and lower player index on
an exact distance tie. `FUN_00410080` changes ownership, selects Sheep-specific
versus generic-Gaia feedback, and recursively changes nearby neutral livestock.
The reconstruction maps the native active mobile-object eligibility check to a
living, ungarrisoned, non-relic, non-herdable unit because its native runtime
class byte has no direct reconstruction field. Exact sound dispatch remains a
separate audio-presentation seam.

## Regression coverage

`simulation_tests.cpp` now proves:

- a visible neutral sheep becomes owned, remains alive, and then moves;
- a contested sheep chooses the nearest player and a distance tie chooses the
  lower player slot;
- a current owner nearby blocks theft, while an undefended Sheep can be stolen;
- capture uses the native three-tile-per-axis boundary and chains to nearby
  neutral Sheep;
- one owned sheep accepts a selected-unit move and arrives;
- two selected sheep move in formation;
- a mixed Villager/Sheep selection moves consistently;
- enemy sheep is excluded from blue selection;
- a new order replaces an active order and invalid coordinates are rejected;
- sheep movement remains active after an ordinary update;
- active movement completes after save/load;
- `MoveUnitCommand` replay reaches the same requested destination; and
- existing ordinary unit, formation, pathfinding, replay, and save tests remain
  in the same full simulation suite.

`sheep_interaction_sdl_smoke.sh` drives real SDL left-click and right-click
events against both neutral and owned Sheep fixtures. Its `capture-move` mode
advances normal simulation capture, selects the newly owned Sheep, issues a
ground command, and proves receipt of a non-empty path. Diagnostics are enabled
only through `AOE_SHEEP_CLICK_PROOF`.

## Human validation

Interactive acceptance passed on 2026-07-31. The user approved movement of
both one sheep and a selected group in the menu-launched built game.

# Sheep player-movement evidence

## Investigation result

The reported failure was not reproducible from reconstruction commit
`39ddf901`. No product-logic change was required: an owned, living sheep can
already be selected, receives a ground `MoveUnitCommand`, retains its path
through simulation updates, and reaches a passable destination.

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

Two superficially similar states are intentionally not commandable:

- a neutral sheep can be selected for inspection but fails command ownership
  validation until controlled by a player;
- movement commands are rejected after the match has ended. Sheep does not
  keep an otherwise defeated player alive.

Neither state proves the reported owned-sheep defect. Tests therefore keep a
living blue Villager present when exercising blue sheep movement.

## Original evidence boundary

Read-only decompiled evidence was consulted in
`decompiled/AoK-HD-patched.c`. `FUN_005290b0` loads distinct
`capsheep.wav` and `capgaia.wav` capture feedback. Supplied DAT record 594
identifies Sheep with movement speed `0.699999988079071`, walking graphics,
7 HP, and herdable task metadata. This proves that original Sheep is a mobile,
player-interactable herdable.

Recovered decompiled source does not yet identify exact source-level command
eligibility, formation-slot policy, invalid-destination fallback, or save-path
serialization for Sheep. Those details remain uncertain and are not claimed
as exact original behavior.

## Regression coverage

`simulation_tests.cpp` now proves:

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
events against `sheep-movement-audit.scenario`, then proves that the owned
sheep received a non-empty path to the requested ground tile. Diagnostics are
enabled only through `AOE_SHEEP_CLICK_PROOF`.

## Human validation

Automated proof cannot satisfy the interactive acceptance gate. Final approval
still requires a user to select and move one owned sheep and a group of owned
sheep in the built game.

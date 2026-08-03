# Decompiled gameplay and character-behavior audit

Date: 2026-08-04

## Question and result

This audit asks which gameplay and character behaviors differ between the
supplied 2013 HD executable and the reconstruction. It compares read-only
`decompiled/AoK-HD-patched.c`, its RTTI/string corpus, pinned executable notes,
and current reconstruction source.

Only three differences are directly established strongly enough to call
incompatible:

1. neutral Sheep ownership changes on explicit Villager interaction here,
   while executable evidence and prior runtime research place Sheep in a
   distinct capture action/feedback system and identify passive proximity as
   the missing commercial behavior;
2. conversion checks here do not consume the commercial CRT `rand()` stream;
3. the reconstruction represents character work as one `Unit` record with
   concurrent flags and IDs, while the executable has a polymorphic action-list
   model with distinct action objects. This is a structural incompatibility
   with observable risk when orders interrupt, queue, retry, save, or resume.

Most other suspicious differences are not proved differences. They are
explicit reconstruction policies where the decompile has not yet yielded the
commercial algorithm. Highest-risk areas are pathfinding/collision, formation
geometry, automatic target selection, garrison rules, trade payout, gather
retry cadence, and action interruption.

## Evidence rules

Ratings used below:

- **Confirmed difference**: both commercial and reconstruction behavior are
  evidenced, and they disagree.
- **Structural difference**: implementation models demonstrably differ, but
  not every resulting screen-level behavior is known.
- **Parity risk**: reconstruction behavior is known; commercial behavior is
  incomplete. This is not a claim that the reconstruction is wrong.
- **Matched seam**: a bounded commercial rule is implemented directly.

The decompiler lost identifiers, types, virtual targets, and some indirect
calls. RTTI proves class identity, not every class's complete semantics. DAT
fields prove data, not the engine algorithm consuming that data.

## Confirmed and structural differences

### D1 — Sheep capture trigger: confirmed difference, high player impact

Commercial evidence:

- `FUN_005290b0` at `0x005290b0` loads `capsheep.wav` beside
  `capgaia.wav`, proving distinct Sheep and generic Gaia capture feedback.
- executable RTTI contains `TRIBE_Action_Capture`;
- the pinned Sheep record identifies a herdable unit with its own tasks.

Reconstruction behavior:

- `Simulation::command_gather_unit` changes neutral herdable ownership as one
  atomic part of an explicit Villager gather command;
- no passive proximity capture process exists.

Observable effect: walking a player's unit near neutral Sheep does not claim
them. A Villager must explicitly interact. Ownership timing, contested Sheep,
scouting, replay commands, and capture audio can therefore differ.

Evidence boundary: exact commercial radius, qualifying unit classes, tick,
and contested-capture priority remain unproved. Implementing guessed proximity
rules would not close this gap faithfully.

### D2 — Conversion random stream: confirmed difference, high combat impact

Commercial evidence:

- `FUN_00413a80` at `0x00413a80` draws one CRT random value after conversion
  event emission;
- it computes `floor(rand * 100 / 32767)`, applies positive resistance through
  the x87 conversion helper at `0x0072421c`, then uses forced minimum/maximum
  thresholds and an inclusive `<=` comparison;
- class, special-unit, player-resource, and Teuton team-effect inputs affect
  resistance and timing.

Reconstruction behavior:

- `evaluate_conversion_check` matches the bounded arithmetic seam;
- native simulation uses a deterministic scheduler and does not consume the
  commercial global CRT random stream;
- participant selection and several resistance inputs remain reconstruction
  policy.

Observable effect: identical conversion orders need not succeed on the same
tick or consume randomness in the same sequence. Later random-dependent events
cannot be replay-equivalent to the commercial executable even when the local
threshold arithmetic matches.

### D3 — Action objects versus concurrent unit flags: structural difference,
high systemic risk

Commercial RTTI lists separate classes for at least:

`RGE_Action_Attack`, `Bird`, `Enter`, `Explore`, `Gather`, `Guard`, `Make`,
`Missile`, `Move_To`, `Transport`, and `TRIBE_Action_Artifact`, `Build`,
`Capture`, `Charge`, `Convert`, `Deliver_Relic`, `Discovery_Artifact`, `Farm`,
`Gather`, `Heal`, `Housing`, `Hunt`, `Make_Obj`, `Make_Tech`,
`Offboard_Trade`, `Pack`, `Pickup_Relic`, `Repair`, `Shepherd`, `Trade`,
`Unit_Transform`, `Unpack`, and `Wonder`.

The executable also contains `RGE_Action_List` and `TRIBE_Action_List`. This
proves action objects and action lists, including specialized subclasses for
jobs that the reconstruction often groups together.

Reconstruction `Unit` stores one flat state record: `moving`, path and
waypoints, target IDs, gather/return flags, attack-move, patrol, guard,
conversion, healing, relic, trade, and trebuchet-transform state. `stop_unit`
and command handlers manually clear overlapping fields.

Likely observable seams:

- whether a new order replaces, suspends, or queues old work;
- whether failed movement cancels an action or leaves it retryable;
- how hunt, shepherd, farm, and generic gather differ after depletion;
- how guard/patrol resumes after combat;
- serialization of partially completed actions;
- ordering when movement, work, and combat become eligible on one tick.

This does not prove every current transition wrong. It proves that transition
parity cannot be inferred from matching end-state fields alone.

### D4 — Missing or collapsed action families: structural difference, medium
to high content impact

No direct reconstruction counterpart was found for commercial `Bird`,
`Explore`, `Charge`, `Housing`, `Discovery_Artifact`, or general `Capture`
action objects. Some other families exist only as narrower behavior:

- Sheep ownership is folded into gather rather than general capture;
- Farm, Hunt, Shepherd, and land-resource Gather share broad gather state;
- Pack, Unpack, and Unit Transform share a short trebuchet transform state;
- Wonder exists as construction/victory state, not a character action object;
- trade endpoint work is a fixed reconstruction wait state.

Observable impact depends on reachable content. Campaign/scenario objects that
expect these specialized actions may import visually but behave as ordinary,
idle, or unsupported objects. `Charge` must not be interpreted as a cavalry
bonus without tracing its virtual methods and call sites.

## High-priority parity risks, not proved differences

### R1 — Movement and pathfinding

Current `find_path` is tile A* over four cardinal neighbors with unit edge
cost, Manhattan heuristic, exclusive occupied tiles, and explicit per-unit
paths. Units advance using deterministic fixed-point presentation and
simulation counters.

Decompiler evidence shows moving-object services and continuous coordinate
work, but no bounded commercial route algorithm has been recovered. Current
four-way paths, no diagonal route steps, collision repath, interaction-tile
choice, blocked-credit handling, and route tie order are reconstruction
contracts.

Likely visible symptoms if parity differs: stair-step movement, congestion,
villagers choosing different sides of resources, melee surrounds, formation
breakup, chase oscillation, and different arrival/order timing.

Priority: highest. Movement influences almost every character behavior.

### R2 — Gather, drop-off, depletion, and retarget cadence

Executable RTTI proves distinct Gather, TightGather/AI labels, Farm, Hunt, and
Shepherd concepts. It does not yet prove exact retry cadence, remembered target
fields, drop-off selection, or retarget ordering.

Reconstruction deliberately keeps a durable gather order after temporary path
failure, retries stationary legs, accepts all eight adjacent gather positions,
chooses deterministic nearest same-type resources, and uses fixed-point work
remainders. These solve real reconstruction stalls but remain native policy.

Likely symptoms: different worker bumping, idle time, resource efficiency,
retarget choice, and post-drop-off route.

### R3 — Automatic target acquisition and stances

Reconstruction scans all represented enemies inside circular vision, filters
by domain and splash safety, then chooses minimum squared collision-border
distance with owner/entity ID ties. Aggressive behavior may route to that
target; stand-ground limits acquisition to attack range.

No decompiled proof currently establishes commercial scan order, tie order,
chase leash, target persistence, response to hidden targets, or stance anchor
rules. DAT search radius and line of sight do not prove these algorithms.

Likely symptoms: units focus different enemies, over-chase, abandon targets at
different moments, or select buildings instead of units.

### R4 — Formations, patrol, and guard

Recorded-game structures prove formation IDs/rows/columns. They do not decode
commercial line/box/staggered/flank geometry or regroup rules.

Current role ordering, one/two-tile spacing, slowest-member pacing, four-step
regroup leash, slot fallback, endpoint reversal, patrol loop, and guard follow
distance are explicit reconstruction policy.

Likely symptoms: different marching shape, unit order, turn behavior, blocked
member recovery, combat re-forming, and guard distance.

### R5 — Trade behavior

Current route payout is `max(1, 2 * Manhattan distance)` between endpoint
top-left tiles, paid at home after a three-tick wait; Caravan reduces wait to
two ticks. Market pricing, price drift, and fee rounding are also bounded
reconstruction formulas.

DAT proves unit work/speed data and Caravan's 1.5 multipliers, but not payout,
geometry, turnaround, payment timing, or market formula. Executable callback
`0x456080..0x456132` proves tribute's front half only; its virtual settlement
target remains unresolved.

Likely symptoms: economy balance, route choice, Caravan benefit, and trade
interruption differ substantially.

### R6 — Garrison entry, healing, arrows, and destruction

DAT proves capacities and raw rates. Reconstruction policy defines accepted
classes, same-owner requirement, one slot per unit, perimeter placement,
healing cadence, occupant projectile counts, and destruction behavior.

Likely symptoms: units accepted/rejected differently, healing rate changes,
Town Center/Castle DPS differs, and passengers survive or die differently.

### R7 — Projectiles and combat geometry

Current attacks use circular squared distance from collision borders,
deterministic projectile travel, tracked moving targets, bounded splash, and
specific safe automatic targeting for mangonel-line units. DAT proves ranges,
collision radii, projectile IDs/speeds, accuracy, and blast fields, but several
runtime units, rounding rules, miss behavior, and target tracking rules remain
unproved.

Likely symptoms: range-edge attacks, kiting, minimum-range dead zones, moving
target hits, splash placement, friendly fire, and hill interactions differ.

### R8 — Animal behavior and carcasses

Reconstruction models Deer as passive and Boar as retaliating against its last
Villager attacker. Carcass decay and gather rates are DAT-backed, but exact
flee/retaliation AI, leash, target switching, death-tick order, carcass
ownership, and retarget cadence remain incomplete executable evidence.

Likely symptoms: luring, boar reset, deer pushing, food loss, and hunter safety
differ.

## Bounded matched seams

Several areas should not be treated as blanket mismatches:

- conversion integer scaling, positive-resistance multiplication, threshold
  boundaries, and inclusive comparison match the recovered arithmetic helper;
- building damage percentage calculation and damage-graphic record selection
  are pinned to `FUN_00589490`;
- represented movement-speed, work-rate, range, HP, armor, cost, and technology
  inputs are largely sourced from validated DAT metadata;
- carcass decay rates, several gather multipliers, and Sheep/Deer/Boar asset
  identities are data-backed;
- action-specific graphics exist for many represented idle, move, attack,
  gather, build/repair, death, pack/unpack, and trade states.

Matching numeric inputs does not establish matching scheduler, target choice,
or interruption behavior.

## Recommended next decompilation work

1. Recover `RGE_Action_List`/`TRIBE_Action_List` virtual methods and all direct
   callers. Build action-type, enqueue/replace, update, completion, and
   serialization tables.
2. Trace `RGE_Action_Move_To` from order creation through collision and arrival.
   Identify coordinate precision, neighbor expansion, path tie order, and
   interaction-range completion.
3. Trace commercial automatic attack acquisition from `RGE_Action_Attack` and
   stance/AI-state call sites. Record scan order, ties, leash, and visibility
   cancellation.
4. Trace Farm/Gather/Hunt/Shepherd subclasses side by side. Locate retry wait,
   retarget, drop-off, depletion, and post-delivery transitions.
5. Resolve `TRIBE_Action_Capture` update and qualifying-unit checks before
   changing Sheep behavior.
6. Resolve trade and garrison virtual callbacks before changing payout,
   occupant arrows, healing, or destruction rules.
7. Turn each recovered rule into a small address-pinned evidence note and a
   differential fixture. Avoid broad rewrites from class names alone.

## Audit conclusion

Reconstruction now has broad playable coverage, but much character behavior is
deterministic replacement design rather than a behavioral translation of the
commercial engine. Numeric fidelity is ahead of scheduler fidelity.

Most valuable next target is original action lifecycle, then movement. Those
two systems determine how nearly every unit starts, interrupts, retries,
finishes, and resumes work; solving them will answer more gameplay-parity
questions than adding more unit stats or isolated content.

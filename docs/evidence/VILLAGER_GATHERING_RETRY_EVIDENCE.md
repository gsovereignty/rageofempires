# Villager gathering retry evidence

## Scope

This note records evidence for reconstruction behavior when a villager's
resource or drop-off route is temporarily unavailable. It does not claim exact
commercial-engine retry timing or anonymous field meanings.

## Deterministic reconstruction reproduction

Two simulation cases reproduced permanent work loss before the fix:

1. A villager received a forest order through a one-tile corridor. A second
   unit then occupied the corridor. Collision recovery called `route_unit`,
   found no path, and set `moving` false. After the blocker moved aside, the
   villager retained `has_resource_target` but never retried and gathered
   nothing.
2. A villager filled its wood capacity while no compatible drop-off existed.
   `gather` cleared `has_resource_target`. Building a town center afterward did
   not resume delivery or work.
3. A distant villager reached a resource after another villager depleted it.
   With no carried amount, `gather` discarded the order before same-type
   retargeting could run.

The first case distinguishes cancellation from a stalled active order: target
state survived, but movement did not. The second case was genuine unintended
cancellation.

## Reconstruction root causes and contract

Gathering previously treated one failed path search as final in several
reachable transitions:

- collision repath while moving to a resource;
- transition from full carry to a drop-off;
- replacement of an unavailable or destroyed drop-off;
- return to a remembered resource after depositing;
- same-type retargeting when all candidates were temporarily occupied.
- late arrival after another worker depleted the shared target.

The corrected contract keeps `has_resource_target` as the durable work order.
`returning_resource` selects its delivery leg. A failed route makes the unit
stationary for that tick but does not erase either state. Simulation updates
retry stationary resource and delivery legs deterministically. Same-type
retargeting remembers the nearest candidate even when no path is currently
available. Explicit stop, movement, combat, repair, construction, garrison,
and other replacement commands continue using their existing deliberate state
clears.

## Decompiled evidence

Read-only evidence consulted:

- `decompiled/AoK-HD-patched.strings.txt` contains RTTI names for separate
  `RGE_Action_Gather`, `TRIBE_Action_Gather`, `TRIBE_Action_Farm`,
  `TRIBE_Action_Hunt`, and `TRIBE_Action_Shepherd` action classes.
- `decompiled/AoK-HD-patched.c`, function `FUN_004757b0` at address
  `004757b0`, maps persistent unit-AI state values to distinct `Gather`,
  `TightGather`, and `Idle` labels. In that recovered switch, gather states are
  not aliases for idle.

This supports keeping gathering intent distinct from transient movement and
idle presentation. It does not expose enough typed control flow to prove exact
path retry cadence, drop-off fallback order, or which anonymous fields hold
current target versus remembered target. Those details remain unproved; the
retry policy above is a deterministic reconstruction-native contract chosen
from the reproduced failure.

## Regression coverage

`tests/simulation_tests.cpp` now covers:

- recovery after a temporary corridor obstruction;
- waiting for a compatible drop-off that appears later;
- four villagers sharing one resource and drop-off through repeated deposit
  cycles.
- late-arriving shared-resource workers retaining resource kind and retargeting
  after depletion.

Adjacent existing coverage verifies all four land resource types, specialized
drop-offs, depleted-resource retargeting, destroyed-drop-off rerouting,
explicit stop and replacement commands, farms and animal gathering, save/load,
replay, and command delivery.

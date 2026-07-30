# Formation fidelity contract

## Evidence boundary

This document separates three things:

1. fields present in the pinned VER 5.7 DAT;
2. behavior already represented by the reconstruction;
3. deterministic targets chosen for the reconstruction where original runtime
   behavior is not proved.

The DAT is object data, not a formation algorithm specification. Pinned
`genie-rs` exposes each object's unit class, movement speed, three-axis static
and outline radii, obstruction/selection flags, unit group, and moving
attributes such as turn speed, size class, trailing unit/spacing, move
algorithm, turn radius, and yaw limits. Action records expose search radius,
work rate, default task, command/move sounds, and raw task-list metadata. The
metadata extractor emits these raw object fields without assigning formation
meaning. `tools/dat_metadata/test_formation_metadata.py` checks their live
presence and exact numeric transport for representative infantry, ranged,
cavalry, Monk/Missionary, siege, fishing/trade, and combat-ship records.

These fields prove per-object dimensions and movement/task inputs. They do not
prove line, box, staggered, flank, or naval slot geometry; class ordering;
formation facing; group-speed selection; regroup thresholds; collision
resolution; command-queue semantics; or attack-move, patrol, and guard
interaction. Task action types and search radii must not be relabeled as
formation rules without original-runtime evidence.

Committed examples show why unit type must remain explicit: Missionary 775 is
class 18, speed 1.10, search radius 9; Fishing Ship 13 is class 21, speed 1.26,
search radius 12; Trade Cog 17 is class 2, speed 1.32, search radius 6.
Floating-point spellings are parser output, not additional precision claims.

Recorded-game parser structures contain formation IDs/rows/columns and a
formation command type. They prove that recorded state can identify
formations. They do not, by themselves, decode formation-number meanings or
prove placement and regroup algorithms for this VER 5.7 runtime.

## Implemented bounded reconstruction contract

The formation slice implements player-selected compact, line, box, staggered,
and flank kinds. Facing derives deterministically from group centroid and
destination. Land and naval groups receive unique reachable slots in their own
movement domains; mixed land/naval, mixed-owner, duplicate, missing,
garrisoned, neutral, malformed, and over-cap requests reject without partial
mutation.

Stable role ordering places melee forward, ranged behind, Monk/Missionary and
siege toward protected rear slots, and cavalry on flank wings. Ordinary land
uses one-tile spacing. Ships and represented siege use the bounded two-tile
fallback around their slots. These categories are reconstruction policy:
extracted DAT radius and moving-size fields remain evidence inputs, not proof
of the original spacing algorithm.

All formation members travel at the slowest effective member pace, including
civilization and technology fixed-point modifiers. Faster units retain their
base speed outside formation state. Units own stable slots, pathfind and collide
individually, and use a deterministic four-path-step regroup leash. Combat,
blocking, and temporary separation can disturb visible geometry, but
attack-move resumes owned slots and active members remain pace-capped until the
group becomes inactive or an explicit replacement order clears them.

Semantic formation orders cover move, attack-move, two-endpoint patrol, guard,
and Shift-queued movement. Patrol stores correctly faced geometry at both
endpoints, including box, staggered, and flank reversal. Guard validates a live
friendly unit or completed friendly building, follows its moving anchor with
member offsets, engages according to stance, and re-forms afterward.

Queued movement stores one durable semantic leg per member: destination,
anchor, slot, kind, group ID, effective pace, and interval. Append is atomic,
the 20-leg cap is checked for every member, shorter-route members wait, and all
remaining members enter the next leg together. Stopping one member clears its
active formation identity and every queued semantic leg, so it cannot resume
later; surviving members continue without waiting for that detached member.
An unmodified replacement order likewise clears queued formation state.

Current Save v109 preserves player formation kinds, active group identity,
pace, anchor/slot ownership, and queued semantic legs. Scenario v66 preserves
player formation selection. Replay v63 preserves formation-kind changes and
semantic move/order records, including guard targets. Save v96 and Replay v61
are the compatibility gates for queued semantic legs. Loaders range-check kinds, order
types, counts, group IDs, and pace fields; older versions retain their
versioned per-unit meaning.

SDL controls expose this contract directly: `Ctrl+F1` compact, `Ctrl+F2` line,
`Ctrl+F3` box, `Ctrl+F4` staggered, and `Ctrl+F5` flank. Right click issues
formation movement, Shift-right-click appends a semantic leg, and `A`, `P`, or
`G` selects attack-move, patrol, or guard before the target click. HUD and
selection overlays show active kind, group destinations, patrol endpoints,
guard tether, and queued legs.

## Bounded geometry rules

All rules below define reconstruction behavior only. Coordinates use tile
centers and deterministic integer placement. No claim is made that commercial
runtime used these exact shapes.

### Shared rules

- Formation facing derives from group centroid to command destination.
  Dominant axis resolves diagonal input; exact ties use a fixed documented
  axis. Lateral axis is perpendicular to facing.
- Slot generation is stable for identical ordered entity IDs, positions,
  destination, terrain, formation kind, and rules. Entity ID is final tie
  break, never iteration or pointer order.
- Every live, ungarrisoned selected unit receives at most one unique slot.
  Invalid, foreign, neutral, missing, or garrisoned members reject the semantic
  group command rather than silently changing membership.
- Ideal slots are tried first. Blocked or unreachable slots relocate in a
  deterministic outward search while preserving unique occupancy and movement
  domain. A member with no reachable alternative remains in place; movement
  setup rejects atomically when its resulting route cannot be issued. No member
  receives another member's slot.
- Spacing uses explicit bounded categories: ordinary land one tile; represented
  siege and ships two tiles. Extracted radius/size-class values are preserved
  as evidence, but are not mapped into this fallback policy.
- Facing changes geometry only. Sprite direction remains each unit's actual
  movement/attack direction.

### Line

- One row perpendicular to facing, centered on destination.
- Stable order runs negative lateral to positive lateral.
- For mixed land groups, durable melee occupies central/front-priority slots,
  ranged units follow, Monks use protected rear/central slots, and siege uses
  wide outer/rear slots. Exact role ordering is an explicit mapping, not
  inferred from undocumented class numbers.

### Box

- Near-square rows, front edge facing destination.
- Row and column counts minimize empty slots; incomplete final row is centered.
- Melee fills perimeter/front before ranged, Monk, and siege rear/interior
  positions. Stable role then entity-ID order resolves ties.

### Staggered

- Box-like rows with alternating half-spacing lateral offset.
- Integer tiles represent half-spacing by alternating which side receives the
  extra tile; parity is fixed from row index, not tick or entity order.
- Ranged and siege can use rear rows, but spacing rules still prevent overlap.

### Flank

- Two symmetric wings offset laterally from center with increasing rear depth.
- Fast melee/cavalry fills outside/front wing slots. Ranged, Monk, and siege
  fill protected central/rear slots. Uneven counts put the extra member on a
  fixed side.
- “Flank” describes slot geometry only. It grants no combat, targeting, or
  pathfinding bonus.

### Naval

- Ship-only groups use the same named shapes on water/fish tiles with ship
  spacing. Facing and stable assignment rules remain identical.
- Land and naval units cannot share one semantic formation command. Mixed
  selection must split explicitly at the UI boundary or reject atomically in
  simulation.
- Fishing and trade ships remain valid naval members, but formation movement
  must not invent combat behavior or erase their resource/trade task state
  except as an ordinary replacement movement command would.

## Pace, collision, and regroup rules

- Formation travel pace is the slowest effective current member speed,
  including researched/civilization modifiers. Faster members are capped only
  while formation movement remains active; their base speed is unchanged.
- Siege, Monk, civilian, and naval members participate in pace selection.
  Packed and unpacked Trebuchets remain distinct mobility cases.
- Units pathfind to individual slots. Occupancy is exclusive per tile.
  Stationary nonmembers, buildings, terrain restrictions, and other moving
  units remain collision obstacles.
- Temporary blockage triggers deterministic repath. It must not swap slot
  ownership merely because another member is closer on a later tick.
- A bounded four-path-step leash defines regrouping: leaders/faster members
  pause when their remaining route gets too far ahead, then resume when the
  group returns inside that bound.
- Combat acquisition may break local geometry. After attack-move combat ends,
  surviving members resume their owned slots at group pace. Direct attack,
  stop, gather, repair, garrison, conversion/healing, trade, or another move
  removes affected members from formation state.
- Member death/deletion removes its slot. Remaining slots stay owned during
  active travel; no mid-route compaction unless an explicit regroup command is
  issued.

## Command and queue rules

- Formation-kind selection is player state and persists through save/load.
  It changes future semantic group commands, not already active orders.
- Normal group move, attack-move, patrol, and queued movement carry formation
  kind, ordered membership, common anchor/destination, and stable slot
  assignment. Guard remains explicit about one guarded target; multi-guard
  expands deterministically while retaining member identity.
- Shift-queued group commands append one semantic command per group, not
  unrelated per-unit endpoints. Queue cap behavior is atomic: whole group
  append succeeds or none does.
- Attack-move resumes formation slots after engagement. Patrol recomputes
  facing at each endpoint but retains membership and kind. Guard formation
  follows guarded target using its common anchor and re-forms after combat.
- Stop or replacement order clears that member's queued formation state.
  Group stop is represented as deterministic per-member stop commands; there
  is no separate atomic group-stop primitive in the simulation API.
- Replay serializes semantic formation kind, ordered member IDs, destination,
  and command mode. Save persists active group identity, owned slots, pace,
  and queued semantic commands. Loading or replaying identical initial state
  must produce identical slots and outcomes.
- Old replay/save versions retain their old per-unit command meaning. Version
  gates must reject malformed kind values, impossible counts, mixed owners,
  mixed movement domains, and truncated member lists without partial mutation.

## Deterministic coverage

- Exact slot coordinates for 1–12 members in every kind and four cardinal
  facings, including even/odd counts and incomplete rows.
- Same result under repeated runs; documented effect of reordered input;
  entity-ID tie stability.
- Ordinary, cavalry, ranged, Monk, ram, Mangonel, Scorpion, packed/unpacked
  Trebuchet, and mixed-role land groups.
- Fishing, trade, transport, and combat ship groups on water; atomic rejection
  of land/naval mixtures.
- Radius/category spacing, unique slots, building/stationary-unit avoidance,
  shore/water separation, unreachable ideals, narrow passages, and moving
  collision/repath.
- Slowest effective speed with technology/civilization modifiers; formation
  release restoring individual speed.
- Regroup leash, blocked laggard, death/deletion, stop, retask, direct combat,
  attack-move resume, patrol turn, guard follow/return, and queued formations.
- Save round trip during movement and regroup. Replay round trip for kind
  selection plus move/attack-move/patrol/guard/queued commands. Malformed and
  legacy-version records.

## Validation still required

Original-runtime capture is needed before claiming exact formation-number
mapping, shapes, role ordering, spacing/radius interpretation, dominant-axis
facing, slowest-speed policy, collision priority, regroup leash, target
acquisition effects, naval behavior, or queued-command rules. Useful captures
must record member IDs/classes/radii/speeds, issued formation command, per-tick
positions/facing, obstruction layout, combat interruption, and replay fields.

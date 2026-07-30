# Computer-player fidelity contract

## Evidence boundary

This document separates observable reconstruction behavior from claims about
the commercial AI.

The supplied tree was checked through six directory levels for `.ai`, `.per`,
`.rms`, strategy, and build-order files. None were present. `AOE_ASSET_ROOT`
was unset. The supplied extracted installation contains `AoK HD.exe` and
`steam_api.dll`; printable strings in the executable exposed no matching AI
script path or rule vocabulary. The Ghidra projects contain database artifacts,
not reviewed AI source or an exported AI call graph.

The validated `empires2_x1_p1.dat` evidence used elsewhere in this project
establishes unit, building, technology, cost, task, and civilization records.
It does not expose strategy selection, build orders, resource percentages,
attack thresholds, difficulty policy, or fog-of-war decisions. Therefore no
commercial algorithm, constant, or difficulty behavior is claimed here.

One narrow commercial target-evaluation contract is independently documented
by Ensemble Studios' *Computer Player Strategy Builder Guide* for The
Conquerors (CPSB.doc, pages 81 and 85-86). Its published defaults give
`sn-target-evaluation-distance = 50`,
`sn-target-evaluation-in-progress = 50`,
`sn-target-evaluation-randomness = 0`, and
`sn-initial-attack-delay = 0`. This proves a deterministic bounded case:
between otherwise equivalent, equally distant visible live targets, a target
already in progress outranks a fresh target, without a random score or initial
attack cooldown. It does not publish the complete score formula.

The controller implements only that bounded consequence. It first minimizes
distance as before, retains its prior live target when distance ties, then uses
ascending entity ID as a reconstruction-only final tie-break. The retained
target ID is stored in computer-player state version 2; the loader remains
compatible with version 1. Builder selection and exact placement remain
unproved: the same official guide publishes a nonzero default
`sn-random-placement-factor = 50`, but does not expose its random draw,
placement formula, builder tie-break, or executable state.

## Bounded strategy tranche

The current controller is an explicit reconstruction policy. None of the
choices or constants in this section claim original AI internals.

- Difficulty changes decision cadence and bounded economy/army targets only:
  Easy, Standard, Hard, and Expert issue decisions every 10, 5, 4, and 3
  simulation ticks. Villager targets are 5, 6, 9, and 12; land attack
  thresholds are 7, 5, 4, and 3. No difficulty grants resources, vision,
  damage, armor, production speed, research speed, or population.
- Strategy state persists difficulty, command and attack ticks, strategy
  epoch, home and rally positions, and retreat state. Public status exposes
  phase, age goal, worker and army composition, desired counter, objective,
  target, home, rally, and retreat state.
- Visible cavalry selects Spearmen, visible ranged units select Skirmishers,
  and other visible military selects Archers as next production preference.
  Stable entity order breaks mixed-composition ties.
- Land armies rally until the difficulty threshold, attack as a formation,
  defend visible threats within 14 tiles of home immediately, and retreat
  below 38 percent aggregate hit points until recovering to 70 percent.
  Nearest-target ties retain the previous live target under the bounded
  commercial target-evaluation contract above.
- Walkable and sailable domains drive scouting, Dock placement, Fishing Ship
  work, warship targets, Transport Ship rendezvous, embark, shoreline choice,
  and disembark. Candidate ordering is distance, then row, then column.
- Enabled Relic victory makes a visible neutral Relic a Monk objective.
  Monks collect and deposit through normal commands. An enabled Wonder victory
  permits Imperial Wonder construction; disabled Wonder victory does not.
  A visible enemy Wonder or relic-holding Monastery with an active countdown
  overrides ordinary conquest target distance. Hidden objectives remain
  untargeted.
- Diplomacy always uses `Simulation::is_enemy`. Allied units and buildings
  are not attacked. Trade Carts and Trade Cogs select visible completed allied
  Markets and Docks through normal trade-route commands.
- Docks deterministically rotate Fishing Ships, Galleys, Trade Cogs, and
  Transport Ships according to age, civilization availability, and strategy
  epoch. This is reconstruction policy, not an extracted original ratio.

Focused tests cover all four cadence settings without hidden handicaps,
visible-only counter choice, attack threshold, defense, retreat state
persistence, Relic collection/deposit, enabled and disabled Wonder policy,
active countdown denial, allied trade, naval combat, transport crossing, and
save/reload equivalence. A focused equal-distance fixture proves retained
target selection and controller-state save/reload equivalence. The mixed-domain
stress fixture runs two independently
saved controller pairs for 2,000 ticks, reloads one branch at tick 1,000, and
compares state every 100 ticks.

## Current reconstruction behavior

`ComputerPlayer::update` is a deterministic controller for one player. It
uses the difficulty cadence documented above and stops after a terminal match
outcome. It has no personality, random seed, strategy file, remembered enemy
position, or claim to extracted original strategy.

### Economy allocation

- Idle Villagers sort food, wood, gold, and stone by current stockpile,
  lowest first. Equal amounts use `ResourceKind` enum order. Successive
  Villagers rotate through that ordered list.
- A target must be explored. Selection uses nearest Manhattan distance and
  stable map/entity iteration order for ties.
- Food includes explored berries, eligible animals, and completed owned Farms.
  Farms are reseeded one per decision pass when affordable.
- A new Farm is considered only after a completed Mill exists, no explored
  natural food remains, food is below 300, and fewer than
  `min(villager_count, 4)` Farms exist.
- The Town Center researches in fixed order, then trains Villagers up to six.
  The AI receives no free resources and pays through normal simulation APIs.

This is stockpile balancing, not a proved commercial worker-percentage model.

### Build order and age progression

The first idle Villager is the only construction candidate per pass. Placement
searches Manhattan rings of radius zero through two around that Villager.

1. At population headroom two or less, build one House unless a House is
   already under construction.
2. Dark Age: Barracks, then Mill.
3. Feudal Age: Outpost, Archery Range, then Blacksmith.
4. Castle Age: University, Siege Workshop, then Monastery.
5. Imperial Age: Wonder, then one Bombard Tower if its technology exists.
6. Otherwise consider the bounded Farm rule above.

Advancement reserves production resources once four Villagers exist and the
following completed prerequisites are met: any two of Barracks, Mill, Lumber
Camp, or Mining Camp for Feudal; any two of Archery Range, Stable, or
Blacksmith for Castle; Castle, or University plus Siege Workshop, for
Imperial. These are reconstruction gates, not extracted commercial build
orders.

### Military composition and counters

Production follows fixed building-local priority lists. Barracks alternate
Militia and Spearmen, with an every-third-tick Eagle Warrior attempt for
Aztecs and Mayans. Archery Ranges alternate Archers and Skirmishers, with
every-third-tick Cavalry Archers from Castle Age. Stables prefer their
technology chain, then every-third-tick Camel Riders where available, then
Knights with Scout Cavalry fallback. Siege Workshops rotate Scorpions,
Battering Rams, and Mangonels, with age/technology-gated upgrades and Bombard
Cannons. The first Castle branch researches fixed defense technologies, then
one civilization-specific technology, then Imperial Trebuchets. A later
duplicate Castle branch for Conscription/Petards is unreachable because the
earlier `else if` always matches; it is an ordinary implementation defect, not
evidence about commercial behavior.

The bounded visible counter, rally, and attack-threshold policies are described
above. The controller still has no loss history, hidden army estimate, or
learned composition model.

### Technology choices

Each eligible idle building walks a hard-coded priority order. The order covers
Town Center vision/economy, infantry and archer lines, Mill/Lumber/Mining
economy, Blacksmith armor and attack, Monastery, Market, Castle, University,
Stable, and Siege Workshop technologies. Civilization availability is checked
where explicitly coded or rejected by the normal research API.

The policy does not score opportunity cost or remaining match time. It uses
visible enemy class, traversable map domains, and enabled victory modes only
through the bounded rules above.

### Scouting, intelligence, and fog

- Enemy units must be currently visible; enemy buildings must satisfy the
  simulation's building-visibility check.
- Resource and animal targets must be explored.
- An idle non-Villager with no visible enemy moves to the nearest unexplored
  grass tile. Water exploration is not selected.
- The controller retains no last-known enemy information and does not infer
  hidden positions.

The existing no-omniscience test proves that a hidden enemy coordinate is not
used as the immediate destination. It does not prove equivalence to commercial
scouting or information decay.

## Mixed terrain and economy integration

The deterministic computer controller now treats Beach and Shallows through
the map's authoritative walkable/sailable domains. Land scouting may cross
either terrain, transport landing scans every sailable tile, and coastal
villagers can place Docks beside explored amphibious shore. Idle Fishing Ships
prefer an owned live Fish Trap, then explored finite fish, then construct a
Fish Trap when the gate technology and resources allow it.

Farm reseed stock is prepaid at a completed Mill before other production can
spend the required wood and is consumed by normal Farm exhaustion. Animal
selection requires positive remaining food, so exhausted or spoiled carcasses
are never assigned. Villagers threatened within five tiles override current
work and route to the nearest completed Town Center, Castle, Watch Tower, or
Bombard Tower that accepts their garrison command. Mixed land/water long-run
tests save and restore both simulation and controller state, then require
identical resource, entity, queue, movement, and strategy results.

The same fixture continues for 2,000 live ticks with both players controlled
by deterministic AIs. Victory termination is disabled only for this stress
contract. Every 100 ticks it compares entity order, kinds, positions, hit
points, carried resources, economies, outcomes, and both strategy epochs. At
tick 1,000 one branch reloads the simulation plus both controller state files;
the fingerprints remain identical through tick 2,000. The run exercises
amphibious shore, naval fishing and Fish Traps, Dock construction, Farm queues,
carcass removal, shelter garrisons, combat, production, and save/load.

### Attack timing and defense

Land armies use the bounded threshold, formation, home defense, and retreat
policy above. There is still no target-value model beyond active visible
victory objectives, patrol, choke-point model, loss history, or repair force.

### Naval and transport behavior

Dock production, water targets, and bounded transport crossings follow the
domain policy above. Island base construction, naval formation, escort, and
repair-at-sea remain absent.

### Wonder and relic response

Enabled-mode Wonder construction, Relic handling, and visible countdown denial
follow the bounded policy above. The AI does not infer hidden objectives or
compute remaining-Relic threshold progress.

### Difficulty and handicaps

Difficulty follows the explicit reconstruction contract above. Commercial
difficulty names, constants, and effects remain unknown until independently
evidenced.

### Determinism

There is no random draw. Decisions depend on persisted simulation state,
five-tick cadence, enum ordering, tick-number modulus, and stable container/map
iteration. For identical initial state and identical command stream, AI-issued
commands must be identical. Save/reload determinism additionally requires
restoring equivalent controller cadence state; `last_command_tick_` is
controller state and is not currently part of the simulation save.

## Existing proof and missing tests

`tests/simulation_tests.cpp` currently proves bounded slices:

- `computer_player_moves_and_attacks`
- `computer_player_scouts_without_omniscient_targeting`
- `computer_player_gathers_and_trains_without_cheats`
- `computer_player_repeats_housing_before_population_block`
- `computer_player_advances_and_builds_age_prerequisites_normally`
- `computer_player_builds_harvests_and_reseeds_farms`
- `computer_player_targets_and_destroys_final_enemy_building`

Future AI changes should add focused tests for:

- stable worker allocation and tie-breaking;
- exact per-age construction order and failed-placement behavior;
- civilization availability and deterministic production ratios;
- lack of hidden-unit/building targeting and last-known-position policy;
- attack assembly/retreat rules if those features are introduced;
- defensive reactions;
- water-only scouting, warship production, transport loading, landing, and
  escort behavior;
- enemy Wonder/Relic countdown response and friendly objective defense;
- each defined difficulty's modifiers and absence of undeclared modifiers;
- identical AI command logs across repeat runs and save/reload boundaries.

Until original scripts, an exported runtime trace, or reproducible black-box
experiments provide stronger evidence, these are bounded reconstruction targets
only. Absence from the supplied files does not prove absence from the
commercial executable.

## Random-map stress contract

A bounded diagonal matrix runs more than 7,500 aggregate simulation ticks:
Arabia/tiny, Black Forest/small, Islands/medium, and Rivers/large. Across those
cases it covers easy through expert difficulty, British, Persian, Mayan, and
Viking starts, and conquest, Wonder, and Relic rule variants. Both players use
ComputerPlayer; one case repeats byte-equivalent inputs to assert identical
age, score, population, objective, tick, and outcome summaries.

The property gate requires strategic progress or a terminal match and forbids
an ongoing match with either player population erased. This is a stall detector,
not evidence of original AI build orders. The test intentionally samples the
cross-product diagonally so routine CTest runtime remains bounded.
## Implemented packet boundary

Native policy now covers represented producer choices, builder assignment,
bounded deterministic placement, economy allocation, land/naval army
selection, visible-target acquisition, scouting, retreat, trade, religion,
victory objectives, and civilization availability gates. Decisions use normal
command/payment paths, stable enum/entity/tile ordering, visible or explored
state where required, and persisted `ComputerPlayerState`.

`aoe_core_tests` covers controller save/reload equivalence, equal-distance
target persistence, hidden-target exclusion, civilization-locked production,
placement, construction, resource allocation, formation orders, difficulty,
and long deterministic dual-AI runs. Policy remains reconstruction-native;
commercial build orders and equivalence remain unclaimed.

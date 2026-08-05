# Computer-player fidelity contract

## Recovered evidence boundary

Supplied `gamedata_x1.drs` contains classic Petersen AI source as BINA
resources 60001 through 60029. `generated/classic_ai_package_evidence.json`
pins archive and resource hashes without redistributing source text: 29
resources, 1,796 rules, 71 fact operators, and 38 action operators. Resource
60026 is the random-game manifest, 60027 supplies constants, 60008 supplies
five difficulty branches, 60019 supplies gather allocation, and 60021 supplies
exploration, military-parity, attack-group, objective-denial, and timer rules.

Read-only executable evidence registers matching facts, actions, and strategic
numbers at `AoK-HD-patched.c:55806-60086`. This proves executable package
behavior. `tools/audit_classic_ai_package.py` reproduces nonverbatim evidence
from a user-owned archive during research. Build and runtime never read parent
workspace files or bundle commercial script source.

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

## Recovered represented strategy

- Five difficulty profiles use recovered enemy-response, formation-distance,
  missile-dodge, alliance-hit, age-gate, and attack-timer constants. Offensive
  timers are Easiest 1800/900, Easy 1200/120, Moderate 1/120 from Castle Age,
  and Hard/Hardest 1/300 from Feudal Age. Short command-pass cadence is only
  engine scheduling and cannot bypass those timers.
- Strategy state persists difficulty, command and attack ticks, strategy
  epoch, home and rally positions, and retreat state. Public status exposes
  phase, age goal, worker and army composition, desired counter, objective,
  target, home, rally, and retreat state.
- Visible cavalry selects Spearmen, visible ranged units select Skirmishers,
  and other visible military selects Archers as next production preference.
  Stable entity order breaks mixed-composition ties.
- Land armies use recovered enemy-population bands: 10 attackers against at
  most 10 enemy soldiers, 20 against at most 20, otherwise 30. Easiest sends
  its available force. Visible home threats defend immediately. Recovered
  military-parity bands, not invented aggregate-HP thresholds, drive regroup.
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
- Docks follow recovered semantic priorities: transport capacity, a first
  warship, fishing, demolition and fire ships, allied trade, then galleys.

Focused tests cover all four cadence settings without hidden handicaps,
visible-only counter choice, attack threshold, defense, retreat state
persistence, Relic collection/deposit, enabled and disabled Wonder policy,
active countdown denial, allied trade, naval combat, transport crossing, and
save/reload equivalence. A focused equal-distance fixture proves retained
target selection and controller-state save/reload equivalence. The mixed-domain
stress fixture runs two independently
saved controller pairs for 2,000 ticks, reloads one branch at tick 1,000, and
compares state every 100 ticks.

## Semantic controller behavior

`ComputerPlayer::update` ports recovered rules onto simulation commands and
stops after a terminal outcome. Package concerns tracked by separate bugs—AI
script selection, allied taunts, outbound chat, and scripted resignation—do
not silently alter this represented strategy.

### Economy allocation

- Villagers use recovered Petersen percentage plans. Dark Age changes from
  0/100/0/0 to 30/70/0/0 at nine civilians. Feudal and Castle plans respond
  to Mining Camp count, age reservation, first-Castle pressure, and difficulty.
  Idle workers fill largest deficit with stable wood/food/gold/stone ties.
- A target must be explored. Selection uses nearest Manhattan distance and
  stable map/entity iteration order for ties.
- Food includes explored berries, eligible animals, and completed owned Farms.
  Farms are reseeded one per decision pass when affordable.
- A new Farm is considered only after a completed Mill exists, no explored
  natural food remains, food is below 300, and fewer than
  `min(villager_count, 4)` Farms exist.
- Town Centers use recovered population-cap and difficulty targets.
  The AI receives no free resources and pays through normal simulation APIs.

Percentages and population tables come from resources 60019 and 60027.

### Build order and age progression

The first idle Villager is the only construction candidate per pass. Placement
searches Manhattan rings of radius zero through two around that Villager.

1. At population headroom two or less, build one House unless a House is
   already under construction.
2. Dark Age: Mill, Lumber Camp, Barracks, then Mining Camp from ten villagers.
3. Feudal Age: Market, Blacksmith, then composition-driven Range or Stable.
4. Castle Age: second Town Center on lower levels, Siege Workshop,
   University, then Monastery.
5. Imperial Age: Wonder, then one Bombard Tower if its technology exists.
6. Otherwise consider the bounded Farm rule above.

Advancement reserves production resources once four Villagers exist and the
following completed prerequisites are met: any two of Barracks, Mill, Lumber
Camp, or Mining Camp for Feudal; any two of Archery Range, Stable, or
Blacksmith for Castle; Castle, or University plus Siege Workshop, for
Imperial. Construction sequence and population gates come from resources
60001 and 60027; placement stays deterministic because exact commercial
placement randomness remains outside represented tile-order semantics.

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

Land armies use recovered group timers, population bands, military-parity
regrouping, formation, and immediate home defense. Target scoring retains the
published in-progress/distance consequence; unrepresented choke and repair
plans are not invented.

### Naval and transport behavior

Dock production, water targets, and bounded transport crossings follow the
domain policy above. Island base construction, naval formation, escort, and
repair-at-sea remain absent.

### Wonder and relic response

Enabled-mode Wonder construction, Relic handling, and visible countdown denial
follow the bounded policy above. The AI does not infer hidden objectives or
compute remaining-Relic threshold progress.

### Difficulty and handicaps

Difficulty follows resource 60008 and attack timers from resource 60021.
Easiest/Easy reaction-error fields, five enemy-response percentages, alliance
hit limits, and five attack schedules are exposed as typed profiles and tested.

### Determinism

Represented decisions depend on persisted simulation state, controller state,
enum ordering, tick number, and stable container/map iteration. Controller
state version 4 persists command, target, attack-timer, home, rally, and regroup
state; loaders retain versions 1 through 3. Identical map/input streams and
restored states therefore issue identical represented decisions.

## Proof

`tests/simulation_tests.cpp` currently proves bounded slices:

- `computer_player_moves_and_attacks`
- `computer_player_scouts_without_omniscient_targeting`
- `computer_player_gathers_and_trains_without_cheats`
- `computer_player_repeats_housing_before_population_block`
- `computer_player_advances_and_builds_age_prerequisites_normally`
- `computer_player_builds_harvests_and_reseeds_farms`
- `computer_player_targets_and_destroys_final_enemy_building`

The extended strategy fixture additionally covers all five typed difficulty
profiles, population-cap worker targets, recovered gather branches, attack
timer profiles, hidden-target exclusion, formation orders, naval combat,
transport, Relic deposit, Wonder enable/disable and denial, allied trade,
civilization availability, and controller save/reload equivalence. Python
tooling tests validate DRS bounds and prove evidence output contains metadata,
not script text.

## Random-map stress contract

A bounded diagonal matrix runs more than 7,500 aggregate simulation ticks:
Arabia/tiny, Black Forest/small, Islands/medium, and Rivers/large. Across those
cases it covers easy through expert difficulty, British, Persian, Mayan, and
Viking starts, and conquest, Wonder, and Relic rule variants. Both players use
ComputerPlayer; one case repeats byte-equivalent inputs to assert identical
age, score, population, objective, tick, and outcome summaries.

The property gate requires strategic progress or a terminal match and forbids
an ongoing match with either player population erased. Recovered build and
economy policy now drives that matrix.
## Implemented packet boundary

Recovered semantic policy now covers represented producer choices, builder assignment,
bounded deterministic placement, economy allocation, land/naval army
selection, visible-target acquisition, scouting, retreat, trade, religion,
victory objectives, and civilization availability gates. Decisions use normal
command/payment paths, stable enum/entity/tile ordering, visible or explored
state where required, and persisted `ComputerPlayerState`.

`aoe_core_tests` covers controller save/reload equivalence, equal-distance
target persistence, hidden-target exclusion, civilization-locked production,
placement, construction, resource allocation, formation orders, difficulty,
and long deterministic dual-AI runs. Operators whose effects require absent
systems remain assigned to their specific open bugs instead of being guessed:
script package selection (`BUG-AI-002`), allied taunts (`BUG-AI-003`), outbound
chat (`BUG-AI-004`), and scripted resignation (`BUG-AI-005`).

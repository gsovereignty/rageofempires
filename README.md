# Native macOS reconstruction

Readable, clean-room C++ reconstruction informed by static analysis of the
supplied Age of Empires II HD binary. This is new source code, not mechanically
cleaned Ghidra output and not a claim of source compatibility.

## Scope and non-requirements

Commercial scenario, campaign, saved-game, and recorded-game compatibility is
not required. The related legacy inspectors and bounded converters support
software-archaeology research only. They may remain partial without affecting
the reconstruction's completion status, release readiness, or implementation
priorities. Native Scenario, Campaign, Save, and Replay formats are the
supported runtime contracts.

Implemented:

- Native Universal 2 macOS build using C++20, CMake, and SDL3.
- Isometric terrain with visible forest resources and combat health bars.
- Animated water wavelets with explored-only sand/foam shoreline transitions.
- Per-player fog of war with live vision, persistent exploration memory, and
  concealed enemy entities. Multi-tile structures reveal, remain valid combat
  targets, and appear on minimap when any footprint tile enters current vision.
- Fog-aware terrain minimap with visible unit and building markers.
- Deterministic tile simulation separated from display and input.
- Resignation now persists per-player controller state. Resigned local
  controllers become read-only full-map observers while simulation commands
  and chat are rejected; ordinary fog APIs remain unchanged. Save110 and
  replay/multiplayer behavior are defined in
  [`RESIGNATION_OBSERVER.md`](RESIGNATION_OBSERVER.md).
- Bounded classic `.ai`/`.per` inspection parses constants, loads, weighted
  random loads, and `defrule` facts/actions with exact unsupported-span
  preservation. A strict mapped subset emits deterministic typed intents under
  rule/action budgets; unknown semantics block executable mode. See
  [`AI_SCRIPT_FIDELITY.md`](AI_SCRIPT_FIDELITY.md).
- Authoritative two-player match statistics cover gathered resources, tribute,
  military and building totals, conversions, relics, research, age timings,
  Wonders, live score, and deterministic timeline samples. Save110 persists
  the model; [`MATCH_STATISTICS.md`](MATCH_STATISTICS.md) defines its UI API
  and compatibility contract.
- Grass, Water, Beach, Shallows, forest resources, and terrain-restricted unit
  movement. Land units and ships cross Beach and Shallows; Water is ship-only.
- Blue and red villagers, knights, ranged archers, and anti-archer
  Skirmishers.
- Click and drag-box selection, formation-spread group movement, finite wood,
  food, gold, and stone gathering with typed villager carry/drop-off,
  nearest same-resource continuation after depletion, persistent combat, and
  unit death.
- Obstacle-aware group slot allocation for movement, queued waypoints,
  Attack Move, and patrol. Slots avoid water, buildings, stationary units,
  and unreachable map regions; nearest-unit pairing reduces route crossings.
- A* pathfinding around water and occupied tiles.
- Town centers with food costs and deterministic production queues.
- Original-style last-item production cancellation with full exact-cost
  refunds, preserved active-order progress, replay support, and an on-screen
  production progress bar.
- Full-size 4×4 Town Centers with original 2,400 HP, 3/5 armor, eight-tile
  sight, 275-wood/100-stone cost, five population support, and footprint-edge
  pathing/combat geometry. Additional Town Centers unlock in Castle Age;
  players without any Town Center may rebuild one earlier.
- Original-style Town Center sheltering: up to 15 friendly foot units can
  garrison by right-clicking, carried resources are deposited, occupants
  disappear from map targeting, and Villagers/archers generate up to ten
  five-damage defensive arrows at range six. Garrison state, projectiles, and
  ungarrison commands persist through saves and replays.
- Timed villager construction with foundations, progress, interruption/resume,
  incomplete-building production gates, and original-style cooperative
  building speed (`3T / (n + 2)`) with deterministic save persistence.
  New movement, gathering, repair, garrison, combat, or construction orders
  detach villagers from their previous foundation.
- Persistent villager repair orders for damaged friendly buildings, including
  full-footprint approach routing and proportional half-cost wood/stone use.
- Dark-Age Palisade Walls with their original 3-wood cost, 250 HP, 2/5 armor,
  segment construction, path blocking, combat destruction, and repair.
- Dark-Age Palisade Gates in both isometric orientations with original
  30-wood cost, 600 HP, 2/6 armor, four-tile footprints, friendly automatic
  opening, closed enemy path denial, combat destruction, and persistent state.
- Feudal-Age Watch Towers with their classic 125-stone cost, 1,020 HP, 1/7
  armor, range-eight autonomous arrow fire, minimum-range dead zone, and
  original Fletching/Murder Holes interactions.
- Feudal-Age Stone Walls with their classic 5-stone cost, 1,800 HP, 8/10
  armor, segment construction, path blocking, repair, and siege destruction.
- Feudal-Age Stone Gates in both isometric orientations with HD-era
  30-stone cost, 2,750 HP, 6/6 armor, four-tile footprints, automatic
  friendly opening, closed enemy path denial, combat, repair, and persistence.
- Barracks Militia and Spearman production with Age gates and Spearman bonus
  damage against cavalry, plus Feudal-Age Stables, Scout Cavalry, and
  Castle-Age Knight production.
- Scout Cavalry receive original automatic age behavior: Feudal Age raises
  attack from three to five and sight from four to six, then Castle and
  Imperial Age add two more sight each. Fixed-point movement reproduces their
  1.20 Dark-Age and 1.55 Feudal-or-later source speeds, including exact
  Husbandry stacking across saves and replays. Their 30-second source training
  time uses the compressed 12-tick military-production cadence.
- Classic Knight balance uses 100 HP, 10 attack, 2/2 armor, four-tile sight,
  a 60-food/75-gold cost, exact 1.35 fixed-point movement speed with 1.485
  after Husbandry, and 30-second source training time represented by the
  simulation's compressed 12-tick military-production cadence.
- Imperial Cavalier research at the Stable costs 300 food/300 gold, uses its
  HD-era 100-second source research time as 20 compressed ticks, and
  converts existing and queued Knights while changing future production to
  120-HP, attack-12 Cavaliers. Existing damage, Bloodlines, blacksmith armor,
  attack upgrades, exact movement timing, saves, and replays remain intact.
- Imperial Paladin research requires Cavalier, costs 1,300 food/750 gold,
  and uses its 170-second source time as 34 compressed ticks. Existing and
  queued heavy cavalry become 160-HP, attack-14, 2/3-armored, five-sight
  Paladins while preserving damage and all generic cavalry technologies.
- Castle-Age Light Cavalry research at the Stable costs 150 food/50 gold,
  uses its 45-second source research time, and converts existing, queued, and
  future Scouts into 60-HP, attack-7 Light Cavalry with 0/2 armor, sight 8,
  speed 1.50, 80-food cost, and 30-second source training time. Damage,
  Bloodlines, Husbandry, and cavalry blacksmith upgrades remain intact.
- Imperial Hussar research requires Light Cavalry, costs 500 food/600 gold,
  uses its 50-second source research time, and converts existing, queued, and
  future Scout-line units into 75-HP, attack-7 Hussars with 0/2 armor,
  sight 10, speed 1.50, 80-food cost, and 30-second source training time.
- Classic Spearman production costs 35 food/25 wood. Timed Castle-Age
  Pikeman research at the Barracks costs 215 food/90 gold, converting
  existing and queued Spears and changing future production to 55-HP,
  attack-four Pikemen with +22 cavalry bonus damage.
- Imperial Halberdier research requires Pikeman, costs 300 food/600 gold,
  uses its 50-second source research time, and converts existing, queued, and
  future Spearman-line units into 60-HP, attack-six Halberdiers with +32
  cavalry, +28 war-elephant, and +16 camel bonus damage. It is available to
  British, French, Goths, Germans, Japanese, Chinese, Byzantine, Persians,
  Celts, Spanish, Mayan, Huns, and Koreans.
- Timed Feudal Man-at-Arms research at the Barracks for 100 food/40 gold,
  converting existing and queued Militia while changing all future Militia
  production to the upgraded 45-HP, attack-six unit.
- Timed Castle-Age Long Swordsman research at the Barracks for the HD-era
  200-food/65-gold cost, requiring Man-at-Arms and converting the full Militia
  line to 55 HP, attack nine, one pierce armor, and +3 building damage.
- Imperial Two-Handed Swordsman research requires Long Swordsman, costs
  300 food/100 gold, and uses its classic 75-second research time. Existing,
  queued, and future Militia-line units become 60-HP, attack-11,
  0/1-armored Two-Handed Swordsmen with +4 damage against buildings while
  preserving damage and infantry blacksmith upgrades.
- Imperial Champion research requires Two-Handed Swordsman, costs
  750 food/350 gold, and uses its classic 100-second research time. Existing,
  queued, and future Militia-line units become 70-HP, attack-13,
  1/1-armored Champions while preserving damage and generic upgrades.
- Imperial Arbalester research requires Crossbowman, costs 350 food/300 gold,
  and uses its classic 50-second research time. Existing, queued, and future
  Archer-line units become 40-HP, attack-6, range-5 Arbalesters while
  preserving damage and archer blacksmith upgrades.
- Castle-Age Elite Skirmisher research costs 230 wood/130 gold and uses its
  classic 50-second research time. Existing, queued, and future Skirmishers
  become 35-HP, attack-3, range-5, 0/4-armored Elite Skirmishers with
  minimum range 1, +4 damage against archers, and +3 against Spear-line units.
- Deterministic relative movement cadence: cavalry travels twice as quickly
  as foot units, with mid-step timing preserved by saves and replays.
- Smooth render interpolation for unit travel and projectile flight between
  deterministic 200 ms simulation ticks.
- Moving foot units bob and alternate steps, cavalry changes hoof stride, and
  siege wheels rotate while shadows and selection diamonds stay ground-locked.
- Persistent attack orders that re-path after moving targets, plus
  vision-limited idle military auto-acquisition and retaliation.
- Deterministic opponent separates villager and military roles, gathers only
  explored resources, trains villagers and military through normal queues,
  and pays standard costs instead of receiving scripted resources.
- Combatants visibly swing toward targets, draw/release ranged weapons, and
  recoil siege engines using deterministic cooldown-derived frames.
- Original-style formation attack-move: selected military advances toward its
  destination, diverts to visible enemies, then resumes its saved route after
  combat. Selected units show a red destination line and tile marker.
- Two-endpoint formation patrols repeatedly traverse their route, engage
  visible enemies, then return to patrol. Selected patrols show cyan endpoint
  diamonds and route line.
- Guard orders protect friendly units or completed buildings. Guards follow
  moving targets, intercept visible enemies, then return; selected guards show
  a green tether and target diamond.
- Shift-right-click queues up to 20 formation-aware movement waypoints.
  Multi-leg routes continue after ordered combat, persist across formats, and
  render as orange linked destination diamonds for selected units.
- `Ctrl+F1` through `Ctrl+F5` select compact, line, box, staggered, and flank
  formations. Deterministic role ordering, ordinary/siege/ship spacing,
  slowest-effective-member pace, four-step regrouping, naval slots, and
  correctly faced patrol/guard geometry apply to semantic group orders.
  Queued legs start atomically after every member finishes the prior leg.
- Current Save v109 preserves active and queued formation state; v99 introduced
  the latest farm-reseed/carcass fields around that state. Current Scenario v66
  preserves player formation selection, first introduced at the v61 gate.
  Replay v63 preserves formation-kind
  changes and semantic move, attack-move, patrol, guard, and queued-order
  records. Save v96 and Replay v61 remain the compatibility gates for queued
  formation legs.
  [`FORMATION_FIDELITY.md`](FORMATION_FIDELITY.md) defines these bounded
  reconstruction contracts and their DAT evidence limits.
- Four persistent military stances: Aggressive pursues through sight,
  Defensive uses a six-tile leash and returns to its exact anchor, Stand Ground
  fires only within weapon range without chasing, and Passive disables
  automatic attacks while preserving explicit orders.
- Own-unit and own-building deletion with immediate victory-state updates.
  Cancelled foundations refund the exact unbuilt fraction of wood and stone;
  completed buildings refund nothing, queued production is lost, and Town
  Center occupants evacuate before deletion.
- Combat deaths leave a brief owner-colored collapse and ground corpse instead
  of vanishing instantly. Death visuals obey current fog and persist through
  mid-effect saves without affecting movement or population.
- Combat-destroyed buildings leave footprint-sized stone and timber rubble.
  Ruins fade deterministically, obey current fog, persist through saves, and
  never block movement or rebuilding.
- Completed buildings ignite below half health. Critical structures show
  multiple animated flame and dark-smoke columns, derived deterministically
  from simulation tick and building ID.
- Deterministic group Stop command clears movement, combat, gathering, repair,
  construction, and pending garrison orders while preserving carried
  resources.
- Archery ranges, archer production, range-aware targeting, and delayed
  projectile flight/impact. Visible arrow strikes linger briefly at the hit
  tile; arrows track moving unit targets through flight. Both behaviors remain
  deterministic across saves.
- Classic Archer balance uses 30 HP and 25 wood/45 gold. Timed Castle-Age
  Crossbowman research at the Archery Range costs 125 food/75 gold, converting
  existing and queued Archers and changing future production to 35-HP,
  attack-five, range-five Crossbowmen.
- Feudal Skirmishers trained at Archery Ranges for 25 food/35 wood, with
  classic 30 HP, attack 2, range 4, 3 pierce armor, and +3 bonus damage
  against the archer class.
- Atomic multi-resource military costs plus melee/pierce damage classes, unit and
  building armor, and one-point minimum damage.
- Dark, Feudal, Castle, and Imperial Age progression with town-center
  research, atomic costs, building prerequisites, and content gates.
- Original-style Age building gates: two distinct Dark-Age buildings for
  Feudal, two distinct Feudal buildings for Castle, and a completed Castle
  landmark for Imperial in the current building roster.
- Castle construction in Castle Age with atomic 650-stone cost, durable armor,
  a distinct tower/battlement silhouette, and autonomous five-arrow defensive
  volleys, plus its original 4,800 HP, 8/11 armor, 11 sight, and 20 population
  support. Its 4×4 footprint blocks placement and unit pathing across the full
  rendered foundation; sight, target range, and projectile flight resolve from
  the nearest footprint edge. Its original one-tile minimum range leaves
  adjacent melee attackers in a dead zone unless Murder Holes is researched.
- Castle-Age Universities with 3×3 footprints and timed Murder Holes research
  at its original 200-food/100-stone cost. The completed technology removes
  Castle and Watch Tower minimum range and persists through saves and replays.
- Castle-Age Fortified Wall at the University costs 200 food/100 wood,
  upgrades existing and future Stone Walls to 3000 HP and 12/12 armor, and
  Stone Gates to their HD-era 4000 HP while retaining 6/6 armor.
- Castle-Age Guard Tower research costs 100 food/250 wood and upgrades
  existing and future Watch Towers to 1,500 HP, 7 attack, and 2/8 armor.
- Imperial-Age Keep research requires Guard Tower, costs 500 food/350 wood,
  and upgrades existing and future towers to 2,250 HP, 8 attack, and 3/9 armor.
- Timed Loom, Double-Bit Axe, Horse Collar, Wheelbarrow, Fletching, Forging,
  Murder Holes, and Man-at-Arms research with Age/site gates, atomic costs,
  and persistent effects.
  Castle-Age Bodkin Arrow requires Fletching, costs 200 food/100 gold, and
  adds +1 attack/range to arrow units, Castles, and towers while adding
  +1 attack/line-of-sight—but no range—to Town Centers.
  Imperial Bracer requires Bodkin Arrow, costs 300 food/200 gold, and adds
  another +1 attack/range with the same Town Center exception.
  Castle-Age Iron Casting requires Forging, costs 220 food/120 gold, and adds
  +1 melee attack to generic infantry and cavalry. Generic Villagers correctly
  receive neither melee technology without an Inca civilization bonus.
  Imperial Blast Furnace requires Iron Casting, costs 275 food/225 gold, and
  adds the original final +2 attack to infantry and cavalry.
  Feudal Scale Mail Armor costs 100 food and adds +1 melee armor and +1 pierce
  armor to existing and future infantry while excluding generic Villagers.
  Castle-Age Chain Mail Armor requires Scale Mail Armor, costs 200 food/100
  gold, and adds another +1 melee armor and +1 pierce armor to infantry.
  Imperial Plate Mail Armor requires Chain Mail Armor, costs 300 food/150
  gold, and adds its original +1 melee armor and +2 pierce armor to infantry.
  Feudal Scale Barding Armor costs 150 food and adds +1 melee armor and +1
  pierce armor to existing and future cavalry.
  Castle-Age Chain Barding Armor requires Scale Barding Armor, costs 250
  food/150 gold, and adds another +1 melee and +1 pierce armor to cavalry.
  Imperial Plate Barding Armor requires Chain Barding Armor, costs 350
  food/200 gold, and adds its original +1 melee and +2 pierce armor to cavalry.
  Feudal Padded Archer Armor costs 100 food and adds +1 melee armor and +1
  pierce armor to existing and future Archers and Crossbowmen.
  Castle-Age Leather Archer Armor requires Padded Archer Armor, costs 150
  food/150 gold, and adds another +1 melee and +1 pierce armor to archers.
  Imperial Ring Archer Armor requires Leather Archer Armor, costs 250 food/250
  gold, and adds its original +1 melee and +2 pierce armor to archers.
  Feudal Bloodlines costs 150 food/100 gold and grants +20 HP to existing and
  future Knights and Scout Cavalry while preserving existing damage.
  Castle-Age Husbandry costs 150 food and gives Knights and Scout Cavalry an
  exact deterministic +10% movement rate with save/replay-stable timing.
  Dark-Age Loom costs its original 50 gold and grants Villagers +15 HP,
  +1 melee armor, and +2 pierce armor.
  Feudal Double-Bit Axe costs its original 100 food/50 wood and grants an
  exact deterministic +20% wood-gathering rate.
  Feudal Horse Collar costs its original 75 food/75 wood and adds 75 food to
  active, future, and reseeded farms.
  Wheelbarrow grants Villagers original +5 carry capacity and exact long-run
  +10% movement speed through deterministic fractional cadence. Fletching
  grants Castles and Watch Towers original +1 attack and +1 range as well as
  upgrading Archers.
- Castle-Age 3×3 Siege Workshops gated by a completed Blacksmith, producing
  slow Battering Rams for 160 wood/75 gold. Rams retain 175 HP, extreme pierce
  armor, and heavy bonus damage against buildings.
- Imperial Capped Ram research costs 300 food and upgrades existing, queued,
  and future Rams to 200 HP, attack 3, 0/190 armor, radius-one area damage,
  and +150 building damage. It is available to all 18 civilizations. Siege
  Ram requires Capped Ram, costs 1000 food, and upgrades them again to 270 HP,
  attack 4, 0/195 armor, radius-two area damage, and +200 building damage. It
  is available to Chinese, Byzantine, Persians, Saracens, Turks, Vikings,
  Mongols, Celts, Spanish, Aztecs, Mayan, and Huns. Both upgrades retain the
  160 wood/75 gold train cost and participate in the same AI, scenario,
  save/replay, Celt/Mongol attack-speed, and Drill movement-speed paths as
  Battering Rams.
- Siege Workshops also produce Mangonels for 160 wood/135 gold: slow
  50-HP ranged siege with attack 40 melee damage, range 7, minimum range 3,
  radius-one area damage, authentic friendly fire, and persistent Attack
  Ground orders that move into range before launching. Impact tiles show an
  expanding dust ring and debris while currently visible through fog of war.
- Imperial Chemistry at the University costs 300 food/200 gold and uses its
  100-second source research time. It unlocks civilization-gated Hand
  Cannoneers and Bombard Cannons. Hand Cannoneers cost 45 food/50 gold and
  provide 65%-accurate range-seven fire with +10 infantry and +2 spearman
  damage. Bombard Cannons cost 225 wood/225 gold and provide 92%-accurate
  range-twelve fire, minimum range five, half-tile splash, and +200 building
  damage. Hand Cannoneer is available to French, Goths, Germans, Japanese,
  Byzantine, Persians, Saracens, Turks, Spanish, and Koreans; Bombard Cannon
  uses the same set without Japanese. Both retain recorded frame delays,
  projectile visuals, and sounds.
- Imperial Siege Engineers at the University costs 500 food/600 wood, uses
  its 45-second source research time, and multiplies represented siege
  building bonuses by exactly 120%, while adding one attack-range, search,
  and LOS tile. Battering, Capped, and Siege Rams receive the LOS/search and
  damage changes but retain their range through the recorded per-unit -1
  range exceptions. It is available to British, French, Germans, Japanese,
  Saracens, Vikings, Mongols, Celts, Aztecs, and Koreans.
- Imperial Conscription at the Castle costs 150 food/150 gold, uses its
  60-second source research time, and multiplies unit-production work by
  exactly 133% at represented Barracks, Archery Ranges, Stables, and Castles.
  It applies to unit queues only: technology and Age research are not
  accelerated. Conscription is available to all 18 civilizations.
- Castle-Age Petards are available to all 18 civilizations through hidden
  availability gate 426; the gate is never exposed as a Barracks research
  button. They cost 80 food/20 gold, train at the Castle in 25 source seconds,
  and self-destruct on attack with 25 base damage, +500 building damage,
  +900 wall damage, and half-tile radius. Siege Engineers adds the recorded
  +200 building damage. DAT blast offense level 2 is preserved, but its exact
  allied-unit filtering remains behaviorally unproved.
- Imperial Bombard Tower research at the University costs 800 food/400 stone,
  uses its 60-second source research time, and requires Imperial Age plus
  Chemistry or the alternative Turk free-Chemistry marker. It is available
  to Germans, Chinese, Byzantine, Turks, and Spanish. The resulting 1×1 tower
  costs 100 gold/125 stone, builds in 80 source seconds, retains 2,220 HP and
  3/9 armor, and fires 92%-accurate attack-120 cannonballs at range eight with
  minimum range one and reload six. Normal, construction, death, projectile,
  and impact SLPs are proved present; missing snow SLP 2263 falls back to the
  normal tower sprite.
- Castle-Age Scorpions and their Imperial Heavy Scorpion upgrade use the live
  DAT costs, HP, range, minimum range, reload, projectile links, and bonus
  classes. Imperial Onager and Siege Onager research upgrades existing,
  queued, and future Mangonels with their recorded splash radii, friendly
  fire, projectiles, costs, and civilization availability.
- Imperial Castles train Trebuchets into their mobile packed state. A
  deterministic two-tick pack/unpack transition switches between mobile,
  weaponless Packed Trebuchets and immobile range-16 Trebuchets with a
  four-tile minimum range. Kataparuto accelerates represented reload and
  transition work without changing unit cost or Castle training time.
- Aztec and Mayan Barracks train Eagle Warriors and expose their Imperial
  Elite upgrade; El Dorado adds its recorded 40 HP. Castle-Age Archery Ranges
  train Cavalry Archers for civilizations whose live DAT gate permits them,
  with Heavy Cavalry Archer research upgrading existing, queued, and future
  units. Archer attack/armor technologies, Husbandry, Bloodlines, civilization
  availability, saves, scenarios, AI production, and replays cover both forms.
- Chinese, Byzantine, Persian, Saracen, Turkish, and Mongol Stables train
  Camel Riders in Castle Age. Heavy Camel research upgrades existing, queued,
  and future units; cavalry technologies and Zealotry apply through the shared
  combat, movement, save, scenario, AI, and replay paths.
- Imperial Age can be unlocked by a completed Castle or by both a University
  and Siege Workshop, matching the available Castle-Age building roster.
- Houses and completed-building population support with queue reservations and
  population-blocked production.
- Mills, lumber camps, and mining camps with resource-compatible nearest
  drop-off routing and recovery when a drop-off is destroyed.
- Villagers visibly swing axes, picks, sickles, or hammers while gathering,
  constructing, and repairing. Carried wood, food, gold, and stone appear as
  color-coded packs and remain derived from saved simulation state.
- Constructed farms with finite food, persistent villager farming, exhaustion,
  mill delivery, and paid deterministic reseeding. Farms render as full
  isometric crop plots whose stalk density reflects remaining food.
- Human-readable versioned save/load format.
- Central named rules table for health, attack cadence, costs, vision,
  armor, construction, and production timing.
- On-screen HUD showing economy, tick, selection, queues, and controls.
- Deterministic red computer player using public simulation commands for
  gathering, natural-food depletion recovery through paid farm construction
  and reseeding, production, repeated housing, staged infrastructure, paid
  age advancement and unit upgrades, visibility-limited targeting, and
  scouting. Military directly targets visible hostile units and structures,
  allowing computer-controlled matches to destroy final buildings and end.
- Building combat, destruction, and explicit victory/defeat outcomes.
- Pause and complete-match restart flow.
- Tick-addressed deterministic command recording and replay.
- Human-readable, versioned replay files.
- Human-editable scenario files for maps, terrain, economy, and entities.
- Bounded commercial `.scn`/`.scx` inspection and import reporting: classic
  headers, player settings, terrain/elevation maps, object tables, and trigger
  records decode with strict limits. The repository's live-DAT/renderer
  catalogs provide unique commercial IDs for all 96 represented unit kinds and
  23 of 27 building kinds; only unresolved gate orientations remain unmapped.
  Every unsupported or lossy tile/object remains explicitly reported, and
  unsupported terrain prevents playable output.
- Bounded classic commercial `.cpn`/`.cpx` campaign-container inspection:
  ordered names/index entries, raw embedded scenarios, and unindexed bytes are
  preserved with strict range validation. Embedded scenarios use the bounded
  commercial scenario decoder; campaign branching/cinematics are not inferred.
- Bounded classic `.mgx` recorded-game inspection: raw-DEFLATE header
  preservation, proved version/action/synchronization/game-start/chat framing,
  exact unsupported-tail retention, and atomic conversion of the fully mapped
  subset into reconstruction Replay v63. It does not claim commercial
  save-game support or complete recorded-game compatibility.
- Core simulation tests independent of SDL.

## Build on macOS

The root `Makefile` provides the shortest development workflow:

```sh
cd reconstruction
make
make run
make test
```

`make` configures a Release build and compiles it. `make run` launches the
Finder-compatible app bundle on macOS, while `make test` builds and runs the
complete CTest suite. `make clean` removes compiled outputs while preserving
the CMake configuration.

The build directory, build type, parallelism, and extra CMake options are
overrideable:

```sh
make BUILD_DIR=build-local BUILD_TYPE=Debug JOBS=8
make CMAKE_ARGS="-DAOE_BUILD_SDL3=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sdl3"
```

The equivalent direct CMake workflow is:

```sh
cd reconstruction
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/aoe_reconstruction
```

Apple builds default to Universal 2 (`arm64` and `x86_64`) with a macOS 11.0
deployment target. CMake fetches SDL3 3.4.12 from a locked upstream commit and
compiles both slices under the same target, avoiding host-package
deployment-version leakage. Set `AOE_BUILD_SDL3=OFF` and provide `SDL3_DIR`
only for local development with a compatible prebuilt SDL.

Finder-compatible app bundle:

```sh
open "build/AoE Archaeology.app"
```

Development builds automatically use `../original-assets/app` when that
installation tree exists. `AOE_ASSET_ROOT` overrides this default, allowing a
different extracted original HD installation without copying its art into this
repository or app bundle:

```sh
AOE_ASSET_ROOT="/path/to/extracted/app" ./build/aoe_reconstruction
```

`AOE_ASSET_ROOT` may contain loose `Terrain/Textures` PNGs for Grass, Water,
Beach, and Shallows. Missing loose textures fall back individually to SLPs
15001, 15002, 15017, and 15014 from `Data/terrain.drs`, using palette 50500
from `Data/interfac.drs`. Forest, berry, gold, and stone ground tiles reuse
Grass; fog shading remains active. Resource objects prefer live archive sprites
for berry 2560, gold 2561, stone 1034, and fish 420, with procedural silhouettes
retained per resource when its mapped sprite is missing or unreadable. Missing,
unreadable, or unset terrain assets fall back individually to built-in
procedural terrain. Original assets stay at their source path and are never
copied into build outputs.

When the same local root contains `Data/graphics.drs` and
`Data/interfac.drs`, original sprites are also used for sheep, deer, boar,
villagers, current military units, oak/forest trees, and gold mines. Directional
animations use the five stored viewing angles plus mirrored eastern views.
Mappings come from `empires2_x1_p1.dat` (animal SLPs 3629, 342, and 2557;
villager 1479; tree 4652; gold 2561), with palette 50500 from `interfac.drs`.
Missing archives, palette entries, mapped SLPs, or undecodable frames fall back
to procedural art. Town Center mapping is
recognized as a multi-layer DAT composition, but remains procedural when any
required layer is absent. DRS contents are read in place and never extracted
into or copied into build outputs.

Release bundle embeds SDL3 under `Contents/Frameworks`; it has no Homebrew
runtime dependency. Verifier checks both Universal 2 slices and macOS 11.0 in
the executable, SDL dylib, and Info.plist alongside resources, linkage, signature,
bundled-scenario loading, headless rendering, screenshot dimensions, and clean
app exit:

```sh
./scripts/verify_macos_bundle.sh "build-release/AoE Archaeology.app"
```

For headless visual audits, set `AOE_SCREENSHOT_PATH` to a `.bmp` path. First
fully rendered frame is captured before presentation, including under SDL's
dummy video driver. `AOE_WINDOW_SIZE=800x600` can exercise resized,
letterboxed output. `AOE_SCREENSHOT_TICK` and `AOE_SCREENSHOT_ALPHA` delay
capture to a chosen simulation tick and interpolation fraction.
`AOE_SCENARIO_PATH` loads a custom scenario for targeted render audits.
`AOE_RENDER_FALLBACK_REPORT` writes deduplicated structured JSON whenever
runtime rendering reaches a reviewed procedural path or an unresolved asset
failure. See `RENDERER_ASSET_COVERAGE.md` for report fields and audit gates.
`AOE_SCREENSHOT_SELECT_TILE=X,Y` selects an initial blue entity so
selection-only HUD and order markers can be audited headlessly.
`AOE_SCREENSHOT_ATTACK_GROUND=X,Y` gives selected Mangonels an initial
Attack Ground order for projectile and target-marker audits.
`AOE_SCREENSHOT_COMMAND_TILE=X,Y` issues one normal contextual command to
the initial screenshot selection.
`AOE_EXIT_AFTER_SCREENSHOT=1` closes cleanly after a successful capture,
supporting bounded CI and bundle smoke tests without synthetic input.
`resources/rubble-audit.scenario` provides a focused destruction scene; its
ram destroys the red house before a tick-15 rubble capture.
`resources/damage-audit.scenario` shows half-health and critical-health fire
stages. Scenario v27 accepts validated `hit_points N` building markers.
`resources/villager-work-audit.scenario` exposes gathering tools and carried
wood when selected at `2,2`, commanded to `3,2`, and captured at tick 3.
`resources/villager-repair-audit.scenario` exposes hammer animation against a
burning house when selected at `3,2` and commanded to `4,2`.
`resources/combat-pose-audit.scenario` exposes melee and ranged attack frames
at tick 2.
`resources/movement-gait-audit.scenario` exposes foot, hoof, and wheel gait at
tick 1 with `AOE_SCREENSHOT_ALPHA=0.25`.
`resources/farm-render-audit.scenario` shows full, partly harvested, and
exhausted crop plots side by side.
`resources/water-render-audit.scenario` exposes animated pond wavelets and
shoreline transitions without revealing unexplored neighboring terrain.
`resources/sheep-gather-audit.scenario` exposes sheep pathing and finite food;
select `2,3`, command `5,3`, and capture tick 4.
`resources/huntable-animal-audit.scenario` places passive Deer and a dangerous
retaliating Boar beside blue hunters for deterministic hunt/carcass audits.
`resources/monk-conversion-audit.scenario` places a Castle Age Monk and enemy
Knight at conversion range for deterministic progress/recharge audits.
`resources/monk-relic-healing-audit.scenario` places a Monk, friendly
Villager, neutral Relic, and Monastery together for support/economy audits.
`resources/market-exchange-audit.scenario` places completed Feudal Markets for
both players with ample stock for deterministic shared-price exchange audits.
`resources/allied-trade-audit.scenario` supplies allied Markets and a blue
Trade Cart for route income, shared-vision, and non-hostile targeting audits.
`resources/civilization-bonuses-audit.scenario` contrasts Castle Age Briton
Crossbowman range with Frank Knight hit points without inventing unique units.
`resources/expanded-civilizations-audit.scenario` contrasts Viking infantry
hit points with a Byzantine counter unit and age-scaled building durability.
`resources/asian-saracen-civilizations-audit.scenario` places faster-attacking
Japanese infantry against a double-HP Persian Town Center.
`resources/final-civilizations-audit.scenario` contrasts Turkish Scout-line
pierce armor with Mongol Scout-line hit points.
`resources/conquerors-civilizations-audit.scenario` places a Korean Villager
and free-upgraded tower opposite a Mayan Archer/Archery Range.
`resources/naval-fishing-audit.scenario` places a Fishing Ship, 200-food fish
node, and shoreline Dock for deterministic water pathing/economy audits.
`resources/naval-combat-transport-audit.scenario` places opposing Galleys and
a shoreline Transport/Villager pair for projectile and passenger audits.
`resources/fire-demolition-ship-audit.scenario` places both classic Fire Ship
and Demolition Ship lines, with blue Imperial upgrades, for combat audits.
`resources/cannon-dock-technologies-audit.scenario` contrasts Cannon Galleon
levels and exposes Careening, Dry Dock, and Shipwright effects.
`resources/unique-naval-units-audit.scenario` contrasts Viking Longboat and
Korean Turtle Ship lines at base and elite levels.
`resources/castle-unique-units-audit.scenario` contrasts Briton Longbowman
and Teuton Teutonic Knight lines at base and elite levels.
`resources/eastern-castle-unique-units-audit.scenario` contrasts Japanese
Samurai and Persian War Elephant lines at base and elite levels.
`resources/loom-render-audit.scenario` exposes a selected Loom-upgraded
Villager with 40 HP and 1/2 armor in HUD.
`resources/double-bit-axe-audit.scenario` exposes upgraded wood gathering;
select `4,2`, command `5,2`, and capture tick 5 for six carried wood.
`resources/horse-collar-render-audit.scenario` shows 250-food full,
half-used, and exhausted upgraded farm density.
`resources/palisade-gate-render-audit.scenario` shows an automatically opened
NW-SE Palisade Gate beside a friendly Villager and a closed NE-SW gate.
`resources/stone-gate-render-audit.scenario` shows matching open and closed
Stone Gates in both orientations.
`resources/ai-building-attack-audit.scenario` shows red computer siege
explicitly attacking blue player's final structure.
`resources/footprint-visibility-audit.scenario` proves a large enemy building
appears in world and minimap when any footprint tile enters vision.
`resources/wheelbarrow-movement-audit.scenario` runs upgraded and baseline
Villagers in parallel; blue leads by one tile at tick 20.

Controls:

- `Ctrl+1` through `Ctrl+9` assigns selected units or building to a control
  group. macOS `Command+number` works too. `Shift+number` adds selected units;
  plain number recalls a populated group and filters destroyed entities.
  Existing numeric build/research hotkeys remain active while slot is empty.
- Press `.` to cycle through idle Villagers. `Shift+.` selects every idle
  Villager. HUD keeps live idle count; gathering, construction, repair,
  movement, combat, garrison, guard, patrol, and queued orders count as work.
- Press `,` to cycle through idle military units. `Shift+,` selects all idle
  military. Movement, targeting, Attack Move, Attack Ground, patrol, guard,
  garrison, stance-return, and queued waypoints count as active orders.
- Left click blue unit: select.
- Shift-click blue unit: add it to selection or remove it when already
  selected. Double-click selects every visible blue unit of same type.
- Left drag: select multiple blue units. Hold Shift to merge box contents
  into current selection.
- Move pointer against map edge or use arrow keys to scroll camera. Mouse wheel
  zooms from 1× to 2× around pointer. Click minimap to center camera.
- `Ctrl+F1` compact, `Ctrl+F2` line, `Ctrl+F3` box, `Ctrl+F4` staggered,
  `Ctrl+F5` flank.
- Right click grass: move selected units in current formation.
- Hold Shift while right-clicking grass to append formation waypoints. Units
  traverse every leg in order; Stop or an unmodified order clears the queue.
- Right click with a completed building selected: set its rally point. Newly
  trained units immediately route there; villagers can rally directly onto a
  resource to begin gathering.
- Right click tree, berry bush, gold deposit, or stone deposit with villager:
  gather, carry up to 10, and return resources to nearest town center.
- Right click sheep with villager: gather its finite 100 food, deliver food,
  and continue until sheep is depleted. Sheep provide vision, move when
  selected, use no population, and do not keep an otherwise defeated player
  alive. Computer players gather owned sheep rather than commanding them as
  military units.
- Right click live Deer or Boar with a Villager to hunt it. Deer remain
  passive and leave 140 food. Boar carry 340 food, retaliate against their
  attacker, chase it, and can kill an unsupported Villager. Right click the
  carcass to gather and deliver its finite food. Deer and Boar use no
  population, provide no military survival, and are never AI military units.
- Castle Age Monasteries cost 175 wood and train Monks for 100 gold. A Monk
  converts one visible enemy unit within nine tiles after eight uninterrupted
  simulation ticks; success changes ownership and starts a 20-tick recharge.
  Friendly units, animals, dead units, garrisoned units, and unseen or
  out-of-range targets are invalid. Monks also heal damaged friendly organic
  units within four tiles by one HP per simulation tick. They can carry one
  adjacent neutral Relic and deposit it at an adjacent completed friendly
  Monastery; each stored Relic generates one gold every ten simulation ticks.
  Relics are neutral, unselectable by either player, noncombatant, population
  free, and do not keep a player alive in victory checks.
- Feudal Age Markets cost 175 wood. Food, wood, and stone trade in 100-unit
  lots against gold. Each commodity starts at a 100-gold base: buying costs
  130 gold and selling returns 70 gold, reproducing the original 30% fee.
  Every buy raises the shared base by 3 and every sale lowers it by 3, with
  the original-style 20-gold floor. A completed owned Market is required.
  Diplomacy can mark the two players allied, neutral, or enemy; enemy remains
  the default. Allies share vision and cannot attack or convert one another.
  Markets train 100-wood/50-gold Trade Carts, which use one population and
  repeatedly carry distance-scaled gold between an owned Market and a
  completed allied Market. Neutral and enemy Markets never permit trade.
- Civilization identity defaults to `generic` and is isolated per player.
  Briton Archers, Crossbowmen, and Arbalesters gain +1 range in Castle Age and
  another +1 in Imperial Age. Frank cavalry receive +20% base hit points,
  including future cavalry recognized by the shared cavalry classification.
  Teuton Farms cost 36 rather than 60 wood and Teuton Monks heal within eight
  tiles rather than four. DAT availability restrictions cover every currently
  represented unit, building, and technology; bonuses requiring an absent
  engine system remain omitted rather than approximated.
  Goth infantry costs 20% less in Dark Age, 25% in Feudal, 30% in Castle, and
  35% in Imperial; Goths also gain ten population capacity in Imperial Age.
  Celt Rams and Mangonels fire 25% faster. Viking infantry gain 20% HP from
  Feudal Age onward. Byzantine buildings gain
  10/20/30/40% HP by age, while Spearmen, Pikemen, Skirmishers, and Elite
  Skirmishers cost 25% less.
  Japanese infantry attack 33% faster from Feudal Age. Chinese technologies
  cost 10% less in Feudal, 15% less in Castle, and 20% less in Imperial.
  Persian Town Centers have double hit points. Persian Town Centers and Docks
  work 10/15/20% faster in Feudal/Castle/Imperial Age. Saracen Markets use a 5% fee,
  producing initial 105-gold buy and 95-gold sell prices instead of 130/70.
  Generated random maps give Chinese three extra Villagers, minus 50 wood,
  and minus 200 food; authored Scenarios remain unchanged. Bonuses lacking a
  represented damage or economy system remain explicitly omitted.
  Turkish Scout Cavalry, Light Cavalry, and Hussars gain +1 pierce armor.
  Mongol Scout-line units gain 30% HP, and Mongol Rams and Mangonels fire 25%
  faster. Spanish builders work 30% faster and Blacksmith technologies cost no
  gold. Huns need no Houses in this bounded 200-pop ruleset, and Hun Stables
  work 20% faster. Mongol hunters work 50% faster and Turkish gold miners work
  15% faster. Reciprocal allies receive Chinese +45 farm food, Byzantine 50%
  faster Monk healing, Persian Knight-line +2 attack versus archers, Saracen
  foot-archer +1 attack versus buildings, 15%-cheaper Viking Docks, and
  50%-cheaper Mayan walls. Spanish trade gold and Korean Mangonel-line minimum
  range also propagate dynamically. Aztec relic gold uses a per-player
  persisted hundredths remainder for exact 33% accumulation. Save110 stores
  both player remainders; Save107 and older migrate them to zero. Teuton
  conversion resistance remains unsupported because this deterministic
  conversion model lacks an evidenced AoC resistance formula. Cavalry Archers
  and all 18 Castle unique-unit lines are represented.
  Korean Villagers gain +3 line of sight, with Guard Tower free in Castle Age
  and Keep free in Imperial Age. Aztec Villagers carry three extra resources,
  and Aztec military units train 11% faster. Mayan archer-class units cost
  10% less in Feudal, 20% less in Castle, and 30% less in Imperial. Korean
  stone miners work 20% faster. Aztec Monks gain five hit points per researched
  Monastery technology. Generated random maps give Mayans one extra Villager
  and minus 50 food. Their finite resources last 15% longer through per-player
  credited-yield/depletion accounting, including Farms, animals, fish, and
  shared nodes. Vikings receive free Wheelbarrow in Feudal Age and free Hand
  Cart in Castle Age.
- Dark Age Docks cost 150 wood, use a 3×3 shoreline footprint, and train
  75-wood Fishing Ships. Fishing Ships use one population, travel only across
  water/fish tiles, carry 15 food, harvest finite 200-food fish nodes, and
  deposit at completed owned Docks before returning automatically. Land units
  cannot traverse fish or water. This first naval slice intentionally omits
  deep-sea/shore-fish rate differences, fish traps, naval combat, transports,
  Dock technologies, and civilization-specific ship bonuses.
- Feudal Docks train 90-wood/30-gold Galleys (120 HP, 6 attack, range 5).
  Castle War Galley research upgrades the line to 135 HP, 7 attack, range 6;
  Imperial Galleon research upgrades it to 165 HP, 8 attack, range 7.
  Galley-line ships use water-only ranged projectile combat against ships and
  shoreline buildings. Dark Age Transport Ships cost 125 wood, use one
  population, carry five adjacent friendly land units, and disembark onto an
  adjacent open shore tile. Passenger state persists; passengers sink when
  their Transport is destroyed or deleted. This bounded slice omits naval
  formations, repair-at-sea, transport capacity technologies, and naval AI.
- Castle Docks train 75-wood/45-gold Fire Ships and 70-wood/50-gold
  Demolition Ships. Imperial Fast Fire Ship and Heavy Demolition Ship research
  upgrades existing and future ships. Fire Ships use rapid range-2 attacks.
  Demolition Ships chase into contact, self-destruct, and damage every nearby
  ship regardless of diplomacy; their class-11 damage also hits shoreline
  buildings. Save, replay, and scenario formats preserve both lines.
- Imperial Cannon Galleons cost 200 wood/150 gold and fire 50%-accurate siege
  projectiles from range 13 with a three-tile minimum range. Elite Cannon
  Galleon raises HP, attack, LOS, range, armor, and building damage from exact
  live DAT records. Careening adds one ship pierce armor and five Transport
  capacity; prerequisite-gated Dry Dock raises total Transport capacity to 15
  and all current/future ship speed by 15%. Shipwright cuts eligible ship cost
  to 80% and training time to 65%. Effects, research, production, saves,
  scenarios, and replays remain deterministic.
- Castle-age Longboats are Viking-only; Castle-age Turtle Ships are
  Korean-only. Their Imperial elite research remains civilization-locked and
  upgrades current and future units. Longboats launch one damaging projectile
  plus four deterministic visual volley arrows using DAT spread values.
  Turtle Ships use one heavy siege projectile with zero minimum range.
  Exact DAT HP, attack, range, reload, armor, cost, training, and movement
  rates drive both lines. Save, scenario, and replay round trips preserve them.
- Castles train civilization-locked Longbowmen for Britons, Throwing Axemen
  for Franks, Huskarls for Goths, and Teutonic Knights for Teutons. Their
  civilization-locked Imperial research upgrades current and future units.
  Longbowmen receive archer attack/range and armor technologies; other three
  lines receive infantry attack and armor technologies. Supported DAT classes
  preserve Longbow anti-spear, building bonuses, and Huskarl anti-archer
  damage. Eagle Warriors now provide reconstructed class-29 targets. The live
  Huskarl elite prerequisite label mismatch is recorded in
  `LAND_UNIQUE_ASSET_MAP.md`; this bounded engine enforces Goth civilization
  and Imperial Age without inventing tech 270.
- Japanese Samurai, Chinese Chu Ko Nu, Byzantine Cataphract, and Persian War
  Elephant lines use exact civilization locks and live-DAT elite upgrades.
  Samurai deal proved class-19 bonus damage to unique units. Chu Ko Nu attacks
  use one damaging arrow plus proved visual volley arrows; no unsupported
  full-damage multishot is invented. Cataphracts receive cavalry blacksmith
  upgrades and proved anti-infantry damage, but no splash because their DAT
  radius is zero. Eagle Warriors now supply class-29 targets. Only Elite War
  Elephants trample: DAT radius 0.5 is enclosed
  by one adjacent integer tile, using the engine's deterministic full-damage
  splash convention. Base War Elephants have no splash.
- Right click adjacent enemy: attack.
- Select military units, press `A`, then right click: attack-move in formation.
  Units engage visible enemies en route, then continue to destination.
- Select Mangonels, press `X`, then right click any map tile: Attack Ground.
  Mangonels move into range, respect their three-tile dead zone and reload
  time, then launch one friendly-fire splash projectile. Building-selected
  `X` remains Fletching research.
- Select military units, press `P`, then right click: patrol between current
  positions and formation endpoints, returning after combat.
- Select military units, press `G`, then right click a friendly unit or
  completed building: guard it and return after nearby combat. Villager `G`
  still enters Mining Camp build mode.
- Select military units and press `C` to cycle Aggressive, Defensive, Stand
  Ground, and Passive. Colored badge and selection HUD show current stance.
  Building-selected `C` still trains Archers.
- Right click a damaged completed friendly building with a villager: repair it,
  spending wood and stone in proportion to restored hit points.
- Right click friendly Town Center with selected foot units: garrison up to 15.
  Villagers and foot archers add defensive arrows. Select occupied Town Center
  and press `E` to ungarrison everyone.
- Left click blue town center, then `V`: train villager.
- Select a production building, then `Backspace`: cancel its last
  queued unit and refund that order's full paid cost without resetting the
  active unit.
- Forward Delete removes selected units or building. On compact Mac keyboards,
  Command-Backspace performs the same action. Plain Backspace remains
  production cancellation.
- Select blue villager, press `B`, then right click nearby: build barracks.
- Select several villagers before placing a building to assign all of them;
  right click an existing friendly foundation to add more builders.
- Select blue villager, press `A`, then right click nearby: build archery range.
- Select blue villager, press `D`, then right click nearby: build stable.
- Select blue villager, press `J`, then right click nearby: build blacksmith.
- Select blue villager, press `U`, then right click nearby: build Castle in
  Castle Age.
- Select blue villager, press `I`, then right click nearby: build University
  in Castle Age.
- With a completed Blacksmith, press `2` and right click nearby to build a
  Siege Workshop in Castle Age; select it and press `3` to train a Battering
  Ram.
- Press `4` and right click nearby to build a Palisade Wall segment.
- Press F9/F10 and right click nearby to build NW-SE/NE-SW Palisade Gates.
- In Feudal Age, press Home/End and right click nearby to build
  NW-SE/NE-SW Stone Gates.
- In Feudal Age, press `5` and right click nearby to build a Watch Tower.
- In Feudal Age, press `6` and right click nearby to build a Stone Wall
  segment.
- Select blue villager, press `H`, then right click nearby: build house.
- In Castle Age, press `1` and right click nearby to build another Town Center
  for 275 wood/100 stone.
- `F` builds mill, `T` builds lumber camp, and `G` builds mining camp.
- `Y` builds farm; select a Mill and press `E` to prepay one Farm reseed.
  The player-wide queue survives Mill loss and consumes one entry when a Farm
  exhausts.
- Select stable, then `Q`: train Scout Cavalry or `K`: train Knight in Castle
  Age.
- Select barracks, then `M`: train Militia or `Z`: train Spearman in Feudal
  Age.
- In Feudal Age, select Barracks and press `9` to research Man-at-Arms.
  Existing, queued, and future Militia become Men-at-Arms.
- Select archery range, then `C`: train archer.
- Select archery range, then `7`: train Skirmisher in Feudal Age.
- In Castle Age, select Archery Range and press `=` to research Crossbowman.
- In Castle Age, select Barracks and press `-` to research Pikeman.
- In Castle Age after Man-at-Arms, select Barracks and press `[` to research
  Long Swordsman.
- In Imperial Age after Long Swordsman, press `Shift+[` to research
  Two-Handed Swordsman.
- In Imperial Age after Two-Handed Swordsman, press `Option+Shift+[` to
  research Champion.
- In Imperial Age after Crossbowman, press `Shift+=` to research Arbalester.
- In Castle Age, select Archery Range and press `Option+7` to research
  Elite Skirmisher.
- Select Siege Workshop, then `8`: train Mangonel in Castle Age.
- HUD population display shows current units and completed housing capacity.
- Select completed town center and press `N` to research next Age.
- In Dark Age or later, `]` researches Loom at a town center.
- In Feudal Age or later, `\` researches Double-Bit Axe at a lumber camp.
- In Feudal Age or later, `;` researches Horse Collar at a mill.
- In Castle Age or later, `'` researches Fortified Wall at a university.
- In Castle Age or later, `/` researches Guard Tower at a university.
- In Imperial Age, `` ` `` researches Keep at a university after Guard Tower.
- In Castle Age or later, `F1` researches Bodkin Arrow at a blacksmith after
  Fletching.
- In Imperial Age, `F2` researches Bracer at a blacksmith after Bodkin Arrow.
- In Castle Age or later, `F3` researches Iron Casting at a blacksmith after
  Forging.
- In Imperial Age, `F4` researches Blast Furnace after Iron Casting.
- In Feudal Age or later, `F12` researches Scale Mail Armor at a blacksmith.
- In Castle Age or later, `Shift+F12` researches Chain Mail Armor after Scale
  Mail Armor.
- In Imperial Age, `Ctrl+F12` (or `Cmd+F12`) researches Plate Mail Armor after
  Chain Mail Armor.
- In Feudal Age or later, `Option+F1` researches Scale Barding Armor at a
  blacksmith.
- In Castle Age or later, `Option+F2` researches Chain Barding Armor after
  Scale Barding Armor.
- In Imperial Age, `Option+F3` researches Plate Barding Armor after Chain
  Barding Armor.
- In Feudal Age or later, `Option+F4` researches Padded Archer Armor at a
  blacksmith.
- In Castle Age or later, `Option+F12` researches Leather Archer Armor after
  Padded Archer Armor.
- In Imperial Age, `Option+Shift+F12` researches Ring Archer Armor after
  Leather Archer Armor.
- In Feudal Age or later, `Option+B` researches Bloodlines at a stable.
- In Castle Age or later, `Option+Shift+B` researches Husbandry at a stable.
- In Castle Age or later, `Option+Q` researches Light Cavalry at a stable.
- In Imperial Age after Light Cavalry, `Option+Shift+Q` researches Hussar.
- In Imperial Age, `Option+K` researches Cavalier at a stable.
- In Imperial Age after Cavalier, `Option+Shift+K` researches Paladin.
- In Feudal Age or later, `W` researches Wheelbarrow at a town center, `X`
  researches Fletching at a blacksmith, and `O` researches Forging there.
- Select a completed University and press `0` to research Murder Holes.
- `S`: stop all selected units and clear their current orders.
- F5: save to `archaeology-save.txt`.
- `L`: load save.
- Space: pause/resume.
- `R`: restart match.
- F8: restart and replay current command history.
- F6: save replay to `archaeology-replay.txt`.
- F7: load and play saved replay.
- F11: toggle macOS fullscreen. Resizing preserves the 16:9 logical canvas and
  mouse hit-testing through letterboxing.
- Escape: quit.

Save and replay files live in the per-user SDL preference directory
(`~/Library/Application Support/Software Archaeology/AoE Archaeology/` on
macOS), so the Finder app never depends on its launch working directory.

## Optional original audio

The reconstruction contains and bundles no copyrighted Age of Empires audio.
It can play original audio directly from a user's legally obtained, extracted
HD installation:

- supported `.mp3` and `.wav` files directly under `Sound/stream`, in stable
  case-insensitive filename order; a single `town.mp3` retains looping
  behavior;
- `Sound/terrain/Wave1.wav`: original shoreline ambience;
- WAV resources selected through `empires2_x1_p1.dat` from `Data/sounds.drs`,
  `Data/sounds_x1.drs`, and `Data/sounds_x2.drs`. Later archives override an
  identical resource ID; missing expansion archives fall back to base.

Reactive playback covers bounded selection, movement, accepted player command,
training, attack, impact, death, building-completion, and building-destruction
events. Civilization-specific DAT records take precedence over generic records
with deterministic fallback. See
[`AUDIO_ARCHIVE_FIDELITY.md`](AUDIO_ARCHIVE_FIDELITY.md).

Set `AOE_ASSET_ROOT` to either the extracted installation directory (the one
containing `Sound`) or its parent (the one containing `app/Sound`):

```sh
AOE_ASSET_ROOT="/path/to/extracted/app" ./build/aoe_reconstruction
```

With installer files in this repository, this minimal extraction enables music
and shoreline ambience outside the source and build trees:

```sh
mkdir -p /tmp/aoe-original-audio
innoextract --output-dir /tmp/aoe-original-audio \
  --include app/Sound/stream/town.mp3 \
  --include app/Sound/terrain/Wave1.wav \
  ../setup.exe
AOE_ASSET_ROOT=/tmp/aoe-original-audio/app \
  ./build/aoe_reconstruction
```

SDL3 plays the WAV ambience without another dependency. MP3 music is enabled
when CMake finds a host `mpg123` library. It defaults on for host-SDL developer
builds and off for locked universal macOS builds, because a Homebrew library
normally contains only the host architecture:

```sh
cmake -S . -B build-audio \
  -DAOE_BUILD_SDL3=OFF -DAOE_ENABLE_MPG123=ON
cmake --build build-audio
```

If the asset root, files, decoder, or audio device is absent, gameplay
continues silently. `AOE_MUTE=1` disables all audio. `AOE_AUDIO_VOLUME` accepts
a master gain from `0.0` through `1.0` and defaults to `0.35`. Files remain in
their original location; build and bundle steps never copy them.

Match ends after one player loses all units and buildings. Simulation freezes
at final state; press `R` for a fresh match.

## Architecture

- `GameMap`: terrain ownership and spatial validity.
- `Simulation`: deterministic rules, units, commands, economy, and ticks.
- `game_rules`: explicit balance assumptions with no hidden magic numbers.
- `ComputerPlayer`: isolated controller with no renderer or private-state access.
- `game_command`: typed commands plus deterministic recording and playback.
- `scenario`: validated text parser and simulation factory.

## Scenario editing

The bundled match is [demo.scenario](resources/demo.scenario). Records describe
map size, economy, terrain rectangles, individual terrain overrides, units, and
buildings. Lines beginning with `#` are comments. Edit file, rebuild, and app
bundle receives updated scenario under `Contents/Resources`.
Building records may end with `rally X Y` to assign a starting rally point.
Scenario v28 farm records may include `resource_amount N` from 0 through 250,
covering base and Horse Collar depletion states for render audits.
Unit records may end with `attack_move X Y` to start advancing and engaging.
They may instead end with `patrol X Y` to start a repeating combat patrol.
`guard_unit X Y` or `guard_building X Y` assigns a friendly protection target.
One or more trailing `waypoint X Y` markers append queued route legs.
`stance aggressive|defensive|stand_ground|passive` sets initial behavior.
- `save_game`: explicit versioned persistence boundary.
- `SdlApp`: macOS window, input translation, timing, and rendering only.

This separation permits simulation testing without graphics and replaces the
recovered Win32/Direct3D 9 boundary with SDL3. Names describe domain intent;
binary addresses stay in the separate archaeology corpus.

## Late Castle unique units

Saracens train Mamelukes, Turks train Janissaries, Vikings train Berserks, and
Mongols train Mangudai at Castles. Imperial elite technologies preserve each
line's DAT costs, combat classes, armor, accuracy, and bonuses. Mameluke
projectiles deal melee-class damage; Mangudai receive their anti-siege bonus.
Berserks regenerate one hit point every three simulation ticks. Berserkergang
doubles that rate to two hit points every three ticks. Tick phase and researched
technology persist through saves and replays, keeping regeneration deterministic.

Aztecs, Mayans, Spanish, and Huns likewise receive their DAT-backed Castle
lines: Jaguar Warriors, Plumed Archers, Conquistadors, and Tarkans. Their
Imperial elite upgrades preserve recorded costs and timings. Combat keeps the
Jaguar anti-infantry bonus, Plumed Archer speed and anti-infantry bonus,
Conquistador gunshot accuracy, and Tarkan anti-building bonus.

## Civilization availability

Unit, building, and technology command availability comes from
`generated/civ_tech_tree_matrix.json`, extracted from the live VER 5.7 DAT.
The central `civilization_has_unit`, `civilization_has_building`, and
`civilization_has_technology` queries cover every represented enum value for
all 18 civilizations: currently 96 units, 27 buildings, and 158 technologies.
Items named by explicit DAT tech-tree disable commands receive `unavailable`;
foundation entries with a definition but no explicit gate remain available.
The synthetic `generic` civilization stays permissive for compatibility.

Missionary and monastery-tech source evidence is separately reproducible as
`generated/religious_dat_metadata.json`. It records Spanish-only Missionary
gate 84, nine technology/effect joins, task filters, SLPs, WAV resource IDs,
and every civilization disable boundary. DAT does not settle conversion
randomness/resistance, Theocracy group-charge semantics, or Heresy death and
ownership edge cases; those remain original-runtime validation work.

Represented Block Printing follows all proved effect-220 fields for Monk and
Missionary classes: +3 conversion range, line of sight, and command/search
radius. Other conversion technologies use explicit deterministic models:
Fervor applies a 1.15 speed multiplier with integer-percent rounding;
Redemption enables current siege/building targets; Atonement enables
Monk/Missionary targets; Illumination halves the bounded recharge; Faith adds
four deterministic conversion-progress ticks; Theocracy limits recharge to
the successful participant; and Heresy kills a converted unit instead of
transferring it. Their raw DAT resources do not prove exact rounding, target
filters, random resistance, recharge constants, group behavior, or
death/ownership edge cases. These behaviors remain pending validation against
a legally obtained original runtime and are not claimed as exact commercial
semantics.

Ten economy-tech records are independently reproducible as
`generated/economy_dat_metadata.json`: Horse Collar, Heavy Plow, Crop Rotation, Bow Saw,
Two-Man Saw, both gold-mining technologies, both stone-mining technologies,
and Hand Cart. It also preserves worker tasks/work rates, finite berry,
gold/stone/fish amounts, and terrain restriction records. DAT proves raw
multipliers and capacities. Represented sequential wood upgrades compose
1.20 × 1.20 × 1.10 into 1.584; both gold and stone chains compose
1.15 × 1.15 into 1.3225. Wheelbarrow and Hand Cart carry multipliers floor
sequentially from 10 to 12 to 18, while their speed multipliers compose to a
fixed-point 1.21 schedule. Heavy Plow's +1 carry applies only while actively
gathering from a Farm. Exact gather cadence and original-engine floor timing,
mid-gather remainder transitions, drop-off/retarget ordering, farm base and
reseeding behavior, terrain policy, and random-map placement remain bounded
reconstruction behavior pending original-runtime validation.

Trade and Fish Trap evidence is reproducible as
`generated/trade_dat_metadata.json`. Represented Caravan applies exact 1.5
movement multipliers through Trade Cart/Cog fixed-point numerators 150/198.
Its DAT work-rate multiplier is modeled as a bounded endpoint turnaround
reduction from three ticks to two. Cartography gates allied unit/building
vision. Coinage and Banking drive allied tribute from a completed sender
Market with bounded base/Coinage/Banking fees of 30/20/0%; raw resource-46
commands prove 0.20 and 0, but not the starting fee, transfer checks, rounding,
or resource meaning. Trade Cog death uses SLP 2116 and sound 379, completed
training uses sound 338, and Fish Trap construction uses SLP 4585.
Distance-to-gold payout and rounding, route interruption, detailed team-vision
semantics, tribute and Market fee behavior, and Fish Trap capacity,
depletion/rebuild ordering remain pending original-runtime validation.

Defensive-infrastructure source evidence is separately reproducible as
`generated/defensive_dat_metadata.json`: Outpost 598 and hidden gate 332,
Town Watch/Patrol, Masonry/Architecture, Ballistics, Heated Shot, Hoardings,
and Sappers. It retains exact costs, times, prerequisites, civilization
disables, class/projectile targets, effect commands, and Outpost SLP/WAV
links. Town Watch/Patrol and Masonry/Architecture now target only exact DAT
building classes 3/52; Masonry respects Byzantine, Aztec, and Mayan
exclusions, and both masonry technologies apply class-11 armor. Ballistics
tracks only its represented affected projectile IDs. Heated Shot changes
Watch Tower's class-16 component 7 to 15, Bombard Tower's 40 to 90, and
Castle's by +4, while excluding Town Center. Sappers applies +15 class 11,
stacks another +15
for class-13 Watch Tower, Bombard Tower, and Stone Wall targets, and excludes
Fish Trap. Fog persistence and LOS timing, projectile lead/impact behavior,
HP/armor rounding and repair interaction, and damage aggregation remain
original-runtime validation work.

Wonder/victory source evidence is separately reproducible as
`generated/victory_dat_metadata.json`: Wonder 276, hidden Wonder Plans 144,
Hun Atheism, raw resource commands 196/197, civilization boundaries, and
Wonder SLP/WAV links. The bounded reconstruction uses a typed per-player
victory countdown. A fresh Wonder/relic tie selects Wonder; while the active
kind remains qualified, its timer keeps priority and the other kind does not
advance. Losing that qualification clears it and starts any currently
qualified alternate kind at its full configured duration. Wonder qualification
continues across destruction of one completed Wonder when another completed
Wonder remains. Countdown completion is checked for both players on the same
tick: opposing winners draw, while an allied winner produces allied victory.
Resignation instead awards the other participant victory even under allied
diplomacy.

Score is bounded to stockpiled food, wood, gold, and stone plus surviving
owned unit/building HP. Score-limit qualification joins other same-tick win
conditions, but a reached time limit overrides them and resolves solely by
that score, drawing on a tie. Terminal outcomes freeze simulation updates and
reject live state-changing commands. Current Save v109 preserves match rules,
terminal outcome, and typed countdown kind/remaining state; v94 is the
compatibility gate for typed countdown kinds. Scenarios preserve match rules,
and replays preserve resignation commands but still require identical initial
match state. The HUD labels active Wonder and Relic timers by their stored
kind.

Pinned extraction exposes no victory-mode/countdown table, civilization
resource defaults, or localized messages. Countdown lengths and thresholds,
fresh/active precedence, score/time/conquest rules, destruction and
multiple-Wonder continuity, resignation, team/simultaneous resolution,
terminal behavior, and persistence above are reconstruction contracts, not
claims about commercial runtime. Ownership-transfer behavior and Hun Atheism
resource meanings remain original-runtime validation work.

Training, construction, research, AI-issued commands, and replay application
use this same boundary. Scenario v41 validates newly authored civilization
state. Scenario v40 and older plus versioned saves retain embedded legacy
state unchanged, while subsequent unavailable commands are rejected.

The first Castle unique technologies also follow live DAT commands. Yeomen
adds one range/LOS/search tile to represented foot-archer class units and two
class-3 attack to Watch Towers; it does not add tower range. Bearded Axe adds
one range/LOS/search tile to both Throwing Axeman forms. Anarchy enables the
hidden Barracks Huskarl line, including its existing Elite conversion.
Crenellations adds three range/LOS/search tiles to Castles. DAT control
resource 194 records an additional garrison-behavior toggle, but its label and
runtime contract are absent, so no capacity, cost, or unit-attack behavior is
invented.

Kataparuto, Rocketry, Logistica, and Mahouts use the next four live Castle
technology records. Kataparuto accelerates represented Trebuchet reload and
pack/unpack work; Rocketry gives Chu Ko Nu forms +2 pierce attack and
Scorpion forms +4 pierce attack. Logistica
adds the proved +6 anti-infantry class and deterministic half-tile blast
behavior to both Cataphract forms, with a splash impact marker. Mahouts
multiplies both War Elephant forms from speed 0.60 to 0.78 through the shared
fixed-point movement path.

Zealotry, Artillery, and Drill continue the live Castle technology sequence.
Zealotry adds 30 maximum and current HP to both represented Mameluke and Camel
forms. Artillery adds two range/LOS/search tiles to Cannon and Elite Cannon
Galleons; it does not target represented Bombard Cannon or Bombard Tower.
Drill multiplies represented Ram and Mangonel movement by 1.5
through a public fixed-point siege-speed path. Berserkergang is exposed
through the same Castle research/AI/availability system: live resource 96
halves its three-unit regeneration timer, represented deterministically as
two HP per three simulation ticks instead of one.

## Reconstruction status

Current code is a functional vertical slice, not the complete commercial game.
All high-impact items from the current DAT fidelity audit have graduated into
the represented roster. Exact source evidence remains recorded in
[`NEXT_IMPLEMENTATION_TARGETS.md`](NEXT_IMPLEMENTATION_TARGETS.md).

The current reconstruction-native Scenario v66 format carries the bounded
ordered typed trigger vectors introduced by v64, with deterministic priority
ordering, snapshot-before-effects evaluation,
loop-relative timers, player-scoped expiring messages, objectives, terminal
match results, and Save v109 persistence. Replay v63 reproduces those internal
firings from the same scenario and command stream. The reconstruction-native
`aoe-campaign 1` manifest provides ordered local `.scenario` missions, a
content-bound digest, linear victory-only unlocking, stale-progress detection,
and atomically replaced `aoe-campaign-progress 1` state. These formats do not
decode proprietary scenario or campaign containers.

Launching with `AOE_CAMPAIGN=/path/to/manifest.campaign` now opens a bounded
mission briefing before simulation starts. It shows campaign/scenario title,
mission order, human civilization, map dimensions, description, and visible
objectives. `Enter` or left click begins; `Escape` goes back. Victory or defeat
opens a debrief after progress is atomically committed, showing objective
results, next unlock/retry state, and continue/back controls. `F5` retains the
compact in-game campaign status panel. `resources/briefing-demo.campaign`
provides a two-mission visual audit, and `/tmp/aoe-campaign-briefing.png` is
the current 1280x720 briefing capture.

No supplied interface mapping identifies a campaign background, button, or
portrait: the proved archive mappings cover the HUD background and cursor,
while action-sheet and portrait-frame mappings remain inferred. Briefing and
debrief therefore use reconstruction-native beveled panels rather than
mislabeling unrelated art. `AOE_CAMPAIGN_NARRATION_PATH` and
`AOE_CAMPAIGN_CINEMATIC_PATH` may identify optional user-owned media and are
shown only as configured metadata; no commercial media is bundled, decoded,
or claimed compatible.

`AOE_EDITOR=1` opens the bounded reconstruction Scenario66 editor and freezes
gameplay simulation while preserving the normal isometric preview. Its panel
offers `1/2/3` Grass/Water/Forest paint, `E` elevation raise, `U` blue
Villager, `B` blue House, `X` erase, `Ctrl+Z`/`Ctrl+Y` undo/redo, left-click
application, and `Ctrl+S` validated save. `AOE_EDITOR_PATH` selects the output;
the default is the application user-data directory. The separate
`ScenarioEditor` model/controller additionally exposes resources, ages,
civilizations, diplomacy, objectives, strict triggers, and match rules for
future property panels. Validation compiles strict triggers through normal
simulation creation before Scenario66 serialization.

`resources/editor-roundtrip.scenario` exercises painted terrain/elevation,
players, placements, objectives, triggers, and match settings.
`aoe_scenario_editor_tests` proves edits, rejection, undo/redo, save/load, and
round-trip preservation. `/tmp/aoe-scenario-editor.png` is the current UI
capture. No proprietary editor format, layout, or compatibility is claimed;
the panel is reconstruction-native because the supplied archive proves no
editor-specific panel art.

`AOE_MAIN_MENU=1` opens the bounded reconstruction front end before simulation
advances. Keyboard and mouse choices cover Single Player setup, the loaded
Scenario66, configured campaign briefing, Scenario66 editor, and
preconfigured localhost Host/Join. The Single Player setup confirms player,
scenario civilization, computer difficulty source, and loaded map/rules
before `Enter` starts; `Escape` returns. Campaign and multiplayer choices
report the required `AOE_CAMPAIGN` or `AOE_MULTIPLAYER` configuration instead
of inventing file browsers or Internet discovery. `/tmp/aoe-main-menu.png` is
the current 1280x720 capture. The dark beveled framing is
reconstruction-native: archive evidence proves the existing HUD background
and cursor, not a commercial main-menu screen.

Single Player setup drives the deterministic reconstruction random-map
generator. `M` cycles Arabia/Black Forest/Islands/Rivers, `Z` cycles
Tiny/Small/Medium/Large, `-`/`+` changes the displayed seed, `C` cycles
civilizations, `D` cycles computer difficulty, and `V` cycles
Conquest/Wonder/Relic victory. Every map change regenerates and validates,
shows a deterministic hash, and updates the procedural minimap preview.
`Enter` applies the selected civilization and rules and starts that exact
generated Scenario66; validation failures remain visible with their reason.

`AOE_RANDOM_MAP_SETUP=1` and `AOE_RANDOM_MAP_SEED=N` support scripted audits.
`aoe_random_map_sdl_smoke` renders seed `424242` twice and requires
byte-identical screenshots. `/tmp/aoe-random-map-setup.png` is the current
capture. The archive-backed HUD remains available in matches; setup framing
and preview are reconstruction-native because no supplied archive mapping
proves commercial random-map setup panels.

`F9` opens a read-only civilization technology tree from setup or a running
match without changing simulation. The model lays out all 94 reconstructed
unit kinds, 27 building kinds, and 156 technology slots in Dark/Feudal/Castle/
Imperial columns. It derives available/disabled state from the generated
18-civilization availability matrices, marks already researched technologies,
and draws factual trained-at/researched-at dependency edges. Hovering shows
wood/food/gold/stone costs and age/building requirements. `Q`/`E` changes
civilization, arrows or WASD pan, `+`/`-` zoom, the mouse wheel zooms, and
right/middle drag pans; `F9` or `Escape` closes.

`AOE_TECH_TREE=1` supports scripted capture.
`aoe_technology_tree_tests` verifies complete node/dependency counts,
availability differences, researched state, and bounded layout.
`/tmp/aoe-technology-tree.png` is the current capture. Nodes use explicit
procedural colored cards because the live archive audit proves no complete
technology-icon mapping; existing archive-backed HUD/panel assets remain
unchanged underneath.

`F10` opens the read-only diplomacy/tribute surface over single-player or
multiplayer. It shows blue/red colors, civilizations and negotiated teams,
current Ally/Neutral/Enemy stance, allied-victory and Cartography shared-
vision state, current market buy/sell rates, selected tribute resource/amount,
and the Coinage/Banking-adjusted fee. `A`/`N`/`E` queues diplomacy,
`1`–`4` selects Food/Wood/Gold/Stone, `-`/`+` changes tribute in 100-resource
steps, and `Enter` queues tribute. Tribute is visibly disabled unless the
target is allied and the sender has enough resources. `T` opens All chat and
`Y` opens Allies chat in multiplayer; single-player reports chat unavailable.

Every state-changing action uses normal `SetDiplomacyCommand` or
`TributeResourceCommand`, so localhost lockstep, replay recording, ownership
checks, and single-player execution share one path. Existing core tests cover
fee tiers, replay round-trip, diplomacy, shared victory, and deterministic
execution; `aoe_diplomacy_panel_sdl_smoke` covers rendering.
`AOE_DIPLOMACY_PANEL=1` supports scripted capture, and
`/tmp/aoe-diplomacy-panel.png` is the current image. The panel is procedural
and labeled because no supplied archive mapping proves original diplomacy art.

The reconstruction also includes a transport-independent protocol-3 lockstep
core for two to eight occupied stable slots, plus a configured localhost TCP
star-relay harness and legacy two-peer nonblocking frame-loop runtime. Harness
binds accepted streams to configured stable slots and relays framed bundles
among one host and up to seven peers. Core performs occupied/active
slot ownership checks, explicit empty turns, ascending-slot canonical command
ordering, sourced Replay command framing, periodic native-Save-derived state
checksums, timeout/disconnect states, queued local input, and an immutable
canonical handshake covering build/schema/save/scenario/content identifiers,
cadence, input delay, seed, controllers, teams, cooperative-control flags,
civilizations, directed diplomacy, and explicit host slot. Exact configuration
echo is required before every occupied slot becomes ready, with categorized
mismatch states. They also provide
partial/coalesced TCP-frame handling and a separate host-sequenced bounded
UTF-8 chat stream for all/allies messages that never enters simulation hashes
or replays. A coordinated committed-tick save barrier permits a durable atomic
native Save plus checkpoint envelope v3 only after every occupied participant
reports the same state hash; loading verifies exact save, configuration,
roster, tick/hash, and ascending slot/sequence digests for a new lobby.
The interactive SDL flow still connects only host and one joiner. A separate
headless application target drives the same `Simulation`, protocol-3 session,
and localhost star relay through three real processes. It does not provide live
reconnect or commercial-save compatibility. Negotiated input delay schedules
commands at deterministic execution ticks with startup empty-bundle priming,
while monotonic ping/pong supplies RTT, peer-traffic age, waiting state, and
latency bands without influencing simulation outcomes. Synchronized
pause/resume and reconstruction-native slow/normal/fast speed
changes use host proposal, peer acknowledgement, and committed-tick control
barriers; pause and speed persist in multiplayer checkpoint envelope v3.
Five seconds without peer traffic exposes a waiting stall; thirty seconds
suspends at the last committed tick. Transport loss alone remains suspended,
not terminal. Only an explicit host drop or peer disconnect terminates, and no
AI takeover, live reconnect, ownership transfer, or host migration occurs.
The SDL application exposes an environment-driven localhost developer
integration, not a finished
user-facing network lobby. The protocol does not claim compatibility with the
commercial game.

The SDL localhost integration has a headless two-process smoke proof: host and
join execute a scripted lockstep command and ordered all/allies chat, write
their committed tick, full state checksum, and visible chat independently,
prove that the opponent cannot see an allies-only message, compare equal
simulation state, capture both screens, and exit cleanly.
Each smoke run asks the kernel for an ephemeral `127.0.0.1` port, retries host
startup up to three times for the rare close-to-bind race, uses an isolated
temporary artifact tree, and prints port/process/log/state diagnostics on an
eight-second attempt timeout. A failed full session is retried up to three
times with a fresh port and state tree. A ten-run soak is expected to pass
without shared port collisions.

The additive three-peer proof is
`aoe_multiplayer_roster_headless`. It leaves the legacy interactive
`AOE_MULTIPLAYER=host|join` behavior unchanged and requires an explicit
configured manifest:

```text
AOE_MULTIPLAYER=host|join
AOE_MULTIPLAYER_PORT=<localhost port>
AOE_MULTIPLAYER_ROSTER=0,1,2
AOE_MULTIPLAYER_LOCAL_SLOT=0|1|2
AOE_MULTIPLAYER_STATE_PATH=<proof path>
AOE_MULTIPLAYER_SCREENSHOT_PATH=<PPM path>
```

Host slot is 0. The host publishes `AOE_MULTIPLAYER_READY_PATH`; clients may
publish their per-process `AOE_MULTIPLAYER_CONNECTED_PATH`. These explicit
files let the smoke bind clients to stable slots without timing sleeps or
lobby/discovery invention. The smoke verifies canonical handshake, all-slot
ready/start quorum, three deterministic turns, one all-peer map signal, equal
state hashes, byte-equal deterministic screenshots, timeout diagnostics, and
clean exit. Run repeatability with:

```sh
ctest --test-dir build -R aoe_localhost_multiplayer_roster_headless_smoke \
  --repeat until-fail:10 --output-on-failure
```

In either the lobby or running game, `Enter` opens chat, `Tab` switches between
All and Allies, `Enter` sends, and `Escape` cancels. Chat input consumes
gameplay hotkeys only while the editor is active; it does not pause simulation.
The UI reports empty, invalid UTF-8, disconnected, and 4096-byte-limit
rejections. Hosts use `Ctrl+Enter` to start a ready lobby. For automated
audits, `AOE_MULTIPLAYER_SCRIPT_CHAT` accepts pipe-separated `all:` and
`allies:` messages; the multiplayer state proof includes ordered `chat` lines.
This remains a developer verification path, not a finished Internet lobby.

The lobby and running-game overlays also show the negotiated input delay,
measured heartbeat RTT and green/yellow/red band, peer-traffic age, and the
five-second waiting indication. These values are presentation-only and never
advance simulation. `F6` lets the host request a coordinated save barrier two
ticks ahead. Both peers pause at the committed tick and compare state hashes;
the overlay reports collecting, matched, or mismatch. On a match the host
atomically writes the current versioned save and its strict multiplayer
envelope, immediately
reloads both for verification, and displays the save path. Automated runs can
set `AOE_MULTIPLAYER_INPUT_DELAY`, `AOE_MULTIPLAYER_SCRIPT_CHECKPOINT`, and
`AOE_MULTIPLAYER_CHECKPOINT_PATH`. The two-process smoke negotiates a
three-tick delay, proves a matched tick-12 barrier, and verifies both durable
checkpoint files.

Host-only `F7` proposes pause/resume and `F8` cycles Normal, Fast, and Slow.
The peer acknowledges proposals before they commit at the shared barrier tick.
Both overlays show proposal/ack feedback, committed pause state, speed, and
effective cadence; a centered pause banner appears while paused. Chat and
network pumping remain active, while gameplay commands are rejected until
resume. `AOE_MULTIPLAYER_SCRIPT_CONTROL=1` makes the two-process smoke commit
Fast, commit Pause, prove the committed tick remains frozen across 24 rendered
frames, commit Resume, and then complete the matched checkpoint. Both peers
must report running, Fast, 100ms cadence, and an unchanged equal state hash.
These speed values and control policy are reconstruction-native.

Remaining full-game work also includes complete original resource-format
coverage, the rest of the construction and technology trees, proprietary
campaign/scenario import and cinematics, production multiplayer UI/lobby,
complete reactive sound,
UI panels, and broader behavioral validation against legally obtained builds. Existing
computer-player behavior is substantial but does not reproduce the complete
commercial AI.

## macOS compatibility

Default macOS builds compile the locked SDL3 source and set both app and
embedded SDL3 slices to a macOS 11.0 minimum deployment version. This is the
supported distributable configuration.

Developer builds using `-DAOE_BUILD_SDL3=OFF` use an installed SDL3 package.
CMake does not claim macOS 11 compatibility for that configuration: its
minimum OS and available architectures follow the selected SDK and SDL3
package unless the developer explicitly supplies compatible
`CMAKE_OSX_DEPLOYMENT_TARGET` and `CMAKE_OSX_ARCHITECTURES` values.
# Deterministic random maps

`aoe/random_map.hpp` exposes seeded, reconstruction-native Arabia, Black
Forest, Islands, and Rivers generators with four size presets. Generated maps
include fair two-player Dark Age starts, deterministic terrain/resources and
elevation, validation with bounded retry, stable map hashing, and Scenario66
serialization. See [RANDOM_MAP_FIDELITY.md](RANDOM_MAP_FIDELITY.md).

`aoe/rms_import.hpp` adds strict bounded inspection/import for an
evidence-backed subset of classic `.rms` sections and directives. Unsupported
map semantics retain exact line spans and refuse playable output. Accepted
scripts deterministically evaluate into Scenario66; full RMS compatibility is
not claimed. See [RMS_IMPORT_FIDELITY.md](RMS_IMPORT_FIDELITY.md).

`aoe/classic_save_import.hpp` provides bounded, read-only inspection of the
classic Genie save/record envelope: exact raw preservation, strict raw-DEFLATE
handling, proved version metadata, and structured diagnostics. Player/map/tick
metadata and conversion remain unavailable where evidence is incomplete. This
is not project Save110 compatibility. See
[SAVE_IMPORT_FIDELITY.md](SAVE_IMPORT_FIDELITY.md).

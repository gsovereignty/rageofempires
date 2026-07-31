# Native macOS reconstruction

Readable, clean-room C++ reconstruction informed by static analysis of the
supplied Age of Empires II HD binary. This is new source code, not mechanically
cleaned Ghidra output and not a claim of source compatibility.

Recent camera, menu, building-age graphics, resource-bar, terrain-boundary,
and fullscreen/resize issues are implemented and automated checks pass, but
none has yet received human interactive verification. See
[`IMPLEMENTATION_STATUS.md`](IMPLEMENTATION_STATUS.md#recent-issue-closure-human-verification-pending)
for per-issue evidence and required manual checks. Do not describe these six
items as human-verified or accepted yet.

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
  [`docs/contracts/RESIGNATION_OBSERVER.md`](../contracts/RESIGNATION_OBSERVER.md).
- Bounded classic `.ai`/`.per` inspection parses constants, loads, weighted
  random loads, and `defrule` facts/actions with exact unsupported-span
  preservation. A strict mapped subset emits deterministic typed intents under
  rule/action budgets; unknown semantics block executable mode. See
  [`docs/fidelity/AI_SCRIPT_FIDELITY.md`](../fidelity/AI_SCRIPT_FIDELITY.md).
- Authoritative two-player match statistics cover gathered resources, tribute,
  military and building totals, conversions, relics, research, age timings,
  Wonders, live score, and deterministic timeline samples. Save110 persists
  the model; [`docs/contracts/MATCH_STATISTICS.md`](../contracts/MATCH_STATISTICS.md) defines its UI API
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
  [`docs/fidelity/FORMATION_FIDELITY.md`](../fidelity/FORMATION_FIDELITY.md) defines these bounded
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

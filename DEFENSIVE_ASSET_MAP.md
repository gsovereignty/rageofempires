# Defensive infrastructure DAT asset map

## Evidence boundary

`generated/defensive_dat_metadata.json` is generated from validated VER 5.7
DAT metadata. It preserves raw entity, technology, effect, target, task,
civilization, graphic, and sound records.

DAT proves numeric LOS, HP, armor, attack, target-class, and projectile
commands. It does not specify fog reveal persistence, LOS update timing,
Ballistics lead/impact algorithms, Heated Shot target filtering and damage
resolution, HP/armor rounding and repair interaction, or Sappers ownership and
attack resolution. Those remain original-runtime validation targets.

## Outpost

Outpost 598 has 500 HP, LOS 6, search radius 3, terrain restriction 4, and
zero armor in classes 21, 11, 4, and 3. A Villager builds its 1×1 footprint in
slot 6 for 25 wood, 10 stone, and 15 source seconds. It has no attack and no
task list.

Hidden technology 332 `Outpost (make avail)` has no prerequisites or research
location. Effect 331 enables object 598. All 18 civilization trees retain the
gate.

Standing graphic 3223 maps to SLP 4330. Construction graphic 118 maps to SLP
236 with one frame and three angles. Death graphic 37 maps to SLP 73 and
triggers sound 323, whose WAV resources are 5316–5318 and 5459. Selection and
construction sound 23 maps to WAV resource 5276.

These are DAT graphic-to-SLP and conceptual sound-to-WAV links. Archive
presence and decoding require separate DRS validation.

## LOS technologies

Town Watch 8 requires Feudal Age 101 and researches at Town Center 109 in slot
8 for 75 food and 25 source seconds; icon 69. Effect 8 adds four LOS to object
classes 3 and 52.

Town Patrol 280 requires Castle Age 102 plus Town Watch 8. It researches at
Town Center 109 in slot 8 for 300 food, 200 gold, and 40 source seconds; icon
89. Effect 280 adds another four LOS to the same two classes. Both technologies
are available to all 18 civilizations.

Represented class 3 comprises Town Center, Barracks, Archery Range, House,
Mill, Lumber Camp, Mining Camp, Stable, Blacksmith, Castle, University, Siege
Workshop, Monastery, Market, Dock, and Outpost. Class 52 comprises Watch Tower
and Bombard Tower. Town Watch and Town Patrol apply only to those structures;
Farm, Fish Trap, walls, and gates are excluded.

The class targets and +4 increments are DAT facts. Fog reveal cadence,
explored-fog persistence, and visibility changes during research remain
runtime behavior requiring validation.

## Masonry and Architecture

Masonry 50 requires Castle Age 102. It costs 175 wood/150 food, takes 50
source seconds at University 209 slot 1, and uses icon 13. Architecture 51
requires Imperial Age 103 plus Masonry 50. It costs 200 wood/300 food, takes
70 source seconds at the same location/slot, and uses icon 14.

Each effect independently applies the same commands to object classes 3 and
52: multiply HP by 1.1, add one armor in classes 4 and 3, and add three armor
in class 11. The two HP multipliers therefore compose numerically to 1.21 in
DAT. Represented scope is exactly the class-3/class-52 list above; Farm, Fish
Trap, walls, and gates receive neither technology. Class-11 armor participates
in building-bonus resolution separately from the one-point class-4/class-3
armor additions. Exact integer rounding, current-HP adjustment, repair
cost/rate, and damage-floor interaction are not established by these commands.

Masonry is unavailable to Byzantine, Aztec, and Mayan civilizations.
Architecture is unavailable to Germans, Japanese, Byzantine, Saracens,
Mongols, Celts, Aztecs, and Huns.

## Ballistics and Heated Shot

Ballistics 93 requires Castle Age 102. It costs 300 wood/175 gold, takes 60
source seconds at University 209 slot 4, and uses icon 25. Its 61 raw commands
set attribute 19 to 1 across 44 unique projectile IDs; repeated IDs are
preserved in the effect record. Ballistics is available to all civilizations.
Represented direct projectile/source mappings are:

- 54 Town Center; 363 Archer; 364 Crossbowman; 365 Skirmisher; 366 Elite
  Skirmisher; 372 War Galley; 373 Galleon.
- 477 Mangudai, Elite Mangudai, and Cavalry Archer; 478 Heavy Cavalry Archer.
- 504/505 Watch Tower; 506 Bombard Tower; 507 Arbalester.
- 510 Chu Ko Nu forms; 511 Longbowman and Plumed Archer forms; 512 Longboat
  forms; 540 Galley; 746 Castle; 767 Turtle Ship forms.

Other effect IDs remain preserved without a represented direct source. Only
listed affected projectile IDs gain represented Ballistics tracking. The DAT
flag does not define target prediction, lead distance, collision, misses,
projectile lifetime, or moving-target impact.

Represented projectile policy is deliberately narrower than the DAT evidence:

- A unit or defensive building may acquire and continue attacking only a
  currently visible enemy. Explored-but-fogged tiles do not expose targets.
- Accuracy is resolved deterministically at launch. A miss keeps a visual
  projectile aimed at the launch-time tile but carries no target and cannot
  deal direct impact damage. Without Ballistics, even an accurate projectile
  retains that fixed tile and misses a unit that moves away before impact.
- For the listed Ballistics sources, an already-launched accurate projectile
  follows the live target tile until impact. A target entering fog after a
  legal visible launch does not break that existing projectile lock; fog still
  prevents a new acquisition. This is represented lock-retention policy, not
  proof of original-runtime hidden-target behavior.
- Projectile travel duration, target lock, origin/destination, damage, attack
  class, source kind, splash geometry, and speed are save-persistent. Mid-flight
  Ballistics replay is deterministic.
- Thumb Ring's DAT-backed attack-rate effect is represented separately.
  Represented qualifying archer shots also receive automatic launch accuracy;
  that accuracy interpretation is policy because the decoded records do not
  establish the original miss algorithm. Thumb Ring and the other added
  standard technologies are now preserved by game saves.

Heated Shot 380 requires Castle Age 102. It costs 350 food/100 gold, takes 30
source seconds at University 209 slot 3, and uses icon 104. Raw effect 378
contains a class-52 attack multiplier command with packed value 4321, decoded
as attack class 16 and unsigned amount 225, plus an explicit Castle 82
attack-class-16 addition of four. The multiplier payload represents 225% of
the existing class-16 attack component, or a 125% increase. The raw class and
amount are authoritative; mapping class 16 to ships, selecting targets, and
resolving bonus damage require original-runtime validation.

Represented class-52 outcomes floor Watch Tower's class-16 component from 7
to 15 and scale Bombard Tower's 40 to 90. Castle is class 3 and receives only
its explicit +4. Town Center is class 3 with no explicit command and is
excluded. These numeric represented outcomes do not establish original
rounding, armor aggregation, projectile ownership, or target-selection order.

Heated Shot is unavailable to French, Japanese, Byzantine, Saracens, Mongols,
Spanish, and Huns.

## Hoardings and Sappers

Hoardings 379 requires Imperial Age 103. It costs 400 wood/400 food, takes 75
source seconds at Castle 82 slot 11, and uses icon 103. Effect 377 multiplies
Castle 82 HP by 1.21. It is unavailable to Goths, Japanese, Chinese, Aztecs,
Huns, and Koreans. Current-HP adjustment and repair interaction remain runtime
validation.

Sappers 321 requires Imperial Age 103. It costs 400 food/200 gold, takes 10
source seconds at Castle 82 slot 12, and uses icon 5. Effect 321 adds 15 attack
to object class 4 against attack classes 11 and 13. Represented Villagers gain
the class-11 component against every represented building except Fish Trap,
whose armor record contains neither class. Watch Tower, Bombard Tower, and
Stone Wall also contain class 13, so both +15 components stack to +30 against
those targets. Fish Trap receives no Sappers bonus. The DAT target classes and
amounts are exact; ownership filters, damage floors, armor aggregation, and
attack timing remain runtime behavior.

Sappers is unavailable to French, Japanese, Byzantine, Saracens, and Koreans.

## Represented defensive firing policy

Base attack, armor, reload, accuracy, projectile, minimum range, maximum range,
and upgrade values come from the bounded DAT records above. Runtime targeting
and extra-projectile rules remain reconstruction policy:

- Town Centers fire only when a contributing occupant exists, capped at ten
  projectiles. Castles add contributing occupants to their base volley up to
  20. Towers add them up to five.
- Villagers and represented archers contribute one projectile each. Accepted
  melee occupants provide shelter and healing but add no projectile.
- Nearest currently visible enemy inside circular range is selected.
  Garrisoned, dead, Relic, allied, neutral, hidden, and minimum-range targets
  are excluded. Murder Holes removes minimum range except for Bombard Tower.
- Projectile hit selection is a stable tick/building/target/arrow hash against
  decoded accuracy. This is deterministic reconstruction, not a recovered
  commercial RNG.
- Diplomacy is checked again at projectile impact. A volley fired before an
  enemy becomes allied causes no damage after the transition.

Guard Tower and Keep technologies retain the represented Watch Tower entity
kind while applying their decoded HP, attack, armor, and technology-dependent
range rules. This avoids inventing a runtime object replacement step.

Tests compare Castle volleys with a melee occupant, Villager, and Archer;
Villager and Archer each add exactly one represented projectile while melee
does not. Another test changes diplomacy after a Castle fires and proves the
in-flight volley causes no allied damage. Existing tests cover DAT capacities,
healing, save/replay, Castle and tower attacks, upgrades, Ballistics,
minimum-range/Murder Holes behavior, ship bonuses, armor, and destruction.

## Regeneration

```sh
python3 tools/dat_metadata/generate_defensive_metadata.py \
  /tmp/aoe-metadata.json
```

Live fixture comparison:

```sh
AOE_TEST_METADATA=/tmp/aoe-metadata.json \
  python3 -m unittest tools/dat_metadata/test_generate_defensive_metadata.py
```

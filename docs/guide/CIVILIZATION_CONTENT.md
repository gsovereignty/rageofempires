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

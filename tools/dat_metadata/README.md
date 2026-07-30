# VER 5.7 metadata extractor

This opt-in research tool reads a legally supplied
`Data/empires2_x1_p1.dat` and writes JSON to standard output. It commits no
game data. The parser dependency is pinned to the exact `genie-rs` revision
validated against the supplied HD installer.

```sh
cargo run --quiet \
  --manifest-path tools/dat_metadata/Cargo.toml \
  -- /path/to/empires2_x1_p1.dat > /tmp/aoe-metadata.json
```

The command fails unless the expected profile is present: 514 effects and 19
civilizations. Each civilization contains its present units and their
standing (both slots), dying, walking, running, attack, and construction
graphic links. It also emits unit/base class, HP, LOS, speed, resource
tracking/group, copy ID, availability flags, button/portrait indices, combat
stats including area/blast fields, primary and volley missile links/settings, creation
costs/location/button/time, three-slot static resource attributes, and tech names.
It also emits exact three-axis static and outline radii, obstruction and
selection shapes, unit group, moving size class, turn/trailing/move-algorithm
fields, unit sound links, graphic sound triggers, conceptual sound
resource/probability entries, and direct graphic records with SLP, animation,
palette, and child-layer metadata. Terrain output includes direct terrain
records plus the 16-record legacy terrain-border table, including SLP links,
colors, underlays, styles, and frame grids. These object fields are inputs available to
formation reconstruction; they do not encode formation geometry or runtime
group-movement policy.

## Validated live regeneration

The 2026-07-29 freshness sweep used VER 5.7 DAT SHA-256
`e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`
and `graphics.drs` SHA-256
`b0541bbf9dc45cdef85eb50563d5412026efc56306334b66a88b56557d11bfdf`.
The extractor produced 514 effects, 19 civilizations, and 7,014 graphics.

| Generated artifact | Primary record count | SHA-256 |
|---|---:|---|
| `civ_tech_tree_matrix.json` | 18 civilizations | `ccacaa351e836b04be43dcd9035f699535dfb480530321b57c1b56b45951fc81` |
| `combat_geometry_dat_metadata.json` | 14 weapons, 8 projectiles | `8777e4cbc0d8420810a6316fa119b30505c383fb57e6f47ce5c3cd8c96035831` |
| `defensive_dat_metadata.json` | 8 entities, 8 technologies | `fcfbaf6d2ba904c2325b006fc9910d7f99e1880516a107cd340037dabdd32e63` |
| `economy_dat_metadata.json` | 10 technologies, 24 units | `22f18b18b0686075e1fddf1e08b2f3049fabf74a3efe213a229344eab5418cc4` |
| `garrison_dat_metadata.json` | 6 buildings | `971a9c4cadfc3e566354689840bc1f8bd4e62b3450a3b797b8d160eda574d047` |
| `procedural_building_dat_metadata.json` | 8 mappings, 160 graphics | `1105bf3dc2a3846c22cc384f37b732ef0dd9c59df76f4b88080501ff7c40b089` |
| `religious_dat_metadata.json` | 9 technologies | `dc140907423a5b6c156f9e487678e9b3c6b23a83ce6c200b66206010122e1282` |
| `renderer_asset_coverage.json` | 96 units, 27 buildings | `b4952e23fc322e2e0c7fa94e14efebe9ce7a28ffd52b419db35b9530988e035b` |
| `trade_dat_metadata.json` | 2 entities, 5 technologies | `21484be851441a1fb7247469fa7b51344da224c81cf7c3be14ff7d8e07421722` |
| `victory_dat_metadata.json` | 18 civilization boundaries | `865a07d901b44aca6b5d2ba25a8976301b8e48214a3f8c39d5c7b938fd0c2d7e` |

Combat records additionally preserve raw break-off, weapon offset,
missed-missile spread, static area-effect level, rear/flank modifiers, and
missile flags/ballistics ratio. Field names and values do not establish their
runtime geometry or targeting semantics.
Action-capable units additionally expose default task, search radius, work
rate, command/move sounds, and their task list. Task lists inherited through a
unit's copy ID are marked `global_copy_id`; the task payload is parser debug
text because this pinned `genie-rs` revision does not expose public task
accessors.

Tech entries include the pinned parser's complete debug record because that
revision exposes only tech names through public accessors. Effect commands
retain raw parameters and add known attribute names plus decoded packed-attack
class/amount fields.

Generate represented gameplay availability for all playable civilizations:

```sh
python3 tools/dat_metadata/generate_civ_matrix.py \
  /tmp/aoe-metadata.json
```

This writes `generated/civ_tech_tree_matrix.json` and fails when current
`UnitKind`, `BuildingKind`, or `Technology` enum coverage changes without an
explicit DAT mapping.

Generate the exhaustive represented core-rules drift report:

```sh
python3 tools/dat_metadata/generate_core_rules_drift.py \
  /tmp/aoe-metadata.json
```

This writes `generated/core_rules_drift.json` for all 94 `UnitRules` and 27
`BuildingRules` records. Direct decoded fields are `exact` or `mismatch`;
seconds/ticks and absolute/relative speed fields are `transformed`; fields
owned outside these rules structs are `intentionally_policy`. The generator
does not infer formulas or translate opaque DAT fields.

Generate the exhaustive represented technology/effect matrix:

```sh
python3 tools/dat_metadata/generate_technology_effect_matrix.py \
  /tmp/aoe-metadata.json
```

This writes `generated/technology_effect_matrix.json` for all 156 represented
technologies. It joins research costs, time, location, all 18 civilization
availability statuses, and every decoded effect command to an implemented
semantic hook. Hidden technologies without a DAT research location and
commands whose attribute/resource remains undecoded are explicitly `policy`;
they are never guessed into gameplay effects.

Generate the exhaustive civilization-bonus matrix:

```sh
python3 tools/dat_metadata/generate_civilization_bonus_matrix.py \
  /tmp/aoe-metadata.json
```

This writes `generated/civilization_bonus_matrix.json` for all 18 AoC
civilizations. It preserves every raw civilization and represented unique-tech
effect command, records isolation/age/save/replay/random-map coverage, and
classifies team bonuses only when reciprocal-alliance runtime hooks implement
their exact contract. Chinese farm capacity, Byzantine healing, Persian
anti-archer damage, Saracen anti-building damage, Viking Dock cost, and Mayan
wall cost join Spanish trade gold, Aztec relic gold with a persisted
hundredths remainder, Korean minimum range, and the previously represented
production/LOS bonuses. Teuton conversion resistance remains
`team-unsupported` because conversion lacks an evidenced AoC resistance
formula. Undecoded tech-tree disable/resource commands remain `policy`; the
generator does not infer opaque meanings.

Generate the exhaustive UI icon evidence catalog:

```sh
python3 tools/dat_metadata/generate_ui_icon_catalog.py \
  /tmp/aoe-metadata.json \
  /path/to/Data/interfac.drs
```

This writes `generated/ui_icon_catalog.json` for all represented units,
buildings, technologies, resources, and commands, plus the complete external
interface-archive inventory. DAT icon indices and SLP frame counts are exact
independent facts. A sheet/frame relationship remains `unknown` unless a
separate source proves it; the generator never treats an icon index as a frame
number by arithmetic.

Generate bounded Missionary and monastery evidence:

```sh
python3 tools/dat_metadata/generate_religious_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/religious_dat_metadata.json`. It joins Missionary 775
and its hidden Spanish gate to tasks, graphics/SLPs, sounds/WAV resources, nine
monastery technology records, raw effect commands, and all 18 civilization
disable boundaries. Conversion randomness/resistance, Theocracy group
semantics, and Heresy death/ownership behavior remain original-runtime
validation, not DAT claims.

Generate bounded economy/resource evidence:

```sh
python3 tools/dat_metadata/generate_economy_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/economy_dat_metadata.json`, joining ten economy
technology/effect records to civilization disables, worker task/work-rate
records, resource amounts, terrain restrictions, and terrain summaries.
Gather cadence/rounding, drop-off and reseed ordering, terrain policy, and
random-map placement remain original-runtime validation.

Generate bounded trade and Fish Trap evidence:

```sh
python3 tools/dat_metadata/generate_trade_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/trade_dat_metadata.json`, joining Trade Cog 17, Fish
Trap 199, their hidden gates, five Market technologies, supporting Market,
Dock, Fishing Ship, and Trade Cart tasks, graphics/SLPs, sound/WAV links, and
all civilization disable boundaries. Distance payout/rounding, team vision,
Market fee behavior, and Fish Trap depletion/rebuild ordering remain
original-runtime validation.

Generate bounded defensive-infrastructure evidence:

```sh
python3 tools/dat_metadata/generate_defensive_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/defensive_dat_metadata.json`, joining Outpost 598 and
its hidden gate to eight defensive technologies, raw target classes and
projectile IDs, supporting building/Villager records, graphics/SLPs,
sounds/WAVs, and all civilization disable boundaries. Fog persistence,
Ballistics lead/impact behavior, Heated Shot filtering, HP/armor rounding, and
Sappers damage resolution remain original-runtime validation.

Generate bounded garrison-field evidence:

```sh
python3 tools/dat_metadata/generate_garrison_metadata.py \
  /tmp/aoe-metadata.json
```

The base extractor now preserves raw `garrison_capacity`, `garrison_type`,
`garrison_heal_rate`, and `garrison_repair_rate` fields. `garrison_type` stays
numeric because the pinned library does not decode its accepted-unit mask.
See `../../docs/assets/GARRISON_ASSET_MAP.md`.

Generate bounded radial-combat and attack-dispersion evidence:

```sh
python3 tools/dat_metadata/generate_combat_geometry_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/combat_geometry_dat_metadata.json`, preserving exact
range, area/blast level, miss/volley spread, weapon offset, projectile link,
and missile fields for represented artillery, explosive units, Scorpions, and
Bombard Tower. Shape, distance falloff, target filtering, friendly fire,
collision, and projectile placement remain original-runtime validation. See
`../../docs/fidelity/COMBAT_GEOMETRY_FIDELITY.md`.

Generate bounded Wonder and victory evidence:

```sh
python3 tools/dat_metadata/generate_victory_metadata.py \
  /tmp/aoe-metadata.json
```

This writes `generated/victory_dat_metadata.json`, joining Wonder 276 and
Wonder Plans 144 to Hun Atheism, raw resources 196/197, civilization
boundaries, graphics/SLPs, and sounds/WAVs. Countdown duration, relic
thresholds, score/time/conquest rules, team semantics, localized messages, and
Atheism resource meanings remain original-runtime validation.

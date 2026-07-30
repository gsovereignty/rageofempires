# Civilization availability matrix

## Deliverables

`generated/civ_tech_tree_matrix.json` is core-importable source data for all
18 playable classic/AoC civilizations. It covers every currently represented
`UnitKind` (96), `BuildingKind` (27), and `Technology` (158), retaining enum
name, exact DAT ID, global definition presence, per-civilization definition
presence, and final availability status.

Regenerate from extractor JSON:

```sh
cargo run --quiet --manifest-path tools/dat_metadata/Cargo.toml -- \
  /path/to/empires2_x1_p1.dat > /tmp/aoe-metadata.json
python3 tools/dat_metadata/generate_civ_matrix.py \
  /tmp/aoe-metadata.json
```

Generator fails if any enum is added, removed, or reordered without an explicit
DAT mapping. This makes stale coverage a hard error instead of silently
defaulting a new gameplay type to all civilizations.

## Status semantics

- `available`: civilization can build, train, or research represented item.
- `unavailable`: definition exists, but civilization tech-tree effect disables
  its gate, or civilization-specific item belongs to another civilization.
- `definition_only`: record is valid game data but not a civilization
  production choice. Current cases are sheep, deer, boar, and relic.
- `missing_definition`: mapped record is absent. Live validation currently
  produces none.

`definition_exists` is global across Gaia plus playable civilizations.
`definition_exists_in_civ` is narrower and explains why neutral deer, boar,
and relic records exist globally but do not appear in playable civilization
unit arrays.

## Evidence model

Each playable civilization points at one tech-tree effect:

| Civ IDs | Tech-tree effect IDs |
|---|---|
| British 1, French 2, Goth 3, German 4, Japanese 5, Chinese 6 | 254, 258, 259, 262, 255, 257 |
| Byzantine 7, Persian 8, Saracen 9, Turk 10, Viking 11, Mongol 12, Celt 13 | 256, 260, 261, 263, 276, 277, 275 |
| Spanish 14, Aztec 15, Mayan 16, Hun 17, Korean 18 | 446, 447, 449, 448, 504 |

Type-102 commands in these effects are authoritative disabled-technology
records. Generator joins them to explicit gates for units and buildings.
Civilization-specific technologies and unique-unit gates also carry an owner,
preventing shared definitions from becoming trainable by every civilization.
Unit/building definition presence comes from each civilization's own unit
array, not Gaia or a global prototype alone.

Most Dark Age foundations have no researchable gate. For those, a present
civilization record is availability evidence. Castle has two live gates:
ordinary Castle tech 137 and Turk replacement tech 354. Stable uses live
`STBL (make avail)` tech 25; this is disabled for Aztec and Mayan trees.

## Compact matrix summary

Counts exclude four `definition_only` objects.

| Civilization | Available units | Available buildings | Available technologies | Represented building exclusions |
|---|---:|---:|---:|---|
| British | 42 | 26 | 82 | none |
| French | 46 | 26 | 80 | none |
| Goths | 46 | 23 | 79 | stone wall, both stone gates |
| Germans | 45 | 27 | 86 | none |
| Japanese | 45 | 26 | 80 | none |
| Chinese | 46 | 27 | 86 | none |
| Byzantine | 51 | 27 | 90 | none |
| Persians | 49 | 26 | 84 | none |
| Saracens | 48 | 26 | 86 | none |
| Turks | 45 | 27 | 86 | none |
| Vikings | 44 | 26 | 80 | none |
| Mongols | 49 | 26 | 80 | none |
| Celts | 44 | 26 | 77 | none |
| Spanish | 48 | 27 | 90 | none |
| Aztecs | 36 | 25 | 72 | stable |
| Mayan | 39 | 25 | 78 | stable |
| Huns | 42 | 26 | 73 | none |
| Koreans | 45 | 26 | 80 | none |

Full per-item truth table lives in JSON; repeating its 4,860 status cells here
would create a second, drift-prone source.

Missionary 775 is Spanish-only through hidden technology 84 ownership, not
unit-definition presence. Nine monastery technology boundaries come directly
from each civilization's type-102 disables. Faith and Fervor are available to
all 18; narrower per-technology lists and raw effect evidence live in
[`RELIGIOUS_ASSET_MAP.md`](RELIGIOUS_ASSET_MAP.md).

Nine economy technology boundaries also use type-102 disables. Heavy Plow,
Bow Saw, Gold Mining, Stone Mining, and Hand Cart are available to all 18;
upgrade exclusions and raw multiplier evidence live in
[`ECONOMY_ASSET_MAP.md`](ECONOMY_ASSET_MAP.md).

Trade Cog and Fish Trap gates are available to all 18 civilizations. Coinage,
Banking, Cartography, and Caravan are also universal; Guilds is unavailable to
French, Japanese, Chinese, Saracens, Vikings, Mongols, and Aztecs. Raw tasks,
effects, graphics, and runtime-validation boundaries live in
[`TRADE_ASSET_MAP.md`](TRADE_ASSET_MAP.md).

Outpost, Town Watch, Town Patrol, and Ballistics are available to all 18
civilizations. Masonry, Architecture, Heated Shot, Hoardings, and Sappers use
their exact type-102 disable boundaries. Raw target classes, projectile IDs,
effect commands, and exclusions live in
[`DEFENSIVE_ASSET_MAP.md`](DEFENSIVE_ASSET_MAP.md).

Wonder and hidden Wonder Plans are available to all 18 civilizations. Atheism
remains Hun-only through technology ownership. Wonder tasks/assets, raw
victory resources, and parser/runtime boundaries live in
[`VICTORY_ASSET_MAP.md`](VICTORY_ASSET_MAP.md).

## Represented-scope boundary

Matrix completeness means complete coverage of current enums, not complete
commercial-game coverage. Missionary, monastery technologies, and nine
economy technologies have graduated into represented enums. Expanded resource
ecology runtime behavior, trade/naval runtime behavior, defensive and victory
runtime behavior, formation, AI, campaign, and multiplayer work remains
outside this matrix and must be assessed against current code before planning.

Six common standard technologies now also come directly from live VER 5.7
records: Tracking 90, Squires 215, Parthian Tactics 436, Thumb Ring 437,
Herbal Medicine 441, and `Stone cutting` 54. The latter name deliberately
follows the internal DAT record; player-facing text calls this technology
`Treadmill Crane`, so UI display uses that public name while persistence keeps
the stable `stone_cutting` token. No absent Gillnets record is invented.
Effects use records 90, 204, 452, 451, 41, and 54 respectively.

Spy Technology 408 is the remaining normal player-clickable common AoC
technology. Its effect 420 sets resource 183, its DAT base price is 200 gold,
and its one-second Castle research is Imperial-gated. Runtime applies original
dynamic pricing: base gold multiplied by living enemy Villager count, then
normal civilization research discount. Researched Spies reveal living enemy
units and buildings without falsely marking unrelated map tiles explored.

## Ambiguous and dangling records

- Tech 100 and tech 99 are both named Crossbow. Current represented upgrade
  maps to tech 100 because live upgrade gating and civilization disables use
  that record. Tech 99 is retained only as an unrepresented duplicate.
- Elite Huskarl tech 365 requires tech 270 (`Berserker (make avail)`) while
  Goth availability uses tech 446 (`Huskarl (make avail)`). Matrix applies the
  explicit Goth ownership plus live disable set and records this dangling
  prerequisite mismatch rather than treating Berserk as Goth-trainable.
- Availability tech 273 is named `Mobile Siege Unit (make avail)` but upgrades
  unit 11 Mangudai. Mapping follows effect 369 and unit records.
- Hand Cannoneer and Bombard Cannon availability follows technology gates 85
  and 188. Hidden enabling effects 174 and 172 have broader civilization
  disable sets and are not substituted as gates. Required record 285 supplies
  alternative Turk free-Chemistry machinery; it is not a literal extra
  prerequisite for every civilization.
- Several type-102 slots contain `d=-1`; generator ignores these empty
  sentinels.
- Palisade and stone gate orientations use separate DAT IDs but share one
  gameplay gate per material.
- Tech prerequisites are not currently emitted structurally by pinned
  `genie-rs`; ownership and disable commands provide final availability for
  represented types. Cost/age prerequisites remain gameplay sequencing, not
  civilization availability.

## Validation

```sh
python3 -m unittest tools/dat_metadata/test_generate_civ_matrix.py
AOE_TEST_METADATA=/tmp/aoe-metadata.json \
  python3 -m unittest tools/dat_metadata/test_generate_civ_matrix.py
cargo test --manifest-path tools/dat_metadata/Cargo.toml \
  --target-dir /tmp/aoe-dat-metadata-target
```

Live regeneration validates exact enum coverage, 18 civilization rows, known
unique-unit locks, Aztec/Mayan stable exclusion, Goth stone-wall exclusion,
Turk Castle replacement, Korean Turtle Ship ownership, and byte-equivalent
JSON output.

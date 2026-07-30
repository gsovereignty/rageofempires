# Commercial Scenario Import Fidelity

## Non-requirement

Commercial scenario compatibility is not required for this project. This
module is optional, bounded archaeology tooling; incomplete mappings and
non-playable commercial files do not block releases or project completion.
Native Scenario files are the supported runtime contract.

## Outcome

The reconstruction now recognizes and validates classic Genie `.scn` and
`.scx` containers. It extracts proved header metadata, navigates the
version-gated scenario/options section, decodes raw player settings and the
terrain/elevation map, object tables, and supported trigger systems, and
verifies the complete raw-DEFLATE body. A bounded converter translates proved
terrain/object IDs and a lossless subset of classic triggers. It does **not**
claim complete AI, victory-rule, civilization, or commercial-ID coverage.

This is deliberate. A syntactically decoded commercial file is not playable
until every referenced terrain, object, civilization, technology, trigger, and
rule ID has an evidenced semantic mapping.

## Commit-pinned evidence

Primary implementation evidence is Siege Engineers' MIT-licensed `genie-rs` at
commit `a77200aef567b40b7db51cf47c1fda8db75e8e67`:

- [`SCXFormat::load_inner`](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-scx/src/format.rs)
  reads the uncompressed header, wraps the remainder in a raw DEFLATE decoder,
  then reads next-object ID, scenario data, map, players, objects, triggers,
  and AI in version-dependent order.
- [`SCXHeader::read_from`](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-scx/src/header.rs)
  proves the sized header, header versions, optional timestamp, length-prefixed
  description, single-player-victory flag, active-player count, and newer
  version branches.
- [`SCXVersion`](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-scx/src/types.rs)
  enumerates recognized classic format versions and their player-data
  compatibility.
- [`ScenarioObject`](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-scx/src/format.rs)
  demonstrates why body conversion is not a header-only task: even one object
  has version-dependent fields and a commercial unit-type ID.
- [`triggers.rs`](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-scx/src/triggers.rs)
  pins trigger record order and physical property indices. Condition indices
  used here are amount 0, resource 1, player 5, timer 7, area 9–12, group 13,
  object type 14, and inverted 16. Effect indices are diplomacy 3, unit type
  6, source player 7, target player 8, timer 12, location 14–15, group 20, and
  object type 21.

Trigger type meanings are independently pinned to `AoE2ScenarioParser` commit
`8e3abd422164961aa5c7857350475088790804f8`:
[`conditions.py`](https://github.com/KSneijders/AoE2ScenarioParser/blob/8e3abd422164961aa5c7857350475088790804f8/AoE2ScenarioParser/datasets/conditions.py)
and
[`effects.py`](https://github.com/KSneijders/AoE2ScenarioParser/blob/8e3abd422164961aa5c7857350475088790804f8/AoE2ScenarioParser/datasets/effects.py).

The implementation was also exercised against two fixtures stored in that
exact commit: AoC-style `Age of Heroes b1-3-5.scx` (`1.21`, header 2, eight
active players) and AoE-style `Dawn of a New Age.scn` (`1.07`, header 1, one
active player). These files are test inputs from the cited parser repository,
not bundled reconstruction assets.

## Implemented contract

`inspect_legacy_scenario(path)` returns a structured result:

| Status | Meaning |
| --- | --- |
| `metadata_only` | Recognized classic version; strict header parsed; entire raw-DEFLATE body verified |
| `unsupported_version` | Container/version recognized as outside the bounded classic header contract |
| `malformed` | Truncated, inconsistent, over-limit, noncanonical, or corrupt data |
| `io_error` | File could not be opened or read completely |

On success, metadata contains:

- four-byte format version;
- header version 1 or 2;
- optional creation timestamp;
- optional/empty description;
- single-player-victory flag;
- active-player count;
- compressed and uncompressed body sizes;
- first body field, `next_object_id`;
- inner scenario-data version and exact end offset of the options section;
- 16 raw player-setting records: active/type/civilization/posture, starting
  resources, starting-age ID, and the 16-way diplomacy matrix;
- map width/height and row-major terrain ID, elevation, and zone per tile;
- raw placed objects with owner slot, position, ID, unit-type ID, state, angle,
  animation frame, and garrison relationship;
- trigger-system version and raw trigger metadata, condition/effect property
  vectors, text/audio fields, referenced object IDs, and display orders;
- explicit player, map, object, and trigger completeness flags.

Validation is mutation-free. No partial `Scenario` is returned. Raw commercial
IDs remain metadata where no authoritative mapping exists.

## Bounds and rejection rules

- Input file: at most 128 MiB.
- Header: 16 bytes through 1 MiB.
- Description: at most 256 KiB; a nonempty value must end in one NUL and may
  not contain an earlier NUL.
- Active players: at most 16.
- Inflated body: at most 256 MiB.
- Raw DEFLATE must reach end-of-stream and consume every compressed byte.
- Inflated body must contain the four-byte next-object ID.
- Inner data versions 1.00 through 1.22 use exact field gates from the pinned
  parser; outer and inner versions are independently decoded.
- Classic maps must be 1–500 tiles in each dimension and contain exactly three
  bytes per tile: terrain, signed elevation, and signed zone.
- Diplomacy values must be the proved ally/neutral/enemy IDs 0, 1, or 3.
- Body player tables are limited to 16 slots and 100,000 total objects.
- Trigger systems accept proved versions 1.0–3.0, at most 10,000 triggers,
  10,000 conditions/effects per trigger, 256 properties per entry, and 100,000
  referenced objects per effect.
- Trigger strings are at most 256 KiB, terminated, and contain no embedded NUL.
- Supported classic format strings:
  `1.07`, `1.09`–`1.16`, `1.18`–`1.21`.
- Only classic header versions 1 and 2 are decoded. Later HD/DE header branches
  return `unsupported_version`, not guessed metadata.

These are reconstruction limits. They are not claims about Microsoft's parser.

## Deferred full conversion

Full conversion needs a separate, evidenced mapping layer for:

1. terrain IDs, elevation storage, and passability semantics;
2. player IDs for ages, civilizations, types, posture, and teams;
3. object IDs to supported reconstruction unit/building kinds;
4. coordinates, ownership, garrison relationships, state, and facing;
5. victory structures and trigger conditions/effects;
6. embedded AI and version-specific trailing sections;
7. explicit policy for unsupported IDs and semantics.

Conversion must fail atomically when required gameplay semantics lack a
mapping. Dropping unknown objects, terrain, triggers, or rules would silently
change scenario meaning and is forbidden.

## Bounded conversion

`convert_legacy_scenario(metadata, dat)` requires parsed map, player, and object
sections plus a live `VER 5.7` DAT catalog. Its canonical mapping table is
restricted to IDs already independently recorded by the repository's generated
live-DAT metadata and renderer/DAT evidence maps.

| Commercial ID | Reconstruction value |
| ---: | --- |
| Terrain 0 / 1 / 2 / 4 | Grass / Water / Beach / Shallows |
| Object catalog | All 96 represented `UnitKind` values |
| Building catalog | 23 of 27 represented `BuildingKind` values |

`TERRAIN_FIDELITY.md`, `STANDARD_UNITS_ASSET_MAP.md`,
`LAND_UNIQUE_ASSET_MAP.md`, `NAVAL_ASSET_MAP.md`, `BUILDING_SPRITE_MAP.md`,
the generated metadata JSON, and focused subsystem asset maps record these IDs
and represented roles. The converter also verifies that a terrain ID is within
the supplied DAT terrain count before mapping it. Palisade and stone gate
orientations are excluded: `BUILDING_SPRITE_MAP.md` explicitly leaves their
post-terrain commercial object links unresolved.

Player slots 1 and 2 become blue and red. Their four represented resources and
blue-to-red diplomacy are copied. Objects translate only when the owner is
blue/red, coordinates are integral and inside the map, and the commercial ID
is in the table above.

The conversion report counts every translated and unsupported tile/object,
stores every unsupported tile index, retains every complete unsupported raw
object record, exposes `unsupported_commercial_object_ids` frequency counts,
and provides diagnostics. Any unsupported terrain suppresses
the playable `Scenario`; no guessed terrain is substituted. Unsupported
objects do not suppress an otherwise playable bounded scenario, but remain
available to callers and are never silently discarded.

Commercial object records do not contain a hit-point field in the pinned
schema. `ScenarioObject::read_from` reads exactly position, object ID, unit
type, state, angle, version-gated animation frame, and version-gated garrison
ID before the next record. No current HP, maximum HP, percentage, or dead-state
value exists to validate or convert. Imported placements therefore use
reconstruction defaults. Report fields `object_hit_points_available=false`
and `objects_using_default_hit_points` make that limitation machine-readable.
Nonzero `z`,
state, angle, animation-frame, and garrison fields are retained in raw metadata
but not represented by `UnitPlacement`/`BuildingPlacement`; each affected
object and field name appears in `lossy_objects`.

Trigger audits are available independently of conversion. Deterministic JSON
records condition/effect type counts, unsupported type IDs, absent physical
schema indices, object-reference blocker reasons,
direct/list/group/type/area selector-shape histograms, and mapped/unmapped
commercial object-ID counts. This lets an external fixture measure the next
conversion target without requiring a playable map or live DAT.

Commercial object IDs are never copied into native trigger strings. Conversion
classifies all placements first, then predicts the IDs `create_simulation`
allocates: units in final unit-vector order starting at 1, followed by
buildings in final building-vector order. The report exposes each source
object, predicted native ID, category, lossless flag, and blocker. Only unique
positive IDs for losslessly translated placements enter the trigger-reference
map. Duplicate, missing, unsupported, or lossy references preserve their raw
trigger entries and suppress playable output.

### Bounded trigger conversion

Global trigger order is decoded instead of skipped. Converted triggers retain
that order through descending reconstruction priority, retain stable source
index IDs, and copy enabled/disabled and looping flags.

Lossless native mappings are deliberately limited:

| Classic semantic | Reconstruction syntax |
| --- | --- |
| Timer condition (type 10) | `elapsed_ticks >= seconds * 5` |
| Accumulate resource (type 8) | `resource PLAYER RESOURCE >= AMOUNT` |
| Unfiltered objects in area (type 5) | `area_presence PLAYER X1 Y1 X2 Y2 >= AMOUNT` |
| Destroy initial object (type 6) | remapped `unit_destroyed` / `building_destroyed` |
| Send chat (effect 3) | strict quoted `message` |
| Create common mapped object (effect 11) | `create_unit` / `create_building` |
| Blue/red diplomacy (effect 1) | `diplomacy` |
| Tribute (effect 5) | `tribute SOURCE TARGET RESOURCE AMOUNT` |
| Canonical represented research (effect 2) | `research PLAYER TECHNOLOGY` |
| Activate/deactivate trigger (effects 8/9) | native trigger-state effect |
| Remove one initial object (effect 15) | remapped `remove_object ENTITY` |
| Declare victory (effect 13) | `victory PLAYER` |

Scenario64 models ordered condition/effect vectors. Conditions use AND
semantics and effects execute in stored order against one atomic preflighted
snapshot. Classic tribute and activate/deactivate-trigger effects therefore
translate alongside the table above. Activate/deactivate targets must be
within the decoded commercial trigger table. Remove accepts only one unique
direct selection, requires its decoded list count to match, and rejects
group/type/area selectors. Multi-condition and multi-effect order
permutations preserve source order.

Object HP remains unsupported even for a remapped object: pinned `genie-rs`
does not name physical condition property 17, while the pinned condition type
dataset names a comparison field without proving its physical index or enum.
Guessing native `>=` would not be lossless. Bring-object-to-area also remains
unsupported because native area presence counts a player population rather
than preserving one object's identity. Object-visible/not-visible is not
treated as entity existence without evidence that commercial visibility has
that meaning. Inversion, filtered area selection, unresolved commercial
object-ID removal, forced-research flags, objectives, defeat, and other
classic types remain unsupported. Every raw trigger, condition, and effect
remains in the conversion report with source indices and a reason. One
unsupported trigger suppresses playable output; supported siblings may still
be counted for audit, but no partial scenario is returned.

The pinned AoC fixture provides pivot frequency evidence. Its unsupported
effect types are led by change-object-HP type 27 (1,787), change-object-attack
type 28 (1,051), range type 32 (437), and speed type 33 (389). None has a
native trigger-effect representation, and classic physical properties do not
prove every later operation field. The highest-frequency exact native semantic
in that unsupported set is research type 2 (52). Conversion now accepts its
strict classic shape only: mapped blue/red source, one of 18 independently
catalogued technology IDs observed in the fixture, all unrelated physical
properties `-1`, and no object list/text/audio payload. Noncanonical force or
extra fields, unknown technologies, and other players preserve the raw effect
and suppress playable output atomically. Trigger audit JSON includes
`research_technology_ids` counts.

Type 27 cannot be promoted from that frequency alone. Pinned `genie-rs`
classic effects decode 23 physical properties and pad property 23; they name
property 22 only as line ID. Pinned `AoE2ScenarioParser` identifies semantic
fields `quantity`, `operation`, and target selectors, but its longer physical
structure places `operation` after fields absent from the classic record. It
does not define operation enum values, set-versus-add behavior, maximum-HP
clamping, zero/death handling, or whether source player further restricts an
explicit object list. Therefore no classic physical value can be identified
as operation without guessing. Conversion reports type 27 with this specific
blocker, preserves complete raw effect data, and returns no partial scenario.

Type 30 attack-move was checked next because 204 records occur in the pinned
fixture and native runtime has attack-move orders. Every fixture record uses
classic rectangle properties rather than an object list. Pinned sources name
both destination and selector-area concepts but do not prove how the classic
rectangle pair packs those concepts. Destination-versus-selector inference is
therefore also refused explicitly.

Remaining higher-frequency candidates have no exact native gameplay target:
play-sound type 4 (175) is presentation state, damage-object type 24 (109)
needs the same selector and zero/death proof missing for HP mutation,
change-name type 26 (71) has no native entity-name state, and task-object type
12 (60) does not prove one native order kind from its generic task payload.
Canonical research type 2 remains the highest-frequency fixture slice with
both proved physical fields and matching deterministic native semantics.

### Unsupported object-catalog ranking

Live audit of pinned `Age of Heroes b1-3-5.scx` ranks unmapped placed
objects as Oak Forest Tree 349 (11,528), Pine Forest Tree 350 (10,905), Map
Revealer 837 (1,167), and Hay Stack 857 (228), before lower-frequency entries.
Conversion-report JSON now exposes the same per-ID count for every mapping
miss encountered during bounded conversion.

None passes the exact vertical-slice gate. Tree records use half-tile centers,
nonzero z/state, and 14 angle/animation-frame variants. Native forest terrain
has no placed-object identity or frame field; Pine would also render through
the oak/forest asset binding. Map Revealer has evidenced HP and LOS but lacks a
proved native graphic/icon/selection contract and has no production, combat,
upgrade, or civilization availability path. Hay Stack is decorative. The
highest-frequency unmapped combatant is King 434 (42), but current generated
evidence does not bind its DAT record to exact standing/walking/dying graphics
or a native icon/production contract. These remain raw unsupported records.
Canonical research conversion above remains the highest-frequency catalog
slice satisfying every available end-to-end evidence gate.

## Test contract

Fixture-driven tests cover:

- header version 2, timestamp, description, flags, player count, and object ID;
- exact 1.21 scenario/options navigation, 16 player records, diplomacy, and a
  two-tile terrain/elevation map;
- a placed object with version-gated frame/garrison fields;
- a trigger with raw metadata, one effect, one condition, ordering, strings,
  property vectors, and object references;
- global trigger-order decoding;
- timer/resource condition and message/create-object effect conversion;
- multiple ordered effects plus tribute conversion;
- multiple ordered object conditions/effects, activate/deactivate target
  bounds, and direct/list remove-selector canonicalization;
- explicit HP-comparison, identity-aware area, filtered-area, and
  visibility/nonexistence blockers;
- exact preservation of priority, disabled, and looping state;
- unsupported tribute preservation and atomic playable-output suppression;
- conversion of proved Grass/Water and Villager IDs against a VER 5.7 DAT
  catalog;
- canonical research-effect conversion plus noncanonical-field atomic refusal;
- machine-readable missing object-HP/default-placement counts;
- preservation and exact counts for an unsupported object;
- suppression of playable output plus exact tile index for unsupported
  terrain;
- exhaustive iteration of the canonical catalog: unique commercial IDs,
  exactly one unit/building target per row, and successful conversion of every
  row;
- explicit lossy-field reporting for state, angle, animation frame, and
  garrison relationship;
- unknown format version diagnostic;
- truncated DEFLATE rejection;
- real pinned AoC `.scx` and AoE `.scn` inspection;
- real-fixture object/trigger traversal (24,618 objects and 780 triggers in the
  pinned AoC fixture; 241 objects and no trigger section in the AoE fixture);
- compilation with warnings enabled.

Future body decoders require section fixtures from a commit-pinned,
redistributable source plus truncation, count overflow, unknown-ID, and
round-trip semantic tests for every newly supported section.

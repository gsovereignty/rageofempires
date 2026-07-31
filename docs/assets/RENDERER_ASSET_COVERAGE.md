# Renderer asset coverage

## Shared selection architecture

`include/aoe/render_asset_coverage.hpp` defines the canonical render-state
dimensions, coverage statuses, requested assets, and resolution results.
`src/render_asset_coverage.cpp` owns the land-unit, special-action, naval,
completed-building, construction, and destruction catalogs.
`src/building_damage.cpp` owns the exact civilization-specific 25/50/75%
damage-overlay roots. `src/projectile_catalog.cpp` owns exact projectile,
impact, linked-shadow, and directional-transform evidence. Runtime sprite
loading, runtime selection, and
`renderer_asset_coverage_audit` consume those same typed catalogs. The audit
does not parse C++ source to infer mappings.

Resolution has two phases:

1. `resolve_unit_asset`, `resolve_building_asset`, or
   `resolve_projectile_asset` selects the exact direct SLP or DAT graphic root
   for a typed state.
2. The loader or audit follows that request through `empires2_x1_p1.dat`,
   `graphics.drs`, the palette resource in `interfac.drs`, frame layout, every
   player palette, and every composite part.

The legacy v1 report and Python source parser remain historical evidence.
`generated/renderer_asset_coverage_v2.json` is the authoritative state-level
baseline.

## Status meanings

- `renderable`: selection, archive lookup, layout, and decoding succeeded.
- `intentional_procedural`: repository evidence explicitly reviews the
  procedural body; this is not inferred from a missing asset.
- `missing_mapping`: a reachable state has no canonical selection.
- `missing_archive_resource`: a directly selected SLP is absent.
- `invalid_dat_reference`: a selected DAT root or delta graph is invalid.
- `decode_failure`: an existing SLP cannot be decoded.
- `missing_frame`: selected frame layout cannot satisfy runtime indexing.
- `missing_player_variant`: selected owner palette is unavailable.
- `missing_composite_part`: a DAT child references an absent required SLP.
- `missing_shadow`: an independently selected shadow dependency is absent.
- `invalid_runtime_selection`: runtime supplied incompatible state dimensions.
- `unsupported_owner_state`: owner is outside supported roster semantics.
- `renderer_failure`: resolution succeeded, but the SDL rendering call failed.

## Runtime fallback report

Set `AOE_RENDER_FALLBACK_REPORT` to a report path. Normal play performs no
telemetry work when the variable is absent.

```sh
AOE_RENDER_FALLBACK_REPORT=/tmp/render-fallbacks.json \
AOE_ASSET_ROOT=/path/to/app \
./build/aoe_reconstruction
```

Each event contains entity ID, object kind, full state, owner, civilization,
Age, direction, requested graphic/SLP/composite/shadow, status, exact reason,
simulation tick, and renderer call site. Events deduplicate by stable
render-state key and serialize in sorted order.

## Audit and project gates

The checked-in packaged Data directory is the default audit input:

```sh
cmake -S . -B build
```

`AOE_ASSET_AUDIT_DATA_ROOT` remains available only for explicit research
comparison against another extracted installation.

Commands:

```sh
# Refresh checked state-level baseline from DAT/DRS evidence.
cmake --build build --target refresh_renderer_asset_coverage

# Regenerate independently and reject new or changed unresolved states.
cmake --build build --target check_renderer_asset_coverage

# Fast resolver, telemetry, and regression-fixture tests.
ctest --test-dir build -R render_asset_coverage_tests --output-on-failure
python3 tools/test_compare_renderer_asset_coverage.py

# Render 24 deterministic scenario/tick cases, merge telemetry by stable key,
# and require exact unit, building, projectile, impact, resource, death, and
# rubble status/asset/reason agreement with static baseline.
cmake --build build --target run_renderer_runtime_coverage
```

The comparison accepts removal of reviewed gaps. It fails when a new state
becomes unresolved or an existing gap changes status/reason. Absolute asset
paths never enter reports.

## Adding a renderable

1. Add the kind/state to the appropriate typed canonical catalog.
2. Make the runtime loader consume that catalog; do not add a second mapping.
3. Ensure runtime state derivation selects the same dimensions.
4. Refresh the live audit and inspect every new unresolved status.
5. Add focused resolver and transition tests.
6. Review a procedural exception only with repository evidence explaining why
   no archive-backed body is expected.

Never substitute unrelated legacy art, swallow asset errors, or call a kind
covered because one action succeeds.

## Current evidence limitations

- Sheep attack is reachable through production `Simulation::command_unit`,
  resolves to SLP `3623`, and that resource is absent from every supplied
  `graphics.drs`. Runtime loading preserves exact
  `missing_archive_resource` evidence. No supported replacement ID is known.
- Town Center delta graphs reference absent SLPs `890`, `896`, `897`, `899`,
  `908`, `909`, `910`, and `911`. Completed rendering remains available
  because every selected root SLP is present and complete.
- Barracks composites reference absent main-body SLPs `122`–`125` and
  `134`–`137`.
- Mill composites reference absent required SLPs `731`, `2516`, and `3481`.
- Dedicated naval death animations now use the shared catalog. Trade Cog,
  demolition ships, cannon galleons, longboats, and turtle ships no longer
  appear as false `missing_mapping` results.
- Projectile bodies, impacts, and linked shadows now use the same exact DAT
  catalog in runtime and audit. Arrow direction mapping remains explicitly
  intentional procedural because its short-SLP transform is unproved.
- Forest, berry, gold, stone, and fish resources share one depletion-frame
  catalog between loading, rendering, telemetry, and audit. Gold no longer
  hides a failed depletion frame by silently substituting frame zero.
- Fish Trap completed/construction animation is exact. Live DAT unit 199 has
  no dying graphic, while damage roots `5357..5359` have no drawable layer;
  damage and rubble therefore have explicit evidence-backed procedural
  classifications.
- Exact DAT-backed death animations now cover Villager, Archer, Sheep, Deer,
  Boar, Trade Cart, Fishing Ship, Galley, War Galley, Galleon, Transport Ship,
  Fire Ship, and Fast Fire Ship in addition to earlier death mappings.
- No `missing_mapping`, `invalid_dat_reference`, `decode_failure`,
  `missing_frame`, `missing_player_variant`, or `missing_shadow` rows remain.
  Remaining unresolved rows are physical archive absence, not unexplained
  selection.
- The workspace snapshot has no `.git`; required focused commits cannot be
  created here.

The current live baseline contains 448,756 state rows: 333,786 renderable,
82,818 intentional procedural, and 32,152 reviewed unresolved
(`960` missing archive resources plus `31,192` missing composite parts). Unit rows
now
include shared special-action/detail and moving variants. Projectile rows cover
logical direction, frame, linked shadow, and impact animations. Building rows
expand generic plus every playable civilization, scenario-placeable Ages,
foundation, all four construction stages, completed, all four damage stages
with overlay roots, dying, and destroyed. State values use stable names rather
than enum ordinals.

Completed-building selection now carries an explicit composition policy.
`complete_root` draws only selected root SLP; `delta_graph` is reserved for a
root such as Dock graphic `215`, whose DAT record has no SLP. Earlier Town
Center loading drew a current-Age base, complete root, and available DAT
children together. That root-plus-delta path duplicated one building graph
and could resemble a larger shell around current body. Runtime now loads exact
current-Age/civilization root SLP and draws it once.

Visual selection uses explicit replacement/minimum-Age rules and never scans
forward until any family has art. Missing selected family returns
`missing_mapping`. Mesoamerican House, Town Center, Barracks, Mill, Archery
Range, Stable, Castle, Siege Workshop, and Watch Tower use proved fifth-family
roots. Scenario-placed Stone Gates explicitly clamp to Castle visual Age;
Watch/Guard/Keep remain technology variant slots independent of Age index.

`building_age_graphics_sdl_smoke` captures Dark, Feudal, Castle, and Imperial
tracked fixtures under SDL software rendering. Color-component, occupancy,
extent, ROI, and outside-ROI checks reject oversized or nested fallback
silhouettes. Exact root IDs and composition policies are asserted separately
in `render_asset_coverage_tests`, so hermetic CI needs no proprietary graphics.

The expanded runtime suite exposed and now guards two former disagreements:
absent root SLP `2263` is allowed only when a naval DAT root has drawable
deltas, and construction HP never masquerades as a completed-building damage
stage. Tick-zero placed units also remain idle rather than falsely selecting
movement animation. Save version 111 persists source entity identity for
projectiles, impacts, unit deaths, and building rubble plus death facing, so
every fallback event carries its actual source/entity ID.

## Legacy v1 audit (deprecated)

Material below documents the earlier source-parsing report. Keep it only for
historical comparison. It is not an authoritative gate and must not be used to
approve new mappings or procedural exceptions.

`generated/renderer_asset_coverage.json` is a deterministic inventory of every
represented `UnitKind` and `BuildingKind` against:

- mappings in `load_local_legacy_sprites`;
- graphic-to-SLP links from the live DAT extractor output;
- SLP resource IDs physically present in the live `graphics.drs`.

The audited asset set contains 96 unit kinds, 27 building kinds, 7,014 DAT
graphic records, and 1,768 SLP resources. All 96 unit kinds and 26 building
kinds have reachable sprite-load mapping evidence.

Farm is the only building kind with no sprite assignment in
`load_local_legacy_sprites` and therefore always uses procedural rendering.
Palisade Gate X/Y have reachable root mappings, while their absent N1
components 4877/4888 retain the documented component-level procedural path.
Stone Wall now uses its five family-specific SLPs with validated straight-axis
frames and a junction/isolated fallback frame. Stone Gate X/Y use distinct
roots; Monastery and Market use their live age/family mappings. Palisade Wall
uses exact shadow, wall, and animated player-flag SLPs at full health; damaged
states retain procedural rendering.

Mapping detection is deliberately narrower than enum-reference detection. It
uses only calls, mapping records, and loop lists inside the balanced body of
`load_local_legacy_sprites`. Enum references in later render switches do not
prove that a sprite was loaded.

## Prioritized direct mapping gaps

| Priority | Group | Kind | Missing SLP IDs | Exact mapped use |
| --- | --- | --- | --- | --- |
| P1 | common | `sheep` | `3623` | attack animation |
| P2 | building | `town_center` | `890`, `896`, `897`, `899`, `908`, `909`, `910`, `911` | one layer of selected age/culture composites |

Unique and naval units have no absent directly mapped SLPs. All other common
units and sprite-mapped buildings also have no absent directly mapped SLPs.
Woad Raider and Elite Woad Raider now resolve their complete idle, movement,
attack, and death families and report `mapped`.

`cavalry_archer` and `heavy_cavalry_archer` now have explicit reachable
animation mapping records. A regression fixture removes those records while
leaving render-time enum mentions intact; both kinds then correctly report
whole-kind fallback.

`mapped_asset_gap` means one or more directly named action or layer SLPs are
absent. It does not mean the whole kind always falls back. For example, sheep
standing and movement assets remain available. Town Center rendering uses its
coherent age/family base SLP without also drawing displaced component SLPs;
those components otherwise add unrelated poles and fragments around the
building. Missing layers do not authorize a future-Age or procedural overlay.

The report's DAT-wide absent-SLP list is broader evidence about unresolved DAT
links. It is not a list of represented renderer failures. Per-kind findings are
under `groups`, while the renderer-only aggregate is
`live_evidence.renderer_direct_slps_absent_from_graphics_drs`.

## Reproduce

From the repository root, first emit the full DAT metadata JSON:

```sh
tools/dat_metadata/target/release/aoe-dat-metadata \
  /path/to/empires2_x1_p1.dat > /tmp/aoe-full-dat.json
```

Then run the audit:

```sh
python3 tools/audit_renderer_asset_coverage.py \
  --dat-json /tmp/aoe-full-dat.json \
  --graphics-drs /path/to/graphics.drs \
  --output generated/renderer_asset_coverage.json
```

When DAT/DRS inputs are unchanged but unavailable on the current machine,
refresh live source mappings while retaining their audited evidence:

```sh
python3 tools/audit_renderer_asset_coverage.py \
  --baseline-report generated/renderer_asset_coverage.json \
  --output generated/renderer_asset_coverage.json
```

Run the synthetic parser/report tests with:

```sh
python3 tools/test_audit_renderer_asset_coverage.py
```

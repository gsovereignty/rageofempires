# Deterministic random maps

This module parses bounded classic random-map scripts and generates
two-player scenarios through recovered RMS semantics. Caller-owned include
payloads are explicit; runtime performs no filesystem or parent fallback.

## Historical shape used

The original *Age of Empires II* manual describes Arabia as an arid desert,
Black Forest as grass islands in dense trees, and its map setup exposes map
type and size choices. The original standard map list also includes Islands
and Rivers. These descriptions define broad identities, not byte-exact
generation rules:

- Original manual scan:
  <https://manuals.plus/m/bede02f48ca7b2aae252168379555520636b871dcd6c6fdba8ba2766ea0186e2.pdf>
- Current official map notes confirm these names remain distinct random-map
  families:
  <https://www.ageofempires.com/news/aoe2de-update-34699/>

## Exact built-in script evidence

The supplied expansion `gamedata_x1.drs` contains source text for every map
family exposed by the reconstruction frontend. `generated/builtin_random_map_evidence.json`
pins archive and payload hashes without redistributing those scripts:

| Map | DRS resource | Bytes | SHA-256 |
|---|---:|---:|---|
| Arabia | 54201 | 4,989 | `ece1c86f38772a8aab1ca4a65630ee843da6c994458cbfa663b1e1bab34ee31a` |
| Black Forest | 54204 | 7,676 | `d6a177b6905bea8a1323da4d1b38bd6cf73dd3946d8974668565dcff0a162cc0` |
| Islands | 54211 | 23,331 | `13ae8c84dd16502b5ed778e3b6fa3d49e1374b1d9154e4f2c5a9775d521a8f84` |
| Rivers | 54217 | 9,714 | `36f438566e08f5131e845177050d38ab2bd45639f099f2e47a3cf5d74fdd5fd2` |

All four include resource 54000 (`random_map.def`); Arabia and Black Forest
also include land-resource resource 54103, while Rivers includes land-and-water
resource 54102. Arabia contains a cliff-generation section. These facts rule
out treating the short strings embedded in `generate_rms_map` as recovered
original definitions.

Refresh metadata from legally obtained evidence with:

```sh
python3 tools/audit_builtin_random_maps.py \
  /path/to/Data/gamedata_x1.drs \
  generated/builtin_random_map_evidence.json
```

`FUN_00622010` proves frontend size selection and dispatch into the map engine,
but does not expose that engine's placement implementation. It invokes map
creation through an indirect method after selecting the RMS and size. Exact
original scripts alone therefore cannot prove output parity. Decompiled
evidence at `FUN_005278d0` proves MSVCRT seeding before engine construction,
one ordered RNG stream through engine modules, and a final `_rand()` after
generation. Evaluator now uses recovered
`state = state * 214013 + 2531011`, `(state >> 16) & 0x7fff` behavior.

`generated/builtin_random_map_parity.txt` pins all four exposed families at
two seeds and Tiny/Normal/Giant sizes. `aoe_rms_parity_probe` checks these
family/seed/size hashes in CTest. Repository stores semantic definitions,
hashes, counts, and fingerprints only—not proprietary RMS text. User-owned
DRS resources can be audited and supplied through caller-owned include maps.

Recovered language support covers includes, constants, defines, nested
conditions, weighted random blocks, land IDs, specific-land placement, zones,
scaling, flat-terrain filters, terrain replacements, connections, elevation,
cliffs, and classic object aliases used by pinned built-in resources.

## Observed map-size ladder

Observed executable evidence:

- `AoK-HD-patched.strings.txt:29297-29302` lists exactly six map-size keys,
  in descending order: `GIANT-MAP`, `LARGE-MAP`, `NORMAL-MAP`, `MEDIUM-MAP`,
  `SMALL-MAP`, `TINY-MAP`. The lowercase selector tokens `giant`, `large`,
  `normal`, `medium`, `small`, `tiny` appear at
  `AoK-HD-patched.strings.txt:29627-29632`.
- `FUN_00622010` converts the selected size index to a tile dimension with a
  dense switch at `AoK-HD-patched.c:356938-356960`:

  | Index | Constant | Tiles |
  |---:|---|---:|
  | 0 | `0x78` | 120 |
  | 1 | `0x90` | 144 |
  | 2 | `0xa8` | 168 |
  | 3 | `200` | 200 |
  | 4 | `0xdc` | 220 |
  | 5 | `0xf0` | 240 |
  | 6 | `0xff` | 255 |

Interpretation: indices 0-5 are the six named presets in ladder order; index 6
has no name string and is treated as an engine-internal maximum. Confidence:
high for indices 0-5, since the count and order match the string table
exactly.

Not ported: the same function bumps the index by one for a specific set of map
type IDs (`iVar9` equal to 10, 0x10, 0x13, 0x15, or 0x17) before the switch, so
those original map types generate one step larger than requested. The
reconstruction's `RandomMapKind` set does not correspond to those IDs, so no
bump is applied.

Modern choice: `RandomMapSize` exposes all seven switch indices and
`random_map_dimension` returns the observed tile counts. Index 6 is named
`maximum` and labelled `MAXIMUM` in the frontend; that name is invented,
because no original name string or menu entry was recovered for it.

Not recovered: the original default selection. The map-size combo is populated
and then reset with `FUN_005bfaf0(0)` (`AoK-HD-patched.c:364097`), i.e. list
item 0, but the populate order for that list was not established, so index 0
cannot be tied to a named preset with confidence. The original also rewrites
the selection from player count (`AoK-HD-patched.c:366567-366582`), which the
reconstruction does not model.

Modern choice: `RandomMapSettings::size` defaults to the recovered index-6
maximum (255 tiles), and the frontend starts on the same preset. No original
default was recovered; selecting the maximum makes the reconstruction's
startup paths use one observable map extent. RMS scripts with no
`override_map_size` directive inherit that default, matching the original
behaviour where a script without the directive uses the lobby-selected size.
An `override_map_size` above 240 snaps to `maximum`.

Modern choice: bundled campaign scenarios are generated from their compact
source layouts at 255x255 by
`tools/generate_campaign_scenarios.py`. Terrain rectangles and elevation tiles
scale as areas, placements scale as points, and generation rejects invalid
footprints. Both bundled campaign missions (`foundations.scenario` and
`elevation-demo.scenario`, referenced by `briefing-demo.campaign`) are
generated, because campaign paths load through the playable-map guard.
No original bundled scenario exists. Normal startup instead creates a
deterministic random map. CMake tracks and deploys runtime resources so source
and app copies do not diverge, and `campaign_scenario_generator_tests` fails
if a checked-in campaign file stops
matching the generator.

Modern choice: the deliberately small renderer audit fixtures under
`resources/` (18x10 and similar single-purpose sprite stages) keep their
original dimensions and are loaded only by
`tools/run_renderer_runtime_coverage.py`, which sets the
`AOE_AUDIT_ANY_MAP_SIZE` diagnostic to bypass the 255x255 guard. Their
fallback telemetry is position independent, so regenerating them at 255x255
would churn the pixel-audit suite without changing what it proves. With the
diagnostic unset, no launch path accepts a smaller map.

Modern choice: the random-map preview in the frontend is square and samples
one filled rect per preview pixel. It was a 128x96 rect filled once per
tile, which stretched every square generated map by 4:3 and issued 65,025
draw calls per frame at the maximum preset.

Modern choice: generated-map feature radii scale by
`sqrt(dimension / 96)`, and corridor widths (the Black Forest route, the
Rivers channel and its fords) by `dimension / 96`. Blob counts already grew
with the dimension, but the radii were fixed, so coverage grew linearly
while area grew quadratically. Measured non-grass coverage at seed 7 before
the change: Arabia 6.76% at 120 tiles, 3.76% at 200, 2.80% at 255; raised
ground 8.78%, 2.96%, 1.65%. After: 6.76%, 7.78%, 7.69% and 13.42%, 9.90%,
10.20%. 96 is the largest dimension the fixed radii were authored against
and the scale never shrinks an extent, so nothing at or below it changes.
No original scaling rule was recovered.

Modern choice: renderer world extents, isometric origin, camera clamps, and
minimap aggregation derive from the loaded map. The fixed 1280x640 world
viewport remains presentation geometry rather than a map-size limit. A
headless `AOE_MAP_DIMENSION_PATH` diagnostic records the map presented by a
real app launch and, when open, the generated frontend preview. SDL smoke
tests assert both contain 65,025 tiles; no original diagnostic was recovered.
Two further diagnostics support headless capture, neither with an original
equivalent: `AOE_CAMERA_TILE` aims the initial camera at a named tile, since
the start view now covers a small fraction of the world, and `AOE_FOG`
overrides the existing fog display option, since one start's explored area
no longer covers the map. The minimap now honours that option, which the
world pass already did.

Observed cost at 255x255 (65,025 tiles against 384): the app holds its
fixed simulation cadence at both sizes. Wall clock to tick 100 was 23.61s
at 255x255 against 23.07s at 24x16 with the GPU renderer, and 23.98s
against 22.40s with the software renderer. The full test suite runs in
21.95s against 12.36s before the change.

Modern choice: playable app boundaries accept only 255x255 scenarios and
saves. This covers bundled launch, `AOE_SCENARIO_PATH`, campaign selection,
editor input/load, save restore, replay reset, random-map start, and SDL
multiplayer (which shares the active simulation). The frontend size control
therefore exposes only `MAXIMUM`; smaller recovered presets remain in the
generation/import APIs for fidelity and compatibility tests. The standalone
multiplayer roster harness also boots a 255x255 map. No original policy
requiring only the maximum was recovered.

## Reconstruction contract

- `RandomMapKind`: Arabia, Black Forest, Islands, Rivers.
- `RandomMapSize`: the original six-step named ladder plus the unnamed
  index-6 maximum, square tiles.
- Optional blue/red civilization selections flow into generated scenarios;
  defaults remain generic.
- Recovered 32-bit MSVCRT `srand`/`rand` stream; no host-platform RNG.
- Blue/red starts use 180-degree paired placement.
- Each player receives one Town Center, three villagers, one scout, sheep,
  hunt, berries, gold, and stone.
- Arabia is open land with sparse forests; Black Forest has clearings joined
  by a narrow route; Islands has separate shore-ringed land masses and fish;
  Rivers has winding water, beaches, and shallow crossings.
- Distinct ground, water-depth, tree-family, and fish identities plus cliff
  topology survive Scenario68 and Save116 round trips. Replay resets use same
  scenario schema. Renderer distinguishes ground/water/tree/fish and visible
  cliff edges; pathfinding blocks cliff tiles.
- Validation checks start buildings/units, legal land, and expected land
  connectivity. Generation retries at most 16 derived seeds, then fails.
- `random_map_hash` covers dimensions, every terrain/elevation/resource tile,
  and all starting placements.
- `save_scenario` writes current Scenario66 output.

Property tests cover all four kinds and all seven size presets over 24 seeds
each, assert the exact observed tile counts, same-seed hash identity, paired
starting resources, validation, simulation construction, and Scenario66
serialization.

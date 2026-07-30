# Deterministic random maps

This module generates reconstruction-native two-player scenarios. It does not
parse or claim compatibility with Ensemble random-map scripts (`.rms`).

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

Modern choice: the bundled native scenario is generated from its original
24x16 demonstration layout at 255x255. Terrain rectangles scale as areas,
placements scale as points, and generation rejects invalid footprints. No
original bundled scenario exists. CMake tracks and deploys the generated
scenario independently of executable relinking so source and app copies do
not diverge.

Modern choice: renderer world extents, isometric origin, camera clamps, and
minimap aggregation derive from the loaded map. The fixed 1280x640 world
viewport remains presentation geometry rather than a map-size limit. A
headless `AOE_MAP_DIMENSION_PATH` diagnostic records the map presented by a
real app launch; no original diagnostic was recovered.

## Reconstruction contract

- `RandomMapKind`: Arabia, Black Forest, Islands, Rivers.
- `RandomMapSize`: the original six-step named ladder plus the unnamed
  index-6 maximum, square tiles.
- Optional blue/red civilization selections flow into generated scenarios;
  defaults remain generic.
- Fixed SplitMix64-derived PRNG; no platform RNG or floating random source.
- Blue/red starts use 180-degree paired placement.
- Each player receives one Town Center, three villagers, one scout, sheep,
  hunt, berries, gold, and stone.
- Arabia is open land with sparse forests; Black Forest has clearings joined
  by a narrow route; Islands has separate shore-ringed land masses and fish;
  Rivers has winding water, beaches, and shallow crossings.
- Elevation patches are deterministic and starting footprints are flattened.
  Blob count and radius are fixed rather than scaled by dimension, so larger
  presets are flatter; no original scaling rule was recovered.
- Validation checks start buildings/units, legal land, and expected land
  connectivity. Generation retries at most 16 derived seeds, then fails.
- `random_map_hash` covers dimensions, every terrain/elevation/resource tile,
  and all starting placements.
- `save_scenario` writes current Scenario66 output.

Property tests cover all four kinds and all seven size presets over 24 seeds
each, assert the exact observed tile counts, same-seed hash identity, paired
starting resources, validation, simulation construction, and Scenario66
serialization.

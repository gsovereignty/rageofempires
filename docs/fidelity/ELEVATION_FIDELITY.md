# Terrain elevation fidelity

## Evidence

- AoE II scenario map tiles carry a distinct byte-sized elevation field. The
  reconstruction already decoded that field from commercial scenarios in
  `LegacyScenarioMapTile`; conversion now preserves values `0..7`.
- The pinned `empires2_x1_p1.dat` terrain records map Grass, Water, Shallows,
  and Beach to terrain-archive SLPs `15001`, `15002`, `15014`, and `15017`.
  Each record declares 100 base animation frames followed by sixteen
  one-frame logical `elevation_sprites` at frame IDs `100..115`. Direct live
  archive inspection shows that all four mapped SLPs contain only 100 bitmap
  frames. Therefore IDs `100..115` are not exact extractable face assets in
  these archives and are not presented as such.
- The same DAT's 16-record legacy terrain-border table is disabled and points
  only to SLP `0`; it provides no cliff-face bitmap mapping. This is expected:
  elevation faces are generated from flat terrain pixels by `FilterMaps.dat`,
  `PatternMasks.dat`, `lightMaps.dat`, and `view_icm.dat`, not by a cliff SLP.
- Supplied 1999 and HD `FilterMaps.dat` files are byte-identical (SHA-256
  `9ec3645f42500ad197b2418c373c722019b1c9b888025bade3cdbc3581bc225a`).
  Packaged copies of all four elevation inputs are reconstruction-local.
- HD decompiled `FUN_004fd590` classifies each tile's exact 3x3 elevation
  neighborhood into slope IDs `0..16`. `FUN_004ffe80` applies that classifier
  whenever elevation changes. `FUN_00552160` loads 17 filter records, 40
  pattern masks, 18 light maps, and 10 inverse-color maps. `FUN_0054eb10`
  proves filter and lighting composition down to packed bit fields.
- Official Age of Empires terrain-tool documentation describes shifting maps
  through elevation levels and distinguishes slopes, cliffs, mountains, and
  hills:
  <https://support.ageofempires.com/hc/en-us/articles/8842669010452-Using-Automated-Terrain-Tools>
- Community formula documentation consistently records AoE II primary damage
  as `1.25` from higher ground and `0.75` from lower ground, independent of
  elevation difference:
  <https://ageofempires.fandom.com/wiki/Elevation>

The damage formula is treated as corroborated gameplay evidence, not an
original source-code claim.

## Reconstruction policy

- Each tile stores integer elevation `0..7`; old scenarios and saves migrate
  to level `0`.
- Land pathfinding permits adjacent steps whose elevation difference is at
  most one. Original terrain editing smooths surrounding heights and therefore
  renders elevation through one-level slope topologies; larger discontinuities
  remain impassable malformed/native-fixture edges and do not invent a face.
- Primary unit, building-target, and non-splash projectile damage receives
  `125%` downhill or `75%` uphill after armor, with the existing minimum of one.
  Equal height is unchanged. Secondary/splash damage is unchanged because
  available evidence does not prove that every secondary projectile receives
  the modifier.
- Scenario format `63` and save format `103` persist elevation. Replay format
  remains command-only: replay determinism depends on the initial
  scenario/save map, whose elevation is now persisted and included in the
  deterministic saved-state hash.
- Commercial scenario conversion clamps the decoded signed byte to supported
  reconstruction levels `0..7`; no inferred hills are synthesized.

## Rendering

Elevation projection uses the stored level as a vertical isometric offset.
Renderer derives slope ID with exact `FUN_004fd590` precedence over the 3x3
height neighborhood. Flat archive pixels are encoded in the classic terrain
scanline address space, then each FilterMaps output pixel consumes:

- one row width byte;
- a 16-bit header whose low nibble is sample count and high 12 bits are the
  lighting-table offset;
- one or more 24-bit samples whose bits `9..23` are source offsets and bits
  `0..8` are RGB weights.

Weighted RGB passes through the exact slope-direction PatternMasks record,
the selected lightMaps record, the corresponding 32x32x32 `view_icm` cube,
and palette 50500. IDs use the four original lighting orientations proved by
the executable switch. Generated textures retain their 25/49/73-row shapes,
hotspots, alpha, and diagonal depth order. No procedural brown wall, inferred
directional shade, or ambient-occlusion edge remains. Terrain Blendomatic
composition remains the top-surface boundary system; elevation adds no second
invented edge pass.

Camera centering includes tile elevation. World picking already uses elevated
tile diamonds. Minimap remains plan-view, matching its role as terrain-state
rather than perspective raster.

`resources/elevation-transition-matrix.scenario` is the visual capture matrix:
its upper row covers isolated/cardinal/corner one-level shapes and its
lower-left hill covers graded transitions. Lower-right two- and three-level
blocks are malformed-edge regression cases: they remain impassable without
synthesizing geometry absent from the original classifier.

`elevation_render_tests` parses packaged source assets and exhaustively pins
all 17 record heights and opaque-pixel totals, exact representative slope IDs,
all lighting asset counts, truncation rejection, and deterministic unlit/lit
scanline hashes. These tests fail if the old geometry path or an approximate
filter/lighting formula returns.

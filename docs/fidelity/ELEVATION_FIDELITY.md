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
  only to SLP `0`; it provides no cliff-face mapping. A search of all 7,014
  DAT graphic records found no graphic name or filename containing `cliff`,
  `slope`, or `hill`. Rock-named graphics are object graphics and are not
  treated as elevation evidence.
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
  most one. Larger differences form an impassable cliff edge. This is a
  reconstruction transition policy; decoded scenarios provide elevations but
  not explicit per-edge cliff geometry.
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
All elevation discontinuities retain procedural exposed faces because no
exact original cliff/slope bitmap asset mapping was proved. One-level slopes
and multi-level cliffs select exposed south/east faces from neighboring
elevation differences. Their directional shade and base-edge
ambient-occlusion accent are inferred presentation. Archive flat-terrain
sampling is explicitly capped at the 100 physically present frames.

`resources/elevation-transition-matrix.scenario` is the visual capture matrix:
its upper row covers isolated/cardinal/corner one-level shapes, its lower-left
hill covers graded transitions, and the lower-right blocks cover two- and
three-level procedural cliffs. Capture once with `AOE_ASSET_ROOT` pointing at
the live original install and once without it to compare exact original top
surfaces with the full procedural fallback.

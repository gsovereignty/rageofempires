# Terrain fidelity

## Live DAT evidence

Evidence was read from the live `empires2_x1_p1.dat` through
`tools/dat_metadata`. Represented terrain IDs and archive SLPs are Grass `0`
(`15001`), Water `1` (`15002`), Beach `2` (`15017`), and Shallows `4`
(`15014`).

Relevant terrain-restriction passability values are:

| Restriction | Represented use | Grass 0 | Water 1 | Beach 2 | Shallows 4 |
|---:|---|---:|---:|---:|---:|
| 3 | ships | 0 | 1 | 1 | 1 |
| 4 | generic buildings | 1 | 0 | 0 | 0 |
| 6 | dock | 0 | 1 | 1 | 1 |
| 7 | workers and land units | 1 | 0 | 1 | 1 |
| 13 | fish trap | 0 | 1 | 1 | 1 |

Unit records independently connect Fishing Ship `13` and Fish Trap `199` to
restriction `13`, Villager `83` to restriction `7`, Galley `539` to
restriction `3`, and Dock `45` to restriction `6`.

## Bounded reconstruction contract

Beach and Shallows are non-resource terrain. Land units traverse both; ships
also traverse both. Water remains ship-only. Generic buildings remain
Grass-only. Fish Traps accept Water, Beach, and Shallows.

These rules apply through unit creation, command validation, pathfinding,
formation placement, movement updates, ship production/trade destinations,
building construction, save/load, scenario parsing, and deterministic replay.
Numeric nonzero restriction values are treated as allowed. Exact original
movement-cost interpretation is not claimed because represented entries expose
binary `0`/`1` values and no distinct cost for these four terrains.

Renderer first uses optional loose Grass, Water, Beach, and Shallows PNGs.
Missing loose textures fall back individually to SLPs 15001, 15002, 15017,
and 15014 from `Data/terrain.drs`, decoded with palette 50500 from
`Data/interfac.drs`. Missing or invalid user-owned assets retain procedural
fallback; archive contents are read in place and never bundled.

Proved classic transitions are documented in
`TERRAIN_TRANSITION_FIDELITY.md`. Exact executable evidence resolves
one-cardinal variants with `(destination_x + destination_y) & 3`; calls
lacking destination position retain unblended fallback.

Dock restriction `6` has same represented values as ship restriction `3`, but
live DAT does not establish how its multi-tile shoreline footprint maps to
terrain cells. Reconstruction currently anchors Docks on Grass beside water.
That placement contract remains unchanged pending original-runtime footprint
evidence; restriction `6` is recorded above but not directly applied.

The original Age of Kings manual narrows placement to shallow water or
shallows beside a coast. Live Dock `45` also exposes terrain restriction `6`
and selection shape `3`, but the current metadata extractor does not expose a
clearance rectangle, rotation/orientation, or which cells of that shape must
touch land. Neither source resolves whether the simulation anchor denotes a
land cell, a water cell, or a mixed rotated footprint. Coast-edge rotation,
collision, and ship spawn/rally mapping therefore remain blocked rather than
guessed.

## Reproduction

```sh
cargo run --quiet --release \
  --manifest-path tools/dat_metadata/Cargo.toml -- \
  /path/to/empires2_x1_p1.dat
```

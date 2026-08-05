# Fog-of-war rendering fidelity

## Evidence boundary

Authoritative local evidence is the pinned executable with SHA-256
`e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
and installer-owned `TileEdge.Dat`, `BlkEdge.Dat`, `interfac.drs`, and
`graphics.drs`. `generated/fog_rendering_catalog.json` records hashes, sizes,
table structure, executable string offsets, proved facts, and missing links.

The executable directly names `TileEdge.Dat`, `BlkEdge.Dat`, and
`diam_map::draw_explored_tiles`. Its load routine proves both edge files begin
with 17 shape offsets. Each shape has 47 classes: `TileEdge.Dat` stores two
pointers per class and `BlkEdge.Dat` one. The inspected files satisfy all
pointer bounds and have exact SHA-256 identities.

The same executable constructs a 256-entry neighbor-mask normalization table.
The audited algorithm reproduces exactly 47 canonical classes numbered 0–46.
`FUN_0054f970` proves the compass mapping:

- bit 0: northwest
- bit 1: southwest
- bit 2: southeast
- bit 3: northeast
- bit 4: west
- bit 5: south
- bit 6: east
- bit 7: north

Its boundary branches independently confirm the same coordinate offsets.
Diagonal bits normalize away unless both adjacent cardinal bits are present.

`FUN_00555020` proves state and shape selection. The tile's stored shape byte
directly indexes one of the 17 `TileEdge.Dat` and `BlkEdge.Dat` tables. Hidden
tiles use `TileEdge` class 0 and no `BlkEdge`. Explored tiles use `TileEdge`
class 0 plus an explored-neighbor `BlkEdge` class. Visible tiles use a
visible-neighbor `TileEdge` class plus an explored-neighbor `BlkEdge` class.

`FUN_0050e520` and `FUN_0050e960` prove payload encoding. Both consume
`0xff`-terminated triples `{row, left, right}`. `TileEdge` triples add visible
scanline spans; `BlkEdge` triples remove scanline spans. These files therefore
encode geometry, not pixels, palette indexes, dither, or alpha.

## Recovered composition contract

`FUN_0054fb20` supplies original 8-bit composition constants. Explored terrain
uses alternating `0x56` stipple rows; hidden terrain uses `0x28` and is opaque
black. Reconstruction applies 50-percent terrain brightness beneath explored
stipple, then subtracts `BlkEdge` spans. Hidden tiles stay black. Visible tiles
use `TileEdge` spans before the same explored boundary subtraction.

Minimap consumes the same three-state visibility decision and black/half-bright
terrain colors. Mobile markers remain restricted to current visibility; enemy
building and wall markers retain their per-viewer last-seen image. Camera
viewport geometry uses the recovered inclusive bounds in `minimap_contract`.

No fog SLP resource ID is claimed: these are loose named DAT geometry files,
not DRS sprites.

## Renderer decision

World and minimap fog now use recovered composition. `generated/fog_edge_geometry.hpp`
contains pointer-free semantic spans derived from every 17x47 entry; original
pointer tables, filenames, and DAT files are not shipped or read at runtime.
`tools/generate_fog_edge_geometry.py` reproduces this tracked artifact from
user-owned archives. `fog_rendering_contract` rejects every out-of-range shape
or edge class and exposes only terminated `{row,left,right}` payloads.

World rendering normalizes all eight compass neighbors, selects exact edge
classes, scales original 96x48 scanlines to reconstruction's 64x32 diamonds,
applies both TileEdge halves, removes BlkEdge spans, and overlays recovered
explored stipple. Minimap uses matching state colors, visibility-gated markers,
and recovered viewport bounds. Runtime and build have no parent path fallback.

## Enemy building memory

Supplied Age of Kings manual, printed page 34, says explored enemy buildings
and walls remain visible. Their displayed upgrades, damage, and destruction do
not change until friendly sight returns. Decompiled `FUN_004f5f50`
independently shows explored tiles remain a distinct draw list through
`diam_map::draw_explored_tiles`; manual remains authority for object-level
stale-state semantics.

Simulation owns ordered per-player building memories. Seeing any footprint tile
atomically captures kind, owner/color, position, construction and gate state,
hit points/damage inputs, owner age, civilization architecture, maximum hit
points, and wall topology. Hidden mutation or destruction cannot change record.
Seeing remembered footprint again refreshes live record or removes destroyed or
replaced one. Diplomacy changes remove records no longer enemy, except Town
Centers retained by starting-team reconnaissance; Cartography uses same
shared-LOS path. Observers bypass memory and see live state.

World and minimap consume only frozen record while footprint stays hidden. No
production queue, garrison, live damage overlay, current age, current
civilization, current maximum HP, or changed wall topology feeds stale image.
Native save version 118 serializes records in entity-ID order; lockstep hash
therefore covers every player's distinct information state.

## Starting allied Town Centers

Supplied Age of Kings manual, printed page 45, says allied Town Centers are
visible when a team game starts; Cartography separately makes all allied units
and buildings visible. This establishes a narrow initial object reveal, not
pre-Cartography sharing of each Town Center's sight radius. Decompiled fog
rendering keeps explored and currently visible tile lists distinct, matching
that two-state contract; manual remains authority for special starting object.

During tick-zero setup, each occupied viewer slot snapshots every Town Center
owned by a player it regards as allied. All footprint tiles become explored,
but none becomes currently visible from this rule and no surrounding tile,
allied unit, or other allied building is revealed. Directed diplomacy is read
from viewer toward owner, so asymmetric scenario diplomacy remains
deterministic. Multiple and unfinished starting Town Centers retain independent
frozen construction, HP, age, civilization, and owner images.

Snapshot uses same ordered `BuildingMemory` consumed by world and minimap.
Later hidden construction, damage, age change, destruction, replacement, or
diplomacy change cannot leak live state. A post-start alliance cannot reveal a
previously hidden Town Center. Cartography suppresses frozen image whenever
normal shared LOS sees live building and terrain; observers continue to bypass
fog entirely. Native save/load and replay checkpoints serialize snapshot, and
lockstep hash covers it for multiplayer determinism.

## Reproduction and tests

```sh
python3 tools/audit_fog_contract.py \
  --executable "/path/to/AoK HD.exe" \
  --tile-edge "/path/to/Data/TileEdge.Dat" \
  --black-edge "/path/to/Data/BlkEdge.Dat" \
  --interface-drs "/path/to/Data/interfac.drs" \
  --graphics-drs "/path/to/Data/graphics.drs"
python3 tools/test_audit_fog_contract.py
python3 tools/generate_fog_edge_geometry.py \
  --tile-edge "/path/to/Data/TileEdge.Dat" \
  --black-edge "/path/to/Data/BlkEdge.Dat" \
  --output generated/fog_edge_geometry.hpp
cmake --build build-release --target aoe_fog_rendering_contract_tests
./build-release/aoe_fog_rendering_contract_tests
```

Tests verify exact 256-to-47 normalization, all 93,281 derived span records,
17-by-47 dimensions, compass order, state selection, shape bounds, terminators,
pointer rejection, recovered dither constants, and enabled archive renderer.
`aoe_core_tests` additionally verifies stale destruction and age state,
per-viewer isolation, sight-return invalidation, save/load, and lockstep hash.
Starting-allied-Town-Center coverage includes four roster slots, team and
directed diplomacy, disabled broad shared vision, multiple/unfinished centers,
footprint-only exploration, post-start diplomacy, Cartography, observer view,
save/load, and deterministic hash.

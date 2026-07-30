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

## Missing exact contract

No authoritative local callsite currently proves:

- final hidden fill color or explored terrain color transform/dither;
- final terrain compositing outside the proved scanline-span clipper;
- minimap hidden/explored/visible colors, markers, masks, or viewport treatment.

DRS hashes prove exact archives, but neither archive supplies names linking a
resource ID to fog. The edge assets are loose named DAT files, so no fog SLP
resource ID is claimed.

## Renderer decision

World and minimap fog remain procedural. Exact selector logic now lives in
`fog_rendering_contract`: compass mask construction, 256-to-47 normalization,
state-to-edge selection, direct tile-shape selection, and payload facts. World
unexplored tiles still use the current opaque dark fill; explored-but-not-visible
tiles retain current texture/color darkening. Minimap retains its independent
hidden fill and explored terrain darkening. Entity visibility remains
simulation-driven.

Archive-backed fog remains disabled because original edge DAT files are absent
from the reconstruction runtime and final hidden/explored color compositing is
not closed. Selector and span geometry are no longer guessed.

## Reproduction and tests

```sh
python3 tools/audit_fog_contract.py \
  --executable "/path/to/AoK HD.exe" \
  --tile-edge "/path/to/Data/TileEdge.Dat" \
  --black-edge "/path/to/Data/BlkEdge.Dat" \
  --interface-drs "/path/to/Data/interfac.drs" \
  --graphics-drs "/path/to/Data/graphics.drs"
python3 tools/test_audit_fog_contract.py
cmake --build build-release --target aoe_fog_rendering_contract_tests
./build-release/aoe_fog_rendering_contract_tests
```

Tests verify the exact 256-to-47 normalization, 17-by-47 file dimensions,
compass bit order, state selection, shape bounds, span terminators,
pointer-bound rejection, and fail-closed archive renderer decision.

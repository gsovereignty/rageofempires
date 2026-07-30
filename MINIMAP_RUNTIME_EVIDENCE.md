# Minimap runtime evidence

## Pinned inputs

- `AoK HD.exe`: SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
- `empires2_x1_p1.dat`: SHA-256
  `e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`
- `interfac.drs`: SHA-256
  `cb9e4d0f59d6cdb7af70da38cc910d0c33d210fe2d2a73dea17ef52a4ac8826e`
- palette resource: `interfac.drs:bina:50500`, JASC-PAL 0100,
  payload SHA-256
  `08251deb0ba2ebab6ac7326053ab12934d33d1215889c6f13c65e88d91fbc939`
- Ghidra C export:
  `/Users/gareth/Downloads/AOE/decompiled/AoK-HD-patched.c`

## Exact player marker palette fixtures

The first minimap color byte in each playable DAT player-color record is the
normal player marker color selected by the object pass. Palette resource 50500
resolves it as follows:

| player-color id | palette index | RGB |
|---:|---:|---:|
| 0 | 242 | 0, 0, 255 |
| 1 | 36 | 255, 0, 0 |
| 2 | 241 | 0, 255, 0 |
| 3 | 243 | 255, 255, 0 |
| 4 | 251 | 0, 255, 255 |
| 5 | 252 | 255, 0, 255 |
| 6 | 132 | 185, 185, 185 |
| 7 | 84 | 255, 130, 1 |

The other two serialized minimap color bytes are zero in every playable record.
Do not derive marker colors from the 16-color unit ramp or outline color.

## Terrain pixel pass

`FUN_004f5ec0` at `0x004f5ec0` draws every map tile by invoking minimap virtual
slot `+0xe8`. `FUN_004f5f50` at `0x004f5f50` separately obtains the explored
tile list from `FUN_00585650` and invokes the same tile primitive for each
listed coordinate.

The tile primitive resolves to `FUN_0053d0f0` at `0x0053d0f0`. It:

1. transforms map `(x,y)` through the precomputed diamond-row table;
2. tests the current player's explored and visible bit masks;
3. chooses one palette byte from the terrain record according to tile subtype;
4. draws a horizontal run whose width comes from the precomputed minimap row
   entry.

Normal-mode terrain palette selection uses terrain-record bytes at offsets
`+0xbc`, `+0xbd`, and `+0xbe`. The exact switch is:

- subtype 0 -> terrain `+0xbd`;
- subtypes 1,4,5,7,8,9,12,13,15 -> terrain `+0xbe`;
- subtypes 2,3,6,10,11,14,16 -> terrain `+0xbc`.

Modes greater than or equal to four do not use the latter two groups. The
meaning of minimap mode field `+0x180` values 0–3 is not named by local RTTI or
strings, so no gameplay label is assigned here.

When the primitive is called with its selected-area flag, it ignores the
terrain byte and uses minimap object `+0x16c`.

## Fog/exploration contract

`FUN_004f6c70` at `0x004f6c70` is the minimap render coordinator.

- On initial/full invalidation it clears, draws all tiles, then clears the
  invalidation flag.
- On incremental frames it calls `FUN_004f5f50`, which redraws only the
  explored-tile list returned by `FUN_00585650`.
- `FUN_0053d0f0` checks both player masks before emitting terrain pixels.

This proves the two-pass/full-versus-explored update structure and mask
participation. It does not prove semantic names for the two world bit masks,
the hidden fill palette byte, or whether the incremental list represents
newly explored, dirty explored, or all explored tiles. Those fields must not
be replaced by guessed visible/explored darkening constants.

## Object pass, ordering, and size

`FUN_0053e2e0` at `0x0053e2e0`
(`diam_map_view::draw_objects`) renders objects after the terrain image.

The pass order visible in the executable is:

1. neutral/world object list;
2. players in numeric order, with several owner lists handled by class;
3. transient list (up to 40 entries);
4. normal visible-object list;
5. signal/flare objects.

Visibility is checked in `FUN_0053d600` at `0x0053d600` against both world
mask bits before dispatching the object marker primitive. Hidden objects return
without drawing. Several object-class and ownership branches intentionally use
owner color, neutral color, alternate color, or no marker; the full
class-to-policy table is not recovered and must not be generalized to “all
units are one pixel/all buildings are 3x3.”

The low-level marker primitive `FUN_004f60a0` at `0x004f60a0` proves:

- size argument 1 draws an inclusive rectangle from `(x-1,y-1)` to
  `(x+1,y+1)`, exactly 3x3 pixels;
- its other branch uses a different low-level primitive, but the decompiler
  did not recover that branch cleanly enough to claim exact dimensions.

## Signal markers

The object pass toggles signal phase every time more than 332 ms has elapsed
(`timeGetTime`, fields `+0x188/+0x18c`).

Signal object type `0x112` uses palette byte `DAT_0081d05a` in one phase and
`DAT_0081d05b` in the other. It calls `FUN_0053cf70` at `0x0053cf70` with size
4. That helper draws an axis-aligned square outline with corners
`center ± 4`, first erasing/drawing black support lines and then drawing the
colored outline. The exact RGB of the two global palette bytes remains
unlinked to a named DAT field.

## Scaling and diamond projection

`FUN_004f5820` at `0x004f5820` rebuilds the minimap row/projection table when a
map is assigned. Its inputs include map width and height, target minimap
dimensions, and a per-row width table. It computes row spans, scale factors,
integer row widths, and cumulative offsets before either full or incremental
drawing.

`FUN_0053d0f0`, `FUN_004f60a0`, and `FUN_0053cf70` all use that same table, so
terrain, objects, and signals share one projection at every map size.

Raw disassembly recovers these formerly lost operands:

- map width = map header signed short `+8`;
- map height = map header signed short `+0xc`;
- source diagonal-row count = `width + height - 1`;
- requested output-row count = minimap signed short `+0x14c`;
- source-row step = `(width + height - 1) / output_row_count`;
- each output row samples a source diagonal row from an accumulated source-row
  position and stores a 36-byte projection entry;
- source diagonal span is
  `row+1` before the width edge and
  `width + height - row - 1` after it, with the asymmetric-map middle section
  clamped to the smaller map dimension;
- horizontal pixel span is derived from that diagonal span and the target
  per-row width at minimap table `+0x13c`;
- minimap `+0x154` is accumulated horizontal scale divided by output-row count;
- minimap `+0x158` is `(width + height - 1) / output_row_count`;
- vertical pixel thickness `+0x15c` is the ceiling of the accumulated scale
  divided by source-row step, and is clamped to at least one;
- `+0x15e` is average accumulated horizontal run width divided by
  output-row count, clamped to at least one.

Conversions call runtime helper `0x0072421c`, the statically linked
`_ftol2`-family C conversion helper. Raw disassembly proves its fast path stores
the x87 value as double and executes SSE2 `cvttsd2si`, which truncates toward
zero independently of MXCSR/x87 rounding modes. Its fallback performs the same
C integer-conversion semantics. All minimap builder operands are nonnegative,
so direct calls are exact floor. Several call sites compare that floor result
with the original float and add one, proving ceiling where stated above.

Therefore halfway fixtures are exact:

- accumulated source row 0.5 -> sampled row 0;
- 1.5 -> 1;
- 2.5 -> 2;
- any positive non-integer value used by an explicit compare-and-increment
  site -> mathematical ceiling.

The executable constant at `0x007f98c8` is x87 control word `0x027f`
(round-to-nearest, 53-bit precision), and CRT math helpers temporarily load it
when necessary. It does not alter `_ftol2` truncation.

This is sufficient to reject a simple closed-form diamond scale. It is not yet
sufficient for a bit-exact independent implementation without porting the
36-byte row-table builder and matching its x87 conversion mode.

## Panel, crop, and viewport

The minimap image is drawn through the generic view/surface object attached at
minimap `+0x17c`; `FUN_004f5ce0` proves row pitch, orientation, clip limits, and
hidden-fill writes come from that surface. Exact HUD frame resource, panel
rectangle, crop inset, and resolution-dependent anchoring are not linked by
the recovered call chain.

The 640-wide UI creation path at executable call site `0x00621c00` names
`map640.bmp` and passes resource ID `0xc4e1` (50401) to `FUN_004f68d0`.
Installer `interfac.drs:bina:50401` is an 8-bit BMP, 246x123 pixels, 31,582
bytes, SHA-256
`fb43c1b8db286febd0a6b416c9f7bfc33e45cc33d54a9d8e21f6e5a027e41d32`.
This proves the classic 640-layout frame asset identity and dimensions. It
does not prove the active minimap image crop within that bitmap or its exact
screen anchor.

The larger-layout path at `0x005f3ad3` names `map1024.bmp` and passes resource
ID `0xc4e8` (50408). `interfac.drs:bina:50408` is an 8-bit BMP, 326x164 pixels,
54,870 bytes, SHA-256
`72d83fad3a2cff76eb71a0deb0eaa391094749c6dedca09594c9e64a2043a2af`.
The raw caller places its 326x164 view at:

- `x = screen_width - 336`;
- `y = screen_height - 169`;
- `width = 326`;
- `height = 164`.

Thus the 1024-class frame has a ten-pixel right margin and five-pixel bottom
margin. The other setup at the same site creates a 420x16 view at `(2,2)`;
that is a distinct sibling view, not evidence for the map-image crop.

Viewport drawing is minimap virtual slot `+0xfc`, resolving to
`FUN_004f62a0` at `0x004f62a0`.

- Camera/world-map floating coordinates come from world-view fields
  `+0x174/+0x178` and are converted to tile coordinates through `0x0072421c`.
- The camera tile is transformed through the same 36-byte minimap row table.
- `FUN_0054f790` supplies the current world viewport width and height.
- The cached minimap bounds are:
  - left = transformed x minus converted `(viewport_width / 2) * scale_x`;
  - top = transformed y minus converted `viewport_height * scale_y`;
  - right = transformed x plus converted
    `(viewport_width / 2 + 2) * scale_x`;
  - bottom = transformed y plus converted
    `(viewport_height + 4) * scale_y`.
- It rasterizes the resulting clipped diamond/trapezoid one minimap scanline
  at a time. Boundary pixels/runs use palette index 255. The prior/offset
  support line is overwritten with palette index 0.
- It caches the last camera tile at `+0x160/+0x162` and only recomputes bounds
  when that tile changes, while redrawing the outline on each call.

Panel crop/inset and non-640 resolution anchoring remain explicit blockers.
Do not treat the reconstruction's 214-pixel panel, 7-pixel padding, beige
viewport polygon, or floating-point diamond formula as original-executable
facts. The executable viewport is palette-indexed 255-with-0-support, not the
reconstructed beige contract.

## Pure implementation contract

The proved, renderer-independent subset is implemented in:

- `include/aoe/minimap_contract.hpp`;
- `src/minimap_contract.cpp`;
- `tests/minimap_contract_tests.cpp`.

It exposes positive floor conversion, source-diagonal scaling rows, the eight
player palette indexes, 3x3 size-one marker bounds, type `0x112` signal phase
and center±4 outline bounds, 1024 frame placement, and proved viewport bounds.
It deliberately exposes `viewport_scanline_polygon_proved == false` and
`map640_anchor_proved == false`; neither unknown is filled with a guessed
renderer policy.

## SDL integration

The SDL minimap now consumes that pure contract:

- map diagonals use `build_scaling_rows` for render placement and click
  inversion;
- playable owners use the eight exact palette-50500 RGB marker colors;
- size-one entity markers use exact inclusive 3x3 bounds;
- signals use the proved 333 ms phase boundary and center±4 outline;
- 1024-class windows place the 326x164 frame at the proved margins;
- camera projection computes the proved cached bounds.

The prior beige camera polygon is disabled. Bounds are computed but not
rasterized because scanline polygon details remain unproved. The 640 anchor is
also unchanged/procedural, and the unknown alternate signal palette byte is
not assigned a guessed color.

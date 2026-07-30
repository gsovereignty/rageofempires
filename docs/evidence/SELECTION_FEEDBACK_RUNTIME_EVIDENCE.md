# Classic selection and health feedback runtime evidence

## Pinned inputs

- `AoK HD.exe`: SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
- `empires2_x1_p1.dat`: SHA-256
  `e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`
- palette resource `interfac.drs:bina:50500`: payload SHA-256
  `08251deb0ba2ebab6ac7326053ab12934d33d1215889c6f13c65e88d91fbc939`
- Ghidra C export:
  `/Users/gareth/Downloads/AOE/decompiled/AoK-HD-patched.c`

Machine-readable fixtures live in
`generated/selection_feedback_catalog.json`. Regenerate them with
`tools/audit_selection_feedback.py`; its PE checks fail closed on an
unrecognized executable. Archive dimensions come from the independently
generated `generated/ui_icon_catalog.json`.

## Exact procedural original primitives

`stat_obj::draw_frame_3d_square_back` at `0x00587e00` and
`stat_obj::draw_frame_3d_square_front` at `0x00587ff0` project the master
record's X/Y outline radii around the object's screen offsets. Each function
draws two sides, producing a four-segment diamond/square. Both select Win32
stock object 6, `WHITE_PEN`. This is not an SLP and is not colored from the
owner's player ramp.

Raw disassembly also recovers the function omitted by the C export:
`stat_obj::draw_frame_3d_cube_back` starts at `0x005882c0`.
`stat_obj::draw_frame_3d_cube_front` starts at `0x00588730`. They use X/Y/Z
outline radii, trim segments at exact factors 0.25 and 0.75, draw with the
same white stock pen, and apply an extra -16 screen-Y offset. The catalog
records exact segment counts and constants.

DAT `selection_shape`, `outline_radius`, collision `radius`, maximum HP, and
`selected_sound` are serialized per unit. The generated catalog exposes only
unit records whose five fields agree across every enabled civilization copy;
civilization-varying IDs are listed separately. A mapping from numeric
`selection_shape` values to the square/cube virtual methods is not recovered,
so no such mapping is guessed.

## HD feedback path and selected-state gate

Application dispatch is exact even though its mode names are not. Renderer byte
`+0xe8 == 1`, or application mode `+0x78 == 1`, selects cube front/back.
Application modes 2 and 3 select square front/back. With hardware available,
renderer byte `+0xe8 != 1`, and application mode 2 or 3, the hardware path
replaces those GDI calls; mode 3 uses collision radii while mode 2 uses outline
radii. Numeric mode labels such as "classic" or "HD" remain unproved.

The same executable also contains the active hardware-rendered feedback path
at `FUN_0058bf30` (`0x0058bf30`). Unlike the older white GDI frame methods,
this path emits projected quadrilaterals or a tessellated circular outline
through the renderer's primitive queue.

`FUN_00583c90` (`0x00583c90`) proves object feedback byte bit 0 is selected:
it appends the object to the owning player's selection list (maximum 40) and
writes byte value 1. `FUN_00583e00` (`0x00583e00`) walks that list, clears
each object's byte, and resets the list count.

Selected-only additions require bit 0 set and bit 3 clear. Group-number
sprites also require a nonzero group mask and local-player ownership. The HD
health primitive additionally requires master byte `+0xb8` bit 1 clear.

Base outline palette indexes are 255 normally and 133 in one alternate
render mode. Bit 0 plus bit 3 uses index 243. Transient bit-1 and bit-2
overrides use primary indexes 241 and 243 respectively and alternate against
support index 36 on the `timeGetTime() & 0x100` phase. Numeric mode semantics,
bit-1/bit-2 event names, and RGB for indexes 255/133 remain unproved.

The HD health geometry differs from the older GDI bar. It projects the selected
X/Y/Z radius corner, uses raw bounds X -16 through +15 and Y -3 through 0,
then splits fill at
`left + trunc(current_hp * 32 / maximum_hp)`, clamped to `right - 2`.
The generated fixture preserves these primitive operands rather than guessing
their final raster coverage.

## Exact health bar

The square-front function draws a health bar after its front selection edges.
Current HP comes from object float `+0x30` and is truncated toward zero by the
runtime conversion helper. Maximum HP is the signed short at master `+0x2a`.
No bar is drawn unless both converted current HP and maximum HP are positive.

Its anchor is the projection of
`(outline_radius_x, -outline_radius_y, object_height)` using the object screen
origin. Rectangle calls are inclusive:

- background: anchor X -12 through +12, anchor Y -2 through -1, exactly 25x2
  pixels;
- fill right edge:
  `anchor_x - 12 + trunc(current_hp * 24 / maximum_hp)`;
- full-health fill: exactly 25x2 pixels.

Palette byte globals are exact: background index 36, RGB `(255,0,0)`; fill
index 241, RGB `(0,255,0)`. There are no yellow/red health thresholds in this
primitive. Color changes are continuous geometry only: fixed red support,
green proportional fill.

The SDL runtime now uses this exact inclusive 25x2 geometry, truncates current
HP before the 24-pixel proportional scale, hides the primitive unless both HP
values are positive, and uses the proved RGB values of palette indexes 36/241.
The deterministic selection smoke capture is
`/tmp/aoe-selection-exact.bmp`, SHA-256
`697aa8c1c0598b50041fb192b388120a06515f56b77d4bcb8fc84c2498c44e56`;
visual inspection confirms the full-health 25x2 bar.

## Asset-backed candidates, bounded

Game UI construction loads `health.shp` as resource `0xc639` and
`unithalo.shp` as resource `0xcf0b`. Load identity is exact. Recovered code
does not prove their draw calls or connect either asset to world health,
hover, or selected states. They therefore remain candidates, not renderer
fixtures. Proven world square/cube frames and bars above are procedural.

The root resource manager also loads:

- `groupnum.shp`, resource `0xc4e3`;
- `waypoint.shp`, resource `0xc4e4`;
- `moveto.shp`, resource `0xc4e5`.

`groupnum.shp` has a proved selected-group-number draw path: selected bit 0,
bit 3 clear, nonzero group mask, and local-player ownership gate it.
Archive inventory proves 9 frames for `groupnum.shp` and one frame for
`waypoint.shp`. Logical groups 1 through 9 map exactly to physical frames
0 through 8 (`group_number - 1`), with an 8-pixel X advance between glyphs.
Resource `0xc4e5` is absent from the pinned interface archive,
so the executable's `moveto.shp` load cannot succeed against that archive.
`waypoint.shp` and `moveto.shp` have no recovered draw call. Their filenames
do not prove destination-marker, order-line, frame, timing, or color semantics.

Archive inventory also proves 26 frames for `health.shp` and one frame for
`unithalo.shp`, without proving their world-rendering roles.

## State, order, visibility, and audio limits

Selected state and its local group-number ownership gate are proved above.
No recovered callsite establishes hover state, general owner policy,
fog/visibility gate, destination flag, waypoint/order line, or range-ring
primitive. Existing reconstruction lines, diamonds, rings, and stance badges
must remain labeled reconstruction overlays, not classic rendering.

The pure selection visual contract therefore exposes exact mode dispatch,
geometry constants, state gates, palette indexes, and group-frame mapping, but
returns unknown for the missing DAT shape bridge. SDL unit/building captures
exercise that boundary; existing yellow diamonds remain unchanged rather than
claiming an unproved square/cube substitution. Group-number sprite rendering
also remains unconnected because the current world-render seam has no proved
group-membership input.

DAT `selected_sound` values are exact and included in unit fixtures. Selection
sound archive identity and trigger evidence remain documented in
`../fidelity/AUDIO_ARCHIVE_FIDELITY.md`; this visual catalog does not infer a click,
hover, or selection-change trigger from the field name alone.

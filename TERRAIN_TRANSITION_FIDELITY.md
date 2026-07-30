# Classic terrain-transition fidelity

## Pinned evidence

The selector and decoder are bounded to openage commit
`9a5a7ccbfc20c2de658fc746462cd4a69aa758ef`:

- `doc/media/blendomatic.md` documents the higher-priority overlay algorithm,
  eight-neighbor bit order, diagonal suppression, influence patterns, mask IDs
  0–30, the 8×8 blend-mode lookup table, and 0–128 alpha meaning.
- `openage/convert/value_object/read/media/blendomatic.py` proves the classic
  binary read contract actually used by the pinned converter: 9 modes,
  31 tiles, 2353 diamond pixels, 31 flag bytes, a packed 32-mask block of
  `tile_size * 4` bytes, then 31 `tile_size` alpha maps per mode.
- `doc/media/terrain.md` proves regular terrain frame selection
  `(x % 10) + (y % 10) * 10`, SLP frame dimensions, DRS priorities, and stored
  blend-mode meanings.
- Supplied 2013 `Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`,
  proves cardinal orientation and variant selection. Its neighbor table at PE
  VA `0x0081d310`, consumed by `FUN_00553b10`, stores NW, NE, SE, SW, N, E, S,
  W offsets with influence bits 1 through 128. `FUN_0054e400` maps those bits
  to mask IDs/families. `FUN_00553b10` computes
  `local_a8 = param_6 + param_7 & 3`; `FUN_0054e810` adds that value to mask IDs
  below 16. Decompiled evidence is at
  `decompiled/AoK-HD-patched.c:223604`, `:223831`, `:223870`, `:227738`, and
  `:227843`.
- Live DAT/SLP evidence already recorded in `TERRAIN_FIDELITY.md` maps the
  represented visuals to Grass SLP 15001, Water 15002, Beach 15017, and
  Shallows 15014. Pinned openage's terrain inventory gives their priorities
  102, 139, 110, and 60. Stored modes are 0, 3, 2, and 4.

The implementation does not bundle `blendomatic.dat`, palette data, or SLPs.
It reads them only from the user's `AOE_ASSET_ROOT`.

## Bounded implementation

`terrain_transition` supplies:

- strict classic `blendomatic.dat` parsing with size, count, truncation, and
  trailing-byte rejection;
- expansion of packed 2353-byte maps into 97×49 alpha diamonds;
- visual-terrain normalization matching the represented renderer
  (resource ground to Grass and Fish to Water);
- priority-grouped eight-neighbor influence selection;
- diagonal suppression and ascending-priority overlay order;
- fixed mask IDs 16–30;
- exact cardinal families: South 0–3, East 4–7, West 8–11, North 12–15;
- cardinal variant `(destination_x + destination_y) & 3`;
- 128-denominator RGBA composition.

The SDL renderer retains decoded terrain RGBA frames beside its textures,
loads Blendomatic only when the exact user-owned file exists, and caches
composed transition textures. It does not inspect unexplored neighbors, so
shore shape cannot leak fogged map information. Missing/malformed assets,
unsupported dimensions, incomplete SLP sets, or calls lacking destination
position preserve existing unblended archive/procedural fallback.

## Proved cardinal variants

Pinned documentation establishes four one-cardinal families but leaves its
“x or y” wording ambiguous. Exact executable evidence resolves that ambiguity:
all four families use low two bits of destination `x + y`. North uses 12–15,
East 4–7, South 0–3, and West 8–11. Renderer passes destination tile position
to selector. Position-less API calls retain unresolved family and fail closed.

`tools/audit_blendomatic_cardinals.py` verifies executable hash, parses exact PE
neighbor table, checks decompiled coordinate formula/application, and writes
`generated/terrain_transition_evidence.json`. This keeps orientation and
variant claims machine-readable and reproducible.

## Tests and fixture

`terrain_transition_tests` constructs a complete synthetic classic
Blendomatic byte fixture and covers:

- strict 9×31×2353 decoding and diamond padding;
- truncated-data rejection;
- fixed adjacent and diagonal mask IDs;
- diagonal suppression;
- separate overlay groups in ascending priority;
- pinned blend-mode lookup results;
- all four cardinal orientations and their `x + y` variant;
- position-less unresolved cardinal-family fallback;
- 0, half, and full 128-based alpha composition.

## Capture plan

After build recovery, use a small checkerboard scenario containing all pairwise
boundaries among Grass, Beach, Water, and Shallows.

1. Capture without `AOE_ASSET_ROOT`: procedural/unblended control.
2. Capture with valid terrain DRS/palette but no Blendomatic: archive tile
   control.
3. Capture with the same assets plus exact `Data/blendomatic.dat`: fixed-mask
   transitions should appear only at proved junctions.
4. Cover masks 0–30 individually, including all four coordinate variants for
   every one-cardinal direction.
5. Repeat with fog hiding one neighbor and confirm the hidden terrain does not
   affect the explored tile silhouette.
6. Compare against original-runtime capture of identical scenario/map
   coordinates to visually validate proved static-analysis mapping.

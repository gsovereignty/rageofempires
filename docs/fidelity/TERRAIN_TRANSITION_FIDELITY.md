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

Normal runtime resolves only packaged reconstruction-local `game_data`.
Research copies outside the repository are never runtime inputs.

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
loads the first valid classic candidate beginning with
`blendomatic_x1.dat`, and caches composed transition textures. Incompatible
HD `blendomatic.dat` no longer prevents the valid classic candidate from
loading. Cache identity includes base terrain/frame plus every overlay,
blend mode, and mask ID.

It does not inspect unexplored neighbors, so transition shape cannot leak
fogged map information. Missing/malformed assets, unsupported dimensions,
incomplete SLP sets, or calls lacking destination position preserve archive
tiles without invented mask selection.

When archive terrain is absent, the procedural renderer now draws one filled
four-vertex strip per represented cardinal edge. Vertex-color interpolation
provides a continuous world-space gradient over 22 percent of the tile
edge-to-center distance. Both adjacent tiles use the same 50/50 boundary
color, then converge monotonically to their own center colors. Shared corner
rays use identical inset geometry, avoiding gaps and spikes. Former seven
one-pixel lines and two hard shoreline lines were reconstruction artifacts;
at 1.25× zoom they became visibly stepped and are removed.

Local PNG, archive SLP, and cached Blendomatic textures all require linear
sampling after upload. Runtime checks both `SDL_SetTextureScaleMode` and
`SDL_GetTextureScaleMode`; failed configuration destroys the texture instead
of caching it. This sampling policy is inferred from non-integer reconstructed
camera scaling. Original evidence proves separate tile/blend texture sampling,
not a specific filter mode.

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
- procedural transition endpoint and monotonic-channel convergence.

`terrain_edge_sdl_smoke` captures the fixed pond at zoom 1.0, 1.25, and 1.5
through the asset-disabled path. A bounded ROI must contain at least 1,500
intermediate pixels, 150 distinct blended colors, and no horizontal
intermediate-color plateau longer than three pixels. Removing the filled
mesh or restoring seven discrete lines fails these checks. The pond notch
covers a corner junction.

The same smoke forces packaged archive terrain and
`blendomatic_x1.dat`, compares it with an audit-only unblended capture, and
requires at least 1,000 changed ROI pixels. Its log assertion confirms
classic 9-mode loading and queried linear texture sampling. These controls
affect test selection only: `AOE_CAMERA_ZOOM` accepts finite values in the
normal 1.0–2.0 range, `AOE_TERRAIN_ARCHIVE_ONLY=1` skips loose HD PNGs, and
`AOE_DISABLE_BLENDOMATIC_AUDIT=1` supplies the negative comparison.

## Remaining fidelity uncertainty

HD `FUN_0051e080` proves nine named blend textures, and `FUN_0051e1a0` proves
separate `g_TileTexture` and `g_BlendTexture` shader bindings. Classic loader
evidence proves packed 9×31 alpha maps. Neither proves reconstruction filter
choice, procedural strip width, or vertex interpolation. Those are explicit
readability choices. Original-runtime comparison and exhaustive visual
captures for all masks 0–30 remain future evidence work.

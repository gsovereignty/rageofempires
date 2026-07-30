# Classic player-color palette evidence

## Result

The installed 2013 HD payload carries an exact, implementation-ready classic
AoC paletted remap for all eight playable color IDs. It does **not** yet prove
that the HD executable presents those RGB values without a later shader,
brightness, gamma, or display transform.

Pinned inputs:

- `interfac.drs` SHA-256
  `cb9e4d0f59d6cdb7af70da38cc910d0c33d210fe2d2a73dea17ef52a4ac8826e`;
- `graphics.drs` SHA-256
  `b0541bbf9dc45cdef85eb50563d5412026efc56306334b66a88b56557d11bfdf`;
- `empires2_x1_p1.dat` SHA-256
  `e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`;
- `AoK HD.exe` SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`.

## Exact classic remap

`interfac.drs` resource `bina:50500` is a 256-entry JASC-PAL 0100 palette.
Its payload SHA-256 is
`08251deb0ba2ebab6ac7326053ab12934d33d1215889c6f13c65e88d91fbc939`.

VER 5.7 DAT color-table IDs 0 through 7 select these base palette indices:

| Color ID | Conventional name | Base |
|---:|---|---:|
| 0 | blue | 16 |
| 1 | red | 32 |
| 2 | green | 48 |
| 3 | yellow | 64 |
| 4 | cyan | 96 |
| 5 | purple | 112 |
| 6 | gray | 128 |
| 7 | orange | 80 |

IDs 4 through 7 prove why `(color_id + 1) * 16` is wrong. Always read the DAT
base. Civilization and team do not choose this base; the match/scenario player
color-table ID does.

Classic SLP player-color list command `0x06` and fill command `0x0a` carry a
source index. The paletted resolver contract is:

```text
resolve_player_pixel(color_id, source_index):
    require 0 <= color_id < 8
    require 0 <= source_index < 16
    base = dat.player_colors[color_id].base_palette_index
    return palette_50500[base + source_index]
```

All 1,768 classic SLP resources in the pinned `graphics.drs` decode under the
scanner. Player commands use source indices 0 through 9; none use 10 through
15. The catalog records separate list/fill histograms, all 16 exact RGB values
per color, per-ramp RGB hashes, outline/minimap palette indices and RGB values,
and statistics text-color IDs.

Offset 4 fixture across color IDs 0 through 7:

```text
[(74,121,208), (255,0,0), (0,87,0), (255,247,37),
 (0,172,150), (211,58,201), (185,185,185), (255,130,1)]
```

## AoC versus HD boundary

The DAT, JASC palette, and classic SLP command mapping are AoC-format asset
behavior carried inside the 2013 HD release. Static executable work has not
yet recovered the final HD texture/upload/display transform. Therefore no
extra RGB tint, gamma correction, brightness multiplier, or shader transform
is authorized. Implement the classic resolver above for asset-faithful
paletted output; classify pixel-perfect HD presentation as pending the missing
post-palette transform.

Machine-readable evidence is
[`generated/player_color_palette.json`](generated/player_color_palette.json).

# Player-color palette fidelity

## Proven mapping

`generated/player_color_palette.json` records palette 50500 and DAT player
color-table bases:

`[16, 32, 48, 64, 96, 112, 128, 80]`

For playable roster slot `0..7`, every SLP player-color source index `0..9`
maps exactly to:

`palette_index = base[slot] + source`

Neutral ownership leaves source palette index unchanged. No tint, gamma,
channel scaling, or inferred color transform runs after palette lookup.

## Runtime policy

- SLP player-color list, fill, and extended commands share one resolver.
- Indexed sprite, composite, and animated-composite caches hold slots `0..7`.
- Owner lookup uses `EntityOwner::slot_index()`.
- Missing, neutral, or unsupported playable variants do not alias red.
- Legacy decoder player numbers `1` and `2` preserve prior blue/red bytes.
- Source indices above `9` fail as malformed/unsupported asset data.

## Verification

`player_color_palette_tests` checks all 80 playable mappings, neutral
preservation, invalid inputs, and exact blue/red compatibility.

`aoe_legacy_assets_tests` decodes synthetic SLP player-color commands through
all eight slots. It checks expected palette bytes and proves remapping changes
neither alpha footprint nor any non-player pixel. This prevents palette decode
from creating opaque rectangles outside original SLP coverage.

## Source evidence and boundary

Pinned evidence uses `interfac.drs` resource `bina:50500`, the VER 5.7 DAT
player-color table, and classic SLP player-color commands from the supplied
2013 HD payload. Palette 50500 is a 256-entry JASC-PAL 0100 resource. DAT
color IDs `0..7` select base indices:

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

Classic list command `0x06` and fill command `0x0a` carry source indices.
The scanner decoded all 1,768 classic SLP resources in the pinned
`graphics.drs`; player commands use source indices `0..9`.

This proves classic paletted remapping carried by the HD assets. It does not
prove any later HD texture, shader, brightness, gamma, or display transform.
No such post-palette transform is inferred.

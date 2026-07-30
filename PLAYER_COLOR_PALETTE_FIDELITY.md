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

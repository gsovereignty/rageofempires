# UI archive fidelity

This lane uses interface art only when the user supplies a legally obtained
installation through `AOE_ASSET_ROOT`. No interface SLP, palette, cursor, or
other proprietary payload is bundled.

## Supplied archive audit

Audit root: `/tmp/aoe-assets.grSVdf/app` during development. Its
`Data/interfac.drs` is a classic readable DRS archive and its resource 50500 is
a readable JASC palette.

The following metadata was decoded directly from that supplied archive:

| Role | SLP | Frames | Audited frame | Size | Mapping status |
|---|---:|---:|---:|---:|---|
| Unlinked full-screen sheet | 51141 | 1 | 0 | 1280×1024 | Archive metadata proved. Pinned executable loads civilization-selected loose `game_b%d.slp` with resource ID -1, then composites frames 0–7. One-frame SLP 51141 is structurally incompatible and is not the proved HUD background; see `HUD_LAYOUT_FIDELITY.md`. |
| Action sheet | 50721 | 69 | 0 | 36×36 | Archive metadata and command/action role executable-proved; frame 0 meaning unproved |
| Portrait frames | 50713 | 4 | 0 | 54×54 | Archive metadata proved; semantic role inferred |
| Cursor sheet | 51000 | 19 | 0 | 24×32 | Archive metadata proved; frame 0 visibly decodes as normal pointer |

Resource-icon frames 18–21 of 50721 and use of portrait frame 0 remain
reference-informed approximations. Exact historical selection rules by game
state, resolution, civilization, and screen are not proved.

## Runtime behavior

- With a valid `AOE_ASSET_ROOT`, the renderer loads palette 50500, uses the
  mapped HUD/action/portrait resources, and installs cursor-sheet frame 0 as an
  SDL color cursor.
- Without the archive, malformed resources, or metadata mismatch, the app
  keeps its procedural beveled HUD and platform default cursor.
- UI archive failure never changes simulation state.

The legacy approximate HUD path takes the bottom portion of unlinked
1280×1024 SLP 51141 and scales it into the reconstruction's 1280×80 logical
HUD. Pinned executable evidence disproves that asset as `game_b%d.slp`; dark
translucent information panels keep debug-font text readable. This crop,
scale, panel
geometry, control placement, text, and the 80-pixel HUD height are
reconstruction approximations, not claims of original pixel layout.

## Catalog and validation

`ui_assets` centralizes IDs, frame indexes, expected counts and dimensions.
Its audit API reports archive/palette availability, decode success, and
metadata matches per role. Tests cover catalog uniqueness, bounded frame
indexes, missing-archive fallback, and—when `AOE_ASSET_ROOT` is present—the
live supplied archive mappings.

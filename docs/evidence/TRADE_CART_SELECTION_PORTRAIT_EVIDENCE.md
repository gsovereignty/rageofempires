# Trade Cart selection portrait evidence

## Result

`BUG-VISUAL-003` is resolved. Selected Trade Carts use original interface
sheet 50730 frame 34 instead of synthetic head/body rectangles.

## Original contract

Read-only VER 5.7 DAT unit 128 (`TCART`) stores button icon 34 at record
offset `+0x54`. Decompiled `FUN_005c7560` classifies ordinary units separately
from building subtypes 2 and 10, then passes that signed-short field unchanged
to `FUN_005c5e40`. Executable setup loads `ico_unit.shp` as resource 50730
(`0xc62a`). Together these prove exact dispatch `unit 128 -> 50730:34`.

Supplied HD `Data/interfac.drs` used for read-only validation has SHA-256
`cb9e4d0f59d6cdb7af70da38cc910d0c33d210fe2d2a73dea17ef52a4ac8826e`.
No archive or decoded bitmap enters tracked project files.

## Reconstruction contract and regression

`ui_icons::training_unit(UnitKind::trade_cart)` now returns exact executable
dispatch sheet 50730 frame 34. Shared HUD portrait selection prefers this
exact DAT/interface binding, retaining world-animation fallback only for unit
kinds without proved icon bindings.

`ui_icon_contract_tests` pins Trade Cart frame 34. `ui_icon_sdl_smoke` launches
the tracked 255x255 `trade-cart-selection-regression.scenario` twice under SDL
dummy/software drivers, selects Trade Cart through deterministic test control,
and requires byte-identical captures. Opt-in `AOE_UI_ICON_VISUAL_PROOF=1`
requires at least 64 colors in portrait interior when original archive assets
are enabled; synthetic fallback contains only flat panel/body fills and fails.

Background proof is under `artifacts/bug-visual-003/`. Capture SHA-256:
`2b11db327c1b9dbe1b1bfed8932d69b213062f49b3187c2a0e59782533a30b87`.
Artifact remains disposable and untracked.

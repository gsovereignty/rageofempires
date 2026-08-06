# Naval selection portrait evidence

## Result

`BUG-VISUAL-011` is resolved. Selected Galley, War Galley, Galleon, and
Transport Ship units use distinct original unit-interface artwork instead of
the procedural two-rectangle placeholder.

## Root cause

At commit `45aed8f`, immediately before the shared portrait correction,
`render_hud` asked only `legacy_action_for` for selected-unit portrait art.
Galley and Transport Ship production rendering uses composite asset paths, not
a single `LegacyAnimation`; both lookups therefore returned no portrait. The
renderer reached its exact fallback branch and drew a 16x16 head block plus a
32x26 body block. Packaged 1280x720 reproduction through ordinary world clicks
is preserved locally under `artifacts/bug-visual-011-before*.png`; Galley and
Transport hashes are `7e43cdf2545439a1614b5da69446f1be554fffc15ce845ded637abdc0e46180e`
and `c57a2415efc6b9d220d592031ded9df0521cd1437b7469499e3cc9c26bde5301`.

Commit `c3fc0ef6` subsequently made the shared production HUD prefer exact
unit-interface bindings before world-animation fallback. Galley frame 87 and
Transport Ship frame 95 were already in `ui_icons::training_unit`, so that
shared correction removed their blank portraits. This change completes the
line by binding the distinct runtime War Galley and Galleon kinds and adds a
naval-specific regression that prevents the incidental correction from being
lost.

## Original contract

VER 5.7 records expose button-picture fields 87 for Galley (unit 539), 25 for
War Galley (21), 60 for Galleon (442), and 95 for Transport Ship (545).
Read-only decompiled `FUN_005c7560` passes the ordinary-unit record signed
short at `+0x54` unchanged into `FUN_005c5e40`. Executable setup binds
`ico_unit.shp` to interface resource 50730. Exact dispatch is therefore
`50730:87`, `50730:25`, `50730:60`, and `50730:95`.

## Production and regression proof

`render_hud` resolves every selected runtime kind through
`ui_icons::training_unit`, then reads that exact frame from packaged
`unit_command_icons`. The same path is independent of movement, attack,
damage, owner color, civilization, save/replay restoration, and observer view;
those states do not replace `Unit::kind` or the selected entity lookup.

`ui_icon_contract_tests` pins all four frames. `ui_icon_sdl_smoke` launches the
tracked naval regression scenario through the real application for every kind,
requires deterministic captures, and offers archive-backed pixel validation:
all four portrait interiors must contain at least 64 colors and be mutually
distinct. Missing archives continue to use the bounded procedural fallback.

Final packaged proof uses ordinary world clicks, not selection overrides, to
select all four runtime kinds in the shipping app. Captures and hashes are
stored under `artifacts/bug-visual-011/` and remain untracked:

- Galley: `d62cc23b865610ea4b3748dd4ecb3c6c135752c98049fd23dcf6ffc41a8f81c4`
- War Galley: `622a221e4f0d885d2e5bf3ec3cb241e8fd27099ac9d9436b97b118d89458c1a4`
- Galleon: `75e5f074b768bfabcbdcea0a465cd0786533829eeb5f3ac95c0e88f700b34eb7`
- Transport Ship: `61f1d5eb226642d1208abd8c051bd76a978e8d47d238d3ee5abbb522f50013b0`

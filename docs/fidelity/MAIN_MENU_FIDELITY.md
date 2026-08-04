# Main-menu fidelity

## Scope and evidence

Runtime provides a fixed 800×600 classic-style main screen and Single Player
flyout. Geometry comes from supplied 800×600 screenshot; it is measured
reference evidence, not recovered executable layout.

Recovered `FUN_00603970` and `FUN_006042a0` in
`decompiled/AoK-HD-patched.c` prove focusable/visible controls and named screen
transitions. They do not prove pixel positions. `main.sin` proves palette
50589, cursor 51000, button 50688, popup 50090, and documented colors.

## Audited assets

`slp_contact_sheet` decoded every loose candidate with interface palette
50589. Contact sheets remain untracked under `artifacts/menu-contact-sheets/`.

| Source | Decoded frames | Dimensions and hotspots | Runtime role |
|---|---:|---|---|
| `Data/Slp/main_32.slp` | 53 | frame 0: 1366×768, hotspot 0,0; frames 1–9, 18–21, 38–45: 1×1; frames 10–13: 192×258; 14–17: 161×188; 22–25: 218×254; 26–29: 123×97; 30–33: 160×147; 34–37: 137×139; 46–48: 230×137; 49–52: 557×173 | Frame 0 supplies Age of Kings background. A 1024×768 source region beginning at x=342 is uniformly scaled to 800×600. Frame 49 visibly supplies matching Age of Empires HD Edition logo art. Other frame semantics are unproved and unused. |
| `Data/Slp/xmain_32.slp` | 1 | frame 0: 1067×600, hotspot 0,0 | Visibly Conquerors-branded; audited but rejected for Age of Kings target. |
| `Data/Slp/btnmain.slp` | 17 | frames 0–15: 39×39, hotspot 0,0; frame 16: 36×36, hotspot -4,-4 | Frame ordering does not prove state semantics; audited but unused. |
| `Data/interfac.drs` SLP 51000 | 19 | frame 0 previously audited as 24×32 | Normal original cursor when packaged archive loading succeeds. |

This audit disproves earlier prompt metadata claiming 17 frames for
`main_32.slp`; live supplied file has 53. Executable and packaged PE evidence
recover visible control dispatch: Single Player `9500/31000`, Multiplayer
`9501/31001`, Learn to Play `9503/31003`, Map Editor `9504/31004`, History
`9505/31005`, Options `9506/31006`, and Exit `9509/31009`. Native image groups
begin at SLP frames 10, 14, 22, 26, 30, 34, and 46. Decompiled bounds are
preserved by `native_main_menu_controls()`; first-control bounds
`(532,9,192,258)` come from deterministic alpha-weighted registration of frame
10 against background frame 0 because decompilation omits that constructor's
arguments. Semantic hit-mask infrastructure rejects transparent pixels rather
than treating image bounds as rectangular controls.

Runtime wiring, exact font rendering, and meanings of group frames 2 and 3
remain unproved. Runtime therefore still uses English defaults, SDL debug
glyphs, measured rectangular hit regions, and pale borders. No pixel-parity
claim and BUG-FRONTEND-001 remains open.

## Runtime and fallback

`configured_asset_root()` only resolves reconstruction-local packaged
`game_data`, never parent research directories or an external environment
path. Valid `main_32.slp` plus palette 50589 enables original background art.
Missing, malformed, or truncated input leaves visibly non-equivalent
procedural plaques. Both paths preserve simulation state.

Main, Single Player, and Arabia-vs-AI size-menu focus use data-driven ordered
item tables. Mouse motion,
click, Up/Down wrapping, Enter/Space, Escape, and close-button input share one
command dispatcher. Unsupported Learn to Play, Regicide, and Death Match
remain legible but disabled. Zone only reports service unavailability.

`frontend_menu_tests` covers geometry, transforms, hit testing, wrapping,
routing, four Arabia size presets, disabled entries, and flyout close.
`legacy_assets_tests` covers
truncated SLP rejection. `frontend_menu_sdl_smoke` captures fallback and
packaged-original 800×600 states, seven focus states, and a letterboxed 16:9
window.

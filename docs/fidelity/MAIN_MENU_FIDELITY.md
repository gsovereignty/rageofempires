# Main-menu fidelity

## Scope and evidence

Archive-backed runtime now presents main screen in native 1366×768 design
space. Archive-absent fallback and Single Player flyout retain reconstruction's
800×600 layout.

Recovered `FUN_00603970` and `FUN_006042a0` in
`decompiled/AoK-HD-patched.c` prove focusable/visible controls and named screen
transitions. They do not prove pixel positions. `main.sin` proves palette
50589, cursor 51000, button 50688, popup 50090, and documented colors.

## Audited assets

`slp_contact_sheet` decoded every loose candidate with interface palette
50589. Contact sheets remain untracked under `artifacts/menu-contact-sheets/`.

| Source | Decoded frames | Dimensions and hotspots | Runtime role |
|---|---:|---|---|
| `Data/Slp/main_32.slp` | 53 | frame 0: 1366×768, hotspot 0,0; frames 1–9, 18–21, 38–45: 1×1; frames 10–13: 192×258; 14–17: 161×188; 22–25: 218×254; 26–29: 123×97; 30–33: 160×147; 34–37: 137×139; 46–48: 230×137; 49–52: 557×173 | Frame 0 supplies full Age of Kings background. Frames 10–48 supply native main-control states. Frame 49 supplies matching logo art for fallback composition only. |
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

`FUN_005c4200` writes normal image index at object offsets `0x610` and `0x300`;
`FUN_005c4180` writes rollover index at `0x630`; `FUN_005c41a0` switches live
index between those fields. Construction supplies consecutive normal and
rollover frames. Adjacent native button input paths select next pressed frame;
fourth frame is disabled presentation. Learn to Play is reachable and enabled
now that its guided scenario exists. Exit supplies only three frames, so
runtime does not invent a fourth image. `slp_contact_sheet` now reports per-frame opaque
counts; deterministic captures also prove equal-count state pairs differ in
their pixels.

Runtime draws archive normal, rollover/focus, pressed, and disabled images,
activates on matching press/release masks, and uses exact 1366×768 contain
scaling with inverse input mapping. Recovered string IDs and English meanings
remain in control contract.

Native labels now use recovered English/localized RT_STRING IDs, exact
`RGE_FONT_BUTTON1` family/height/style, `main.sin` colors and shadow, centered
alignment, and all seven text rectangles. Six rectangles are direct
`FUN_006042a0` decompiler constants; Single Player uses the constructor
rectangle recovered from original executable instructions at `0x00604a3e`-
`0x00604a62`. Exact-family lookup accepts the
packaged user-supplied face or a matching installed system face and never
substitutes an unrelated font. See
`docs/evidence/MAIN_MENU_TEXT_EVIDENCE.md`.

## Runtime and fallback

`configured_asset_root()` only resolves reconstruction-local packaged
`game_data`, never parent research directories or an external environment
path. Valid `main_32.slp` plus palette 50589 enables original background and
control-state art.
Missing, malformed, or truncated input leaves visibly non-equivalent
procedural plaques. Both paths preserve simulation state.

Main, Single Player, and Arabia-vs-AI size-menu focus use data-driven ordered
item tables. Mouse motion,
click, Up/Down wrapping, Enter/Space, Escape, and close-button input share one
command dispatcher. Learn to Play launches guided playable content. Regicide
and Death Match share random-map setup but apply distinct rules, units, ages,
technologies, and resources before simulation creation. Zone opens a dedicated
retired-service presentation with historical URL and supported alternative.
See `docs/evidence/FRONTEND_GAME_MODES_EVIDENCE.md`.

`frontend_menu_tests` covers geometry, transforms, hit testing, wrapping,
routing, four Arabia size presets, restored mode entries, and flyout close.
`legacy_assets_tests` covers
truncated SLP rejection. `frontend_menu_sdl_smoke` captures fallback states,
native normal/focused/pressed states at 1366×768, alpha-mask activation, and
native 1024×768 letterboxing, playable-mode launches, and Zone presentation.

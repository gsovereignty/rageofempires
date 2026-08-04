# Market standing runtime evidence

## Result

`BUG-VISUAL-002` is resolved. Completed Markets select their exact DAT root
graphic and remain byte-identical across the reported tick sequence.

The earlier `allied-trade-audit.scenario` interpretation did not isolate the
Market from the Trade Cart. The apparent rubble, stakes, tents, and towers
moved with the cart and were combinations of valid directional Trade Cart
frames with the large Market beneath them. They were not changing Market
frames.

## Original asset contract

Read-only `empires2_x1_p1.dat` records Feudal western Market graphic 2268
`MRKT2NNW` as:

- layer 20;
- one frame and one angle;
- frame rate 0, replay delay 0, sequence type 0;
- root SLP 2278;
- zero-offset deltas 2260, absent `-1`, and 2264.

Graphic 2260 is the one-frame layer-5 main component. Graphic 2264 is the
one-frame layer-20 annex component. Supplied SLP 2278 is the present,
one-frame complete root. Native selection therefore draws frame 0 of root
2268; it must not sequence delta identifiers as animation frames or expand
the already-complete root into duplicate parts.

The decompiled executable corpus was searched for the renderer's graphic and
layer handling. It contains no stronger symbolic Market-specific selector;
the live DAT fields and archive header provide the exact bounded contract.

## Reconstruction contract and regression

`canonical_building_composite_sets` now binds Market standing states by DAT
graphic root rather than by an untyped direct SLP. Its `complete_root` policy
loads only the selected root SLP and deliberately does not recursively expand
the baked delta graph. Feudal western selection resolves to graphic 2268,
not a tick-derived SLP frame.

`render_asset_coverage_tests` pins graphic 2268, composite selection,
`complete_root`, and absence of a direct-SLP request.

`market_standing_sdl_smoke` launches
`market-standing-regression.scenario` through the SDL dummy/software drivers
without foreground input. It captures isolated blue and red Feudal Markets at
ticks 0, 12, 20, 24, 40, and 64, then requires every BMP to be byte-identical.

Persistent background proof is under `artifacts/bug-visual-002/`. All six BMPs
have SHA-256:

`a8e4757daf43279fd3071cdff71b33941059b8ca1dbb6d10d105dbeb7e19a70a`

These artifacts are disposable local evidence and remain untracked.

# Building placement preview

The SDL battlefield now shows the pending building footprint under the mouse.
Clear placements use green diamonds; rejected placements use red diamonds and a
short reason. The deterministic preview checks terrain, footprint overlap,
builder range, resources, and uniform elevation before it submits a command.

`Esc` cancels placement. A successful click exits placement mode unless `Shift`
is held, which keeps the same building selected for repeat placement.

Walls and gates use right-button drag. The endpoints are converted into the
same Bresenham tile segment on every peer. Each tile is submitted as a typed
`ConstructBuildingCommand` through the normal command/replay path. There is no
speculative prepayment: each accepted segment atomically pays its own cost. The
operation stops at the first rejected tile and preserves the already accepted
prefix. Holding `Shift` keeps wall/gate placement active.

The footprint overlay is procedural. No archive entry has been identified as an
exact placement-ghost sprite, so the renderer deliberately does not guess an
asset mapping. An archive-backed translucent ghost can be enabled later when an
exact sprite identity is evidenced.

## Proof hooks

Set `AOE_BUILD_PREVIEW=valid` or `AOE_BUILD_PREVIEW=invalid` to open the demo
with a house preview on a known clear or occupied tile. Combine it with
`AOE_SCREENSHOT_PATH` and `AOE_EXIT_AFTER_SCREENSHOT=1` for deterministic
headless capture.

Pure coverage lives in `building_placement_tests`; SDL startup/render coverage
lives in `building_placement_sdl_smoke`.

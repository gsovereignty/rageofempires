# Building-contact layering evidence

## Original behavior and data

Read-only VER 5.7 DAT metadata maps Battering Ram attack graphic 680 to SLP
173, with DAT render layer 20. Dark-Age House standing graphic 2197 uses SLP
2223 and the same DAT layer 20. These are peer world objects; graphic layer
does not place the House unconditionally above the Ram.

Original executable decompilation was consulted read-only. In
`AoK-HD-patched.c`, `FUN_0053e2e0` (`diam_map_view::draw_objects`) traverses
world objects from shared map object lists and sends each through the common
draw path `FUN_0053d600`. Reconstruction's former per-diagonal, separate
building-then-unit passes did not preserve that shared contact ordering: a Ram
on the north contact tile was drawn on an earlier diagonal and then completely
overwritten by its target House.

## Reconstruction contract

Ordinary units retain tile diagonal depth. A unit actively striking an
adjacent building shares at least the target building's contact diagonal.
Buildings render first on that diagonal and the contacting striker renders
afterward. Distant attack orders and unit-target attacks keep their own depth,
preventing premature foreground promotion while pathing.

## Deterministic runtime proof

`building_contact_layering_sdl_smoke` runs
`ai-building-attack-audit.scenario` twice at tick 40 with dummy video/audio and
software rendering. Frames must be byte-identical. Overlap capture proves the
Ram uses attack SLP 173 plus present composite child SLP 171. Every one of more
than 7,000 opaque isolated Ram pixels below the original 32-pixel-high top HUD
chrome must remain byte-identical in the final composed frame, proving the
target House no longer erases the world-visible strike. Pixels beneath that
opaque HUD chrome are intentionally excluded because HUD composition follows
world-object rendering and correctly replaces them.

Local, untracked audit output lives under `artifacts/bug-visual-005/`.

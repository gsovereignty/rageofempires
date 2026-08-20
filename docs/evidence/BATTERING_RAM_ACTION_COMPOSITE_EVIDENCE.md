# Battering Ram action-composite evidence

## Original data

Read-only VER 5.7 `empires2_x1_p1.dat` links Battering Ram unit 35 to
standing graphic 686 and walking graphic 690. Graphic 686 uses root SLP 179
and delta graphics 684/685. Graphic 690 uses root SLP 183 and delta graphics
688/689. Present walking child graphic 688 supplies SLP 181; optional sibling
SLPs 177, 178, and 182 are absent from both supplied 1999 and HD
`graphics.drs` archives. Original executable decompilation was consulted
read-only. In `AoK-HD-patched.c`, `FUN_004eaf00` at `0x004eaf00` passes a
graphic record to `FUN_00510830`; `FUN_00510830` selects static or animated
graphic-instance creation (`FUN_004eb870` for animated records). This supports
preserving DAT graphic composition and cadence rather than treating root SLP
identifiers as complete unit bodies.

Archive headers prove SLP 179 has 5 physical frames and SLP 183 has 75 (15
frames across 5 stored directions). Drawing only SLP 183 therefore omits
present SLP 181 child body and produces tiny, low-anchored moving artwork.

## Reconstruction contract

`UnitActionCompositeSet` records native DAT roots. Loader expands each root,
skips only archive-absent optional delta sprites, decodes every present SLP,
and renders parts through shared hotspot-ground anchoring. Battering Ram idle
uses root 686; movement uses root 690; attack uses root 680; death uses root
683. No bitmap or original archive is tracked.

## Deterministic runtime proof

`battering_ram_composite_sdl_smoke` runs
`movement-gait-audit.scenario` twice per checkpoint using dummy video/audio and
software rendering. Overlap manifests prove idle draws SLP 179 while movement
draws SLPs 183 and 181. For logical east direction 0, DAT mirroring mode 6
selects stored direction 2 and horizontally mirrors idle frame 2, moving root
frame 30, and moving child frame 2. At 1.25x capture zoom, both isolated
composites are exactly 120x80 pixels at the same `(105,101)` capture anchor.
Repeated full frames are byte-identical.

Local audit output lives under `artifacts/bug-visual-004/` and is untracked.

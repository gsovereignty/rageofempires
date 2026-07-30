# Projectile and impact visual fidelity

## Evidence and guarded catalog

Live VER 5.7 DAT/DRS audits in `../assets/STANDARD_UNITS_ASSET_MAP.md`,
`../assets/LAND_UNIQUE_ASSET_MAP.md`, `../assets/NAVAL_ASSET_MAP.md`, and
`../assets/BUILDING_SPRITE_MAP.md` prove represented projectile roots, neutral player
color, animation metadata, direct layer-10 shadow children, and impact links.

`projectile_catalog` accepts those records only when exact graphic ID, SLP,
layer, frame count, angle count, mirroring mode, neutral player color, shadow
link, and impact metadata agree. Any DAT drift, missing record, wrong layer, or
missing link fails closed.

## Renderer coverage

Before this pass, Fire Ship flame was the only fully exact represented flight
path. Cannonballs and gunpowder shots omitted their DAT shadows. Onager and
Trebuchet animations displayed frame zero only; Onager primary shots used the
volley art. Throwing axes omitted their matching animated shadow. Arrow and
Scorpion multi-direction SLPs were displayed as a single frame regardless of
direction. Both impact animations were frozen on frame zero.

Current exact paths:

- Fire Ship stream: graphic 3822 / SLP 4193;
- cannonball: 3382 / 3803 with shadow 3383 / 3804;
- gunpowder and Onager primary shot: 3396 / 4500 with shadow 3397 / 3818;
- Onager volley: 3385 / 3986 with shadow 3386 / 3807;
- Trebuchet stone: 3394 / 3815 with shadow 3395 / 3816;
- throwing axe: 3380 / 3801 with shadow 3381 / 3802;
- standard impact: 1744 / 416, all 10 frames;
- fire/gunpowder impact: 5463 / 4370, all 20 frames.

Onager lane zero uses primary art; additional volley lanes use volley art.
Impact routing covers represented Scorpion/Onager/bombard/cannon naval sources
for 416 and Fire Ship/hand-cannon/Janissary/Conquistador sources for 4370.

Pinned openage commit `9a5a7ccbfc20c2de658fc746462cd4a69aa758ef`
proves clockwise logical angles, front vector `(-1,+1)`, nearest-angle
selection, horizontal mirroring above 180 degrees, and
`frame + angle * frames_per_angle` layout. Exact installer SLP headers prove
Scorpion body 3812 and shadow 3813 each contain 10 physical frames, matching
the 18-direction half-plus-center layout. Scorpion now selects body and shadow
independently from the same physical angle, applies DAT offsets, transforms
mirrored draw hotspots with `width - hotspot_x`, and flips both layers.

Arrow uses DAT graphic 638 / SLP 50 (`ARROW_NN`): one frame over 72 logical
directions. Its 37 physical frames exactly match the mirrored
half-plus-center layout. Runtime selects the nearest logical direction and
mirrors directions above 180 degrees. Expansion graphic 3378 / SLP 3799
remains unsuitable because its 176 frames do not match its declared
11-by-32 layout; renderer deliberately selects the complete static arrow.

Machine-readable before/after inventory lives in
`generated/projectile_impact_coverage.json`; regenerate with:

```sh
python3 tools/generate_projectile_coverage.py
```

`tools/audit_projectile_directions.py` joins pinned source evidence to exact
installer SLP counts, payload hashes, and hotspot bounds in
`generated/projectile_direction_evidence.json`.

## Tests and remaining validation

`projectile_catalog_tests` covers exact body/shadow/impact resolution, wrong
shadow-layer rejection, impact-cadence drift rejection, Scorpion front/back/
mirrored selection, and static Arrow direction selection.

Paired original-runtime captures remain needed for projectile hotspot, arc,
shadow offset, action cadence, and impact timing.

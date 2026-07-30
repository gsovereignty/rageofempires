# Building construction, damage, and death body fidelity

## Exact audit

`generated/building_body_state_catalog.json` records every represented
`BuildingKind`, its classic unit ID, every civilization-specific state family,
and recursive standing/construction/damage/death DAT chains.

Promotion requires:

- exact state root from each civilization's unit record;
- every recursive graphic and positive SLP ID present;
- stable DAT layer ordering and accumulated x/y offsets;
- archive player-index pixels decoded for each owner;
- compatible frame counts across composite layers;
- construction progress mapped monotonically across exact frames.

Focused audit against the installer DAT and `graphics.drs` proves every promoted
construction and death family satisfies those requirements.

## Renderer coverage

- Construction body composition: 0/27 before, 4/27 after.
- Death body composition: 3/27 before, 25/27 after.
- Exact damage-state application: 0/27 before, 26/27 after across 103
  complete civilization families.

Generic construction roots 118, 119, 120, 121, 123, and 4248 resolve only
layer-10 art. A visual capture proved treating those roots as complete bodies
produces a large opaque player-colored rectangle. They are now explicitly
classified as shadow-only/incomplete and never promoted as body compositions.
Fish Trap 5441/SLP 4585, civilization-specific Stone Wall construction, and
both civilization-specific Palisade Gate axes are complete exact paths.
Remaining construction bodies retain procedural fallback.

Death now uses exact animated composite roots 37, 38, 39, 40, 42, and 5452.
All recursive layers render in DAT order with exact offsets and owner color.

## Classified fallback

- Farm has no construction or death root.
- Stone Wall construction uses its five proved civilization-family roots.
- Both Palisade Gate axes use their five proved civilization-family roots.
- Fish Trap has no death root.
- Damage uses exact recursive DAT/DRS animated composites. Runtime selection
  computes `100-floor(hp*100/maxhp)` and chooses last record with threshold
  strictly below damage. Flag 0 records overlay standing/construction art;
  Stone Wall flag 2 records replace standing art. Record changes reset
  animation cadence; repair reverses selection; death removes damage display.
  No flag 1 record exists in represented catalog, so randomized placement is
  neither inferred nor rendered.
- Fish Trap damage roots `5357..5359` have no drawable SLP or delta layer.
  They fail closed to no exact damage overlay.

Missing/malformed DAT, DRS, palette, SLP, incompatible layer cadence, or empty
compositions fail closed to existing procedural rendering.

## Reproduction

```sh
python3 tools/audit_building_body_states.py \
  /path/to/empires2_x1_p1.metadata.json \
  /path/to/Data/graphics.drs
python3 tools/test_audit_building_body_states.py
```

Machine report includes exact roots, SLP IDs, layers, offsets, frame counts,
angles, palette IDs, and DAT cadence fields for every civilization family.

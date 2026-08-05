# Procedural building DAT/DRS asset map

`generated/procedural_building_dat_metadata.json` joins live VER 5.7 unit
records, recursive DAT graphic deltas, and physical `graphics.drs` SLP headers
for represented buildings that currently use procedural drawing.

## Civilization families

Family suffixes used below:

- W: British, French, Celts, Spanish
- E: Goths, Germans, Vikings, Huns
- F: Japanese, Chinese, Mongols, Koreans
- M: Byzantine, Persians, Saracens, Turks
- X: Aztecs, Mayan

| Kind | Standing graphic by family | Construction by family | Death | Damage graphics |
|---|---|---|---|---|
| Farm 50 | G 255 | none | none | 5354/5355/5356 |
| Fish Trap 199 | G 3281 | 5441 | none | 5357/5358/5359 |
| Monastery 104 | W150, E147, F148, M149, X6734 | 120 | 39 | W 4774/4778/4782; E 4771/4775/4779; F 4772/4776/4780; M 4773/4777/4781; X 6728/6729/6730 |
| Palisade Wall 72 | G 588 | 118 | 37 | 4610/4611/4612 |
| Stone Wall 117 | W2024, E2021, F2022, M2023, X7096 | W3321, E3318, F3319, M3320, X7107 | 37 | W 3794/3798/3802; E 3791/3795/3799; F 3792/3796/3800; M 3793/3797/3801; X 7150/7152/7154 |
| Repository `palisade_gate_x`/unit 792 | 6512 | W3289, E3286, F3287, M3288, X6798 | 38 | 6509/6510/6511 |
| Repository `palisade_gate_y`/unit 796 | 6533 | W3305, E3302, F3303, M3304, X6830 | 38 | 6530/6531/6532 |
| Repository `stone_gate_x`/unit 789 | 6497 | 118 | 38 | 6492/6493/6494 |
| Repository `stone_gate_y`/unit 793 | 6518 | 118 | 38 | 6513/6514/6515 |

Ordinary raw damage thresholds are 25/50/75 with flag 0. Stone Wall records
store raw 537/562/587 with flag 2; low bytes are 25/50/75. Preserve raw values
until runtime semantics for flag 2 are proved.

## Archive coverage and complete `NN` roots

- Farm root `FARM0NNG` graphic 255 points to SLP 419, absent. Its component
  SLPs 417/418 are also absent. Damage graphics have no SLP. Farm remains a
  guaranteed procedural case for this archive.
- Fish Trap standing root 3281/SLP 3593 and construction root 5441/SLP 4585
  are complete and render exactly. Its DAT record has no dying root. Damage
  roots 5357–5359 contain neither an SLP nor drawable delta layer. Procedural
  damage/rubble is therefore reviewed and explicit; no replacement ID is
  inferred.
- Monastery complete `NN` roots 147–150 and 6734 are present as SLPs
  278–281/4953. Construction 120/SLP 238 and death 39/SLP 75 are present.
  All mapped damage SLPs are absent. Some delta component SLPs are also absent;
  root presence must not be treated as proof that every composite layer exists.
- Palisade Wall complete root 588/SLP 4534, construction 118/236, and death
  37/73 are present. Its damage graphics have no SLP.
- Stone Wall complete roots 2021–2024/2098–2101 and 7096/5124 are present.
  All family construction, death, and damage SLPs are present. This is the
  only mapped group with a complete standing/construction/damage/death chain.
- Gate roots 6497/4878 and 6518/4889 are present. Composite roots 6512 and
  6533 have no SLP of their own; they require delta composition. Their N1
  component SLPs 4877 and 4888 are absent. Gate construction and death SLPs
  are present; all gate damage graphics have no SLP.

Packet-10 re-audit on 2026-07-29 selected the player-visible Palisade Gate
family. The live archive still lacks required N1 components 4877 and 4888, so
no visually similar art was substituted. Reachable exact roots remain active;
the missing component/damage states deliberately retain procedural rendering.
`generated/renderer_asset_coverage.json` was refreshed after the audit.

Other absent SLP IDs in recursive dependencies are
270, 271, 274–277, 2219, 2220, 2260, 2263, 4951, 4952, and 5156.
Fixture records every graphic, delta offset, presence bit, DAT frame count,
and every SLP frame's dimensions/hotspot.

## Gate direction contract

DAT graphic names encode two axes:

- X-kind mappings use A roots: `SGAX1NN`/`SGAA1NN`.
- Y-kind mappings use B roots: `SGBX1NN`/`SGBA1NN`.

Composite A root 6512 uses side offsets `(72,-36)` and `(-72,36)`.
Composite B root 6533 uses `(-72,-36)` and `(72,36)`. These signs establish
the two isometric diagonals. Do not obtain Y by horizontally flipping A:
each axis has distinct root, damage, and construction graphics.

Unit IDs 789–804 are a family of gate state/orientation records. DAT fields
prove IDs, costs, HP, graphics, and offsets, but names in this fixture do not
prove repository material labels. Keep current material-to-unit selection
separate from orientation mapping until scenario/runtime comparison confirms
it.

## Safe renderer contract

1. Choose civilization family first, then state:
   construction, standing, 25/50/75 damage, or death.
2. Prefer exact complete `NN` standing graphic roots when their SLP exists.
   Expand DAT deltas when fidelity requires layers; tolerate only explicitly
   absent optional layers.
3. Use SLP frame hotspots verbatim. DAT `frame_count × angle_count` matches
   physical SLP frame count for present assets; do not center by texture size.
4. Wall angle index selects connectivity shape. Gate X/A and Y/B use separate
   graphics and offsets, not rotation or flip synthesis.
5. Missing Fish Trap death/damage, building damage, or gate component SLPs require documented procedural
   fallback. Never substitute an unrelated SLP sharing a graphic role.
6. Generic `CNST*` roots are footprint scaffold/shadow art. Reveal selected
   completed DAT body bottom-up by authoritative construction progress, then
   draw scaffold; never stretch scaffold into a body.
7. Exact construction progress, damage-threshold edge, gate open-state record,
   and delta draw order remain runtime-validation boundaries.

## Regeneration

```sh
tools/dat_metadata/target/release/aoe-dat-metadata \
  /path/to/empires2_x1_p1.dat > /tmp/aoe-full-dat.json
python3 tools/dat_metadata/generate_procedural_building_metadata.py \
  /tmp/aoe-full-dat.json /path/to/graphics.drs
```

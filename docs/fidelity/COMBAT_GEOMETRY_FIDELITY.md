# Combat geometry fidelity

## Evidence boundary

Pinned live `empires2_x1_p1.dat` exposes numeric fields named minimum/maximum
weapon range, area-effect range, offense blast level, static area-effect
level, missed-missile spread, weapon offset, and volley count/spread. It also
links projectile unit records, whose raw missile flags and ballistics ratio
are preserved in
`generated/combat_geometry_dat_metadata.json`.

These names and values are DAT evidence. They do **not** prove runtime
geometry or interpretation. Original-runtime validation remains required for:

- distance metric, impact origin, circle/ellipse/other shape, and footprint
  interaction;
- constant damage versus distance falloff (the pinned parser exposes no field
  explicitly named as a damage-falloff curve or coefficient);
- meanings of numeric `area_effect_level`, `blast_level_offense`, and missile
  flags, including target classes and friendly fire;
- miss sampling, volley placement, collision, terrain, elevation, flight, and
  impact behavior.

Absence of an explicitly named falloff field in this pinned schema is not
proof that original runtime applies no falloff.

## Semantic evidence

Evidence grades below use only:

1. the [official *The Conquerors* manual scan](https://manuals.plus/m/dfcc44241b836c6ff27a5847cc3143380251208c49d8efc417d8ba5624469bb0.pdf),
   published by Microsoft/Ensemble Studios in 2000; and
2. official World's Edge
   [Update 51737 modding notes](https://www.ageofempires.com/news/aoeiide-update-51737/)
   and
   [Update 61321 modding notes](https://www.ageofempires.com/news/age-of-empires-ii-definitive-edition-update-61321/).

The original manual says Mangonels, Onagers, and Siege Onagers avoid
auto-attacking when they would harm friendly units. This establishes that
friendly harm is possible for those three records; it does not specify exact
diplomacy/target-class filtering. The manual also says bolts damage everything
they touch, but does not identify friendly-fire filtering or a radius origin
for Scorpions.

DE Update 51737 documents newer flags for fixed/percentage blast styles and a
distance-reduction flag that works only for melee units. Update 61321
documents attack dispersion and a directed-blast flag. These are official
descriptions of a later DE runtime, not proof that pinned HD/AoC records use
those semantics. They therefore constrain terminology but do not upgrade any
AoC result below.

`possible (manual)` means only that official manual establishes possible
friendly harm. `unknown` means evidence does not support a semantic claim.

| Represented record | Friendly-fire target filtering | Uniform or falloff | Radius origin |
|---|---|---|---|
| Bombard Cannon | unknown | unknown | unknown |
| Trebuchet, unpacked | unknown | unknown | unknown |
| Bombard Tower | unknown | unknown | unknown |
| Scorpion | unknown | unknown | unknown |
| Mangonel | possible (manual); exact filter unknown | unknown | unknown |
| Trebuchet, packed | unknown | unknown | unknown |
| Cannon Galleon | unknown | unknown | unknown |
| Petard | unknown | unknown | unknown |
| Demolition Ship | unknown | unknown | unknown |
| Heavy Demolition Ship | unknown | unknown | unknown |
| Heavy Scorpion | unknown | unknown | unknown |
| Onager | possible (manual); exact filter unknown | unknown | unknown |
| Siege Onager | possible (manual); exact filter unknown | unknown | unknown |
| Elite Cannon Galleon | unknown | unknown | unknown |

No represented unit has adequate source evidence here for uniform damage,
distance falloff, or radius origin. No behavior change is recommended.
Preserve current implementation until an original AoC/HD executable experiment
records attacker/target coordinates, ownership/diplomacy, footprints, HP
before/after, elevation, and impact point for controlled shots.

## Bounded projectile travel contract

Projectile flight duration uses Euclidean distance from the attacker or the
nearest building-footprint border. The calculation uses squared integers and
rounds upward to the first whole simulation tick, with a minimum of one tick.
Nonzero DAT projectile speed is interpreted as tenths of a tile per second at
five simulation ticks per second. Records without a represented speed retain
the earlier effective cardinal speed of ten tiles per second.

This mapping is deterministic across save/replay and makes equal-length
cardinal and diagonal flights take equal time. The DAT proves raw speed values,
not their original-runtime units or rounding, so this remains an explicit
bounded reconstruction contract. Ballistic tracking and impact behavior are
unchanged.

## Bounded building line-of-sight contract

Building line of sight uses circular squared distance from the nearest tile of
the building footprint, matching unit line-of-sight geometry. Building vision
upgrades continue to modify the DAT-derived numeric range before the geometry
test.

The DAT supports the line-of-sight values, but does not establish the original
runtime's footprint-origin or distance convention. Nearest-footprint radial
visibility is therefore an explicit bounded reconstruction contract.
Cartography/team sharing and persistent explored fog retain their existing
behavior.

## Manual-backed mangonel automatic-target safety

Mangonel, Onager, and Siege Onager automatic acquisition skips an enemy impact
point when any living, ungarrisoned friendly or allied unit, or any living
friendly or allied building footprint, lies within that unit's represented
splash radius. Candidate iteration and nearest-target tie behavior remain
unchanged; unsafe candidates are filtered before ranking, so a safe alternate
can still be selected.

This filter applies to automatic aggressive, stand-ground, guard, and
attack-move acquisition through their shared target-acquisition path. Explicit
player attack and attack-ground orders bypass it and retain existing friendly
splash damage. Diplomacy is evaluated when acquisition occurs.

## Live raw values

`min-max` is raw weapon range. `area` is raw area-effect range. `level/blast`
are uninterpreted numeric mode fields.

| Represented record | DAT ID | min-max | area | level/blast | missed spread | volley spread |
|---|---:|---:|---:|---:|---:|---:|
| Bombard Cannon | 36 | 5-12 | 0.5 | 3/2 | 0 | 1, 1 |
| Trebuchet, unpacked | 42 | 4-16 | 0 | 3/1 | 0.2 | 1, 1 |
| Bombard Tower | 236 | 1-8 | 0 | 2/2 | 0 | 0, 0 |
| Scorpion | 279 | 2-7 | 0 | 3/0 | 0 | 1, 1 |
| Mangonel | 280 | 3-7 | 1 | 3/2 | 0 | 1, 1 |
| Trebuchet, packed | 331 | 4-16 | 0 | 3/0 | 0 | 1, 1 |
| Cannon Galleon | 420 | 3-13 | 0 | 3/2 | 0.1 | 0, 0 |
| Petard | 440 | 0-0 | 0.5 | 3/2 | 0 | 0, 0 |
| Demolition Ship | 527 | 0-0 | 2.5 | 3/2 | 0 | 0, 0 |
| Heavy Demolition Ship | 528 | 0-0 | 3.5 | 3/2 | 0 | 0, 0 |
| Heavy Scorpion | 542 | 2-7 | 0 | 3/0 | 0 | 1, 1 |
| Onager | 550 | 3-8 | 1.25 | 3/2 | 0 | 1.25, 1.25 |
| Siege Onager | 588 | 3-8 | 1.5 | 3/1 | 0 | 1.5, 1.5 |
| Elite Cannon Galleon | 691 | 3-15 | 0 | 3/2 | 0.1 | 0, 0 |

Packed Trebuchet retains range fields but has no weapon entries or primary
projectile. This proves record contents, not attack availability.

Mangonel-line records also carry raw volley counts `6`, `8`, and `10`, volley
projectile `369`, and start-spread adjustment `99`. Those fields support
metadata fidelity only; they do not define placement geometry by themselves.
Projectile records `367`, `368`, `369`, `371`, `374`, `506`, `627`, and `656`
are included so simulation work need not infer projectile flags from attacker
records.

## Reproduction

```sh
cargo run --quiet \
  --manifest-path tools/dat_metadata/Cargo.toml \
  -- /path/to/empires2_x1_p1.dat > /tmp/aoe-metadata.json
python3 tools/dat_metadata/generate_combat_geometry_metadata.py \
  /tmp/aoe-metadata.json
```

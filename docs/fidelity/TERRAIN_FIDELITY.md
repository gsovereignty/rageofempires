# Terrain fidelity

## Live DAT and executable evidence

Evidence was read from supplied `empires2_x1_p1.dat` through
`tools/dat_metadata`. DAT declares 41 enabled terrains (IDs `0..40`) and 22
terrain-restriction rows. Reconstruction keeps every enabled ID distinct:

| ID | DAT name | Native token | SLP |
|---:|---|---|---:|
| 0 | Grass | `grass` | 15001 |
| 1 | Water | `water` | 15002 |
| 2 | Beach | `beach` | 15017 |
| 3 | Dirt 3 | `dirt3` | 15007 |
| 4 | Shallows | `shallows` | 15014 |
| 5 | Leaves | `leaves` | none |
| 6 | Dirt | `dirt` | 15000 |
| 7 | Farm1 | `farm1` | 15004 |
| 8 | Farm2 | `farm2` | 15005 |
| 9 | Grass 3 | `grass3` | 15009 |
| 10 | Forest | `classic_forest` | 15011 |
| 11 | Dirt 2 | `dirt2` | 15006 |
| 12 | Grass 2 | `grass2` | 15008 |
| 13 | Palm Desert | `palm_desert` | 15010 |
| 14 | Desert | `desert` | none |
| 15 | Old Water | `old_water` | none |
| 16 | Old Grass | `old_grass` | none |
| 17 | Jungle | `jungle` | none |
| 18 | Bamboo | `bamboo` | none |
| 19 | Pine Forest | `pine_forest_floor` | none |
| 20 | Oak Forest | `oak_forest_floor` | none |
| 21 | Snow Forest | `snow_forest` | 15029 |
| 22 | Water 2 | `deep_water` | 15015 |
| 23 | Water 3 | `medium_water` | 15016 |
| 24 | Road | `road` | 15018 |
| 25 | Road 2 | `road2` | 15019 |
| 26 | Ice | `ice` | 15024 |
| 27 | Foundation | `foundation` | 15006 |
| 28 | Water Bridge | `water_bridge` | none |
| 29 | Farm Cnst1 | `farm_construction1` | 15021 |
| 30 | Farm Cnst2 | `farm_construction2` | 15022 |
| 31 | Farm Cnst3 | `farm_construction3` | 15023 |
| 32 | Snow | `snow` | 15026 |
| 33 | Snow Dirt | `snow_dirt` | 15027 |
| 34 | Snow Grass | `snow_grass` | 15028 |
| 35 | Ice 2 | `ice2` | none |
| 36 | Snow Foundati | `snow_foundation` | none |
| 37 | Ice Beach | `ice_beach` | none |
| 38 | Snow Road | `snow_road` | 15030 |
| 39 | Snow Road 2 | `snow_road2` | 15031 |
| 40 | KOH | `koh` | 15018 |

Relevant restriction rows contain only exact `0.0` and `1.0` values. Positive
IDs are:

| Restriction | Represented use | Positive terrain IDs |
|---:|---|---|
| 3 | ships | 1, 2, 4, 15, 22, 23, 26, 37 |
| 4 | generic buildings | 0, 3, 5-14, 16-21, 24-25, 27, 29-34, 36, 38-39 |
| 6 | dock | 1, 2, 4, 15, 22, 23, 26, 35, 37 |
| 7 | workers and land units | every ID except 1, 15, 22, 23 |
| 13 | fish trap | 1, 2, 4, 15, 22, 23, 26, 37 |

Unit records independently connect Fishing Ship `13` and Fish Trap `199` to
restriction `13`, Villager `83` to restriction `7`, Galley `539` to
restriction `3`, and Dock `45` to restriction `6`.

Read-only decompiled `AoK-HD-patched.c` `FUN_005607f0` loads restriction and
terrain counts followed by complete float rows. `FUN_00569de0` classifies two
rows as equivalent only when each corresponding entry has the same positive
versus nonpositive class. This proves sign-based passability for these binary
rows; no distinct movement cost exists in the five represented production
rows.

## Reconstruction contract

Classic scenario import maps all enabled IDs one-to-one. Native scenario and
save formats preserve those identities through their text tokens and versioned
numeric encoding. No classic floor terrain aliases a reconstruction resource
overlay: for example, classic Forest `10` remains walkable floor rather than
becoming harvestable `Terrain::forest`.

Land movement, ship movement, generic building placement, Fish Trap placement,
and Dock placement use the exact positive sets above. Dock `45` has a fixed
3x3 restriction-`6` foundation. Its center cell must be Water `1` or Shallows
`4`, and at least one non-diagonal cell on the one-tile perimeter must be Beach
`2`, Ice2 `35`, or the original executable's Ice `26`/Ice Beach `37` aliases.

These rules apply through unit creation, command validation, pathfinding,
formation placement, movement updates, ship production/trade destinations,
building construction, save/load, scenario parsing, and deterministic replay.
Numeric positive restriction values are allowed; zero and negative values are
blocked. Current represented production rows are binary, so this does not
discard a distinct movement-cost value.

Renderer first uses optional loose Grass, Water, Beach, and Shallows PNGs.
Missing loose textures fall back individually to SLPs 15001, 15002, 15017,
and 15014 from `Data/terrain.drs`, decoded with palette 50500 from
`Data/interfac.drs`. Missing or invalid user-owned assets retain procedural
fallback; archive contents are read in place and never bundled. Other distinct
classic IDs use distinct procedural fallback colors rather than an incorrect
Grass or Water texture. Exact rendering for those IDs is outside this
passability/import contract.

Proved classic transitions are documented in
`TERRAIN_TRANSITION_FIDELITY.md`. Exact executable evidence resolves
one-cardinal variants with `(destination_x + destination_y) & 3`; calls
lacking destination position retain unblended fallback.

Live Dock `45` exposes radius and construction radius `(1.5, 1.5)`, center tile
requirements `(1, 4)`, perimeter tile requirements `(2, 35)`, terrain
restriction `6`, obstruction type `2`, and selection shape `3`. Decompiled
`FUN_00577ac0` applies those fields in that order: center requirement, four
non-diagonal perimeter sides, then restriction-`6` footprint cells. When the
perimeter requirement is Ice2 `35`, it also accepts Ice `26` and Ice Beach
`37`.

There is no completed-Dock orientation contract to implement. Standing graphic
`215` and its child graphics `214`, `216`, and `4411` each have one angle.
Placement validation returns no rotation, and the 3x3 collision rectangle is
fixed. Coast side therefore constrains placement and exit availability; it
does not rotate the sprite or footprint.

Produced ships and Trade Cogs search only the producer rectangle's four
non-diagonal perimeter sides. Decompiled `FUN_0057fc90` tests each candidate
through the produced object's own placement validator `FUN_0057d5b0`, retains
the closest valid cell, and uses west/north/south/east tie priority from the
centered start. Rally orders are applied after creation, so they route from
that valid ship cell rather than selecting a diagonal corner.

## Reproduction

```sh
cargo run --quiet --release \
  --manifest-path tools/dat_metadata/Cargo.toml -- \
  /path/to/empires2_x1_p1.dat

cmake --build build --target aoe_legacy_scenario_tests
./build/aoe_legacy_scenario_tests
```

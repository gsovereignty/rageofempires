# Tower damage presentation evidence

This contract binds Watch Tower, Guard Tower, and Keep damage presentation to
their effective researched identity. Presentation-only tier state is forbidden:
standing bodies and damage attachments derive from persisted `BuildingKind`,
civilization, owner, HP, and maximum HP after construction, repair, save/load,
and replay command playback.

## Original evidence

Read-only VER 5.7 DAT extraction identifies units 79, 234, and 235. Each has
three flag-0 damage records at effective thresholds 25, 50, and 75. Family
order below is European, Central European, East Asian, Middle Eastern, and
Mesoamerican:

| Tier | Unit | 25/50/75 roots by family |
|---|---:|---|
| Watch Tower | 79 | `5198/5202/5206`, `5195/5199/5203`, `5196/5200/5204`, `5197/5201/5205`, `7110/7111/7112` |
| Guard Tower | 234 | `5214/5218/5222`, `5211/5215/5219`, `5212/5216/5220`, `5213/5217/5221`, `7117/7118/7119` |
| Keep | 235 | `5230/5234/5238`, `5227/5231/5235`, `5228/5232/5236`, `5229/5233/5237`, `7125/7126/7127` |

`FUN_00589490` at `0x00589490` reads only low threshold byte and chooses last
record whose threshold is strictly below computed damage. It attaches flag-0
graphics through `FUN_004eaf00` and removes old attachment through
`FUN_004eafc0`, proving 25/50/75 equality behavior and repair reversal.

Each upgraded root retains its DAT delta offsets, layer order, cadence, and
palette dispatch. Runtime loads all `BuildingKind` damage roots, including
upgrade identities declared after `wonder`, then chooses player-colored child
parts with existing owner-slot palette routing.

## Regression proof

- `building_damage_tests` checks exact roots, flags, and thresholds for all
  five architecture families and all three tiers.
- `render_asset_coverage_tests` checks all 19 runtime civilization IDs, three
  effective tiers, pristine plus three damage stages, and every owner palette
  slot. Alias resolution must use `upgrade_variant`, never Watch Tower roots.
- `tower_damage_state_sdl_smoke` performs 378 dummy-SDL captures: 18 named
  civilizations, three researched tiers, and 0/25/26/50/51/75/76 damage.
  Equality pairs prove strict threshold direction; damaged captures prove
  actual DAT attachments render over each tier body at deterministic cadence.

No damage presentation fields are serialized. Existing save/load and replay
tests preserve authoritative tier identity, civilization, owner, and HP;
renderer re-derives same body and attachment after restore or playback.

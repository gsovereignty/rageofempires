# Civilization technology-tree fidelity

## Player-facing graph

`build_technology_tree` now emits one stable player-facing graph for each of
the 18 playable civilizations. Rows are producer families and columns are the
four Ages. The graph includes trainable units, their explicit upgrade chains,
clickable research, research upgrade chains, and player-facing buildings.
Civilization DAT availability only changes node state; it does not erase the
shared tree or invent nodes from another civilization.

Neutral ecology (`sheep`, `deer`, `boar`), Relics, Kings, packed Trebuchets,
orientation duplicates, and internal make-available technologies are absent.
Unavailable Paladin, gunpowder, naval, siege, monastery, economy, defensive,
unique-unit, and unique-technology tiers remain visible only where they are
genuine tree exclusions. `generated/civ_tech_tree_matrix.json` and
`civilization_has_*` provide the per-civilization availability truth table.

## Original evidence

- Supplied `TC Tech Tree.pdf` contains 18 pages, one for each playable
  civilization. Pages use producer rows, Age progression, connected unit and
  research tiers, and shaded exclusions. They contain no Gaia ecology or
  internal availability records.
- `AoK-HD-patched.c`, `FUN_004bf8d0`, constructs `One Button Tech Tree Screen`.
  It loads `btntech.shp` resource 50729, `ico_unit.shp` 50730,
  civilization-selected `ico_bld%d.shp` 50706-family artwork, `arrows.slp`
  53004, `technodex.slp` 53206, `techback.slp` 50341, `techages.slp` 50342,
  `ttx.slp` 53211, and `tech_tile.slp` 50343. Recovered node dimensions are
  36 by 36 pixels.
- Font dispatch selects `RGE_FONT_TECH_TREE_NODE` slot 200. Supplied English
  `language.dll` RT_STRING IDs 200 through 202 resolve to Georgia, 9, bold.
- DAT civilization tech-tree effects and type-102 disables are documented in
  [CIV_TECH_TREE_MATRIX.md](../assets/CIV_TECH_TREE_MATRIX.md).

No proprietary bytes are tracked. When user-owned archives are configured,
the screen uses exact technology, unit, building, background, Age-strip, and
node-tile resources. Missing optional archive art falls back to deterministic
drawn chrome. Georgia 9 bold follows the existing user-owned font contract.

## Verification

`aoe_technology_tree_tests` checks all 18 civilizations, stable node identity,
forbidden-record absence, producer-family membership, availability shading,
dependency bounds, explicit unit/research upgrade edges, and spatial
navigation. `aoe_technology_tree_sdl_smoke` captures localized background
screens and injects civilization/arrow key events through SDL's queue, proving
input changes rendered output under the dummy video driver.

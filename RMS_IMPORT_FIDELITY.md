# Classic RMS parsing and native placement

This module parses a strict subset of classic *Age of Conquerors* random-map
scripts (`.rms`) and evaluates accepted scripts into reconstruction-native
Scenario66 maps. Accepted generation sections drive native tile and entity
placement directly. It does not claim byte-for-byte compatibility with the
commercial generator.

## Pinned grammar evidence

Parser structure and token shapes were checked against Mangudai 0.3.0,
commit
[`e06ed3474d23168822f02b04108b938a1fb7feb9`](https://github.com/mangudai/mangudai/tree/e06ed3474d23168822f02b04108b938a1fb7feb9).
Its pinned
[`grammar.ne`](https://github.com/mangudai/mangudai/blob/e06ed3474d23168822f02b04108b938a1fb7feb9/src/parse/grammar.ne)
documents sections, command bodies, constants/includes, conditionals, nested
block comments, and `start_random`/`percent_chance`/`end_random`.

Common command vocabulary and block layout were cross-checked against archived
AoE2 scripts, including the visible `Asia Peninsula 2.rms` example:
<https://gist.github.com/lachlanmcdonald/4c9534809bbf83a5b3fba2396d405abe>.
The seven classic generation sections match the documented ordering in the
open `aoe2-rms-lib` reference:
<https://docs.racket-lang.org/aoe2-rms/sections.html>.

## Accepted reconstruction subset

- Sections: player, land, elevation, cliff, terrain, connection, objects.
- Player placement and map-size override. `override_map_size` is a tile
  count, so it snaps up to the nearest `RandomMapSize` preset
  (120/144/168/200/220/240/255) and clamps above 255. A script with no
  `override_map_size` inherits the `RandomMapSettings` default.
- Land creation paints requested terrain over `base_terrain`; player lands use
  deterministic player anchors and requested percentages/base sizes. Explicit
  `number_of_tiles`, `land_position`, player assignment, zones, and borders
  feed land origin and size decisions.
- Terrain clumps use requested terrain, counts, tile/percentage coverage, and
  player-start avoidance. A block-local `base_terrain` filters replacement
  tiles and cannot alter the map-wide base. Clumping changes edge regularity;
  spacing excludes terrain types already present when a clump is generated.
- Elevation clumps write requested height, count, tile coverage, level spacing,
  and map-size/group scaling. They honor block-local base terrain and avoid
  player-land origins.
- Connections operate on recorded player and neutral land origins. Supported
  connection kinds select origin pairs, while
  `default_terrain_replacement`, `replace_terrain`, and `terrain_size`
  control path painting.
- Common object creation/count/group/player/resource attributes. Neutral
  groups use seeded map-wide placement with player-distance and inter-group
  constraints. `group_variance` varies each group independently within classic
  bounds, and `second_object` overlays every placed primary.
- Supplied classic `temp_min_distance_group_placement`, bounded as the same
  per-generation minimum group-spacing field used by
  `min_distance_group_placement`. The temporary lifetime has no distinct
  representation once one object generation has been materialized.
- Weighted random branches. Selection uses a fixed seed mixer.
- `#include_drs` and `#include` are preserved but treated as non-map-affecting;
  no external include is loaded.

Evaluation constructs a blank map at the selected size, fills base terrain,
then applies active land, elevation, terrain, connection, and object
generations in classic section order regardless of source section order. It
no longer selects Arabia, Black Forest,
Islands, or Rivers from script keywords. Map-size and civilization selections
flow into the Scenario66 result. Placement is deterministic for a given seed.

The SDL random-map lobby uses this evaluator for both its preview and the
scenario started by Enter. Its Arabia, Black Forest, Islands, and Rivers
choices select reconstruction-owned RMS definitions rather than the older
hard-coded native recipes. Set `AOE_RMS_PATH` to a supported `.rms` file to
evaluate that script through the same gameplay path.

## Refusal and limits

- Unknown map-affecting directives or sections make `playable()` false.
- Unsupported commands and bodies retain exact source text plus inclusive
  first/last line numbers for tooling.
- Malformed braces/comments/random blocks fail closed.
- Default limits: 1 MiB, 20,000 lines, 200,000 tokens, nesting depth 64.
- Unsupported `if`/`elseif`/`else`, constants, defines, external include
  expansion, named-user constants, and later-engine extensions are preserved
  or rejected; they are never guessed.

Hermetic tests cover direct section-driven tile/elevation/entity placement,
common sections/directives, random selection,
same-seed output identity, civilization flow, exact unsupported spans,
closed failure for malformed/oversized input, live simulation construction,
and Scenario66 serialization.

## Supplied-package audit

The supplied `original-assets/app/Random` directory contains 15 complete
standalone `.rms` files. `ES@Canals_v2.rms` was inspected as the packet-9
package fixture. Its `temp_min_distance_group_placement 20` directive now
flows through bounded deterministic object placement. Its exact `if REGICIDE`
branch and other unsupported placement attributes remain retained with source
spans, so the complete script correctly remains inspection-only instead of
selecting a game-mode branch silently. No include dependency is required to
establish that refusal.

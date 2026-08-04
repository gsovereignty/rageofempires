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
- Player placement and map-size override. `random_placement` consumes the
  shared recovered RNG before land generation and rotates the opposed player
  pair around seeded edge anchors. `direct_placement` retains authored quarter
  anchors; `grouped_by_team` retains team grouping (the current two-player
  runtime has one member per opposing team). `override_map_size` is a tile
  count, so it snaps up to the nearest `RandomMapSize` preset
  (120/144/168/200/220/240/255) and clamps above 255. A script with no
  `override_map_size` inherits the `RandomMapSettings` default.
- Land creation paints requested terrain over `base_terrain`; player lands use
  deterministic player anchors and requested percentages/base sizes. Explicit
  `number_of_tiles`, `land_position`, player assignment, zones, and borders
  feed land origin and size decisions. `border_fuzziness` consumes seeded edge
  noise, `other_zone_avoidance_distance` rejects too-close origins from other
  zones, and `set_zone_by_team` assigns distinct player-team zones.
- Terrain clumps use requested terrain, counts, tile/percentage coverage, and
  player-start avoidance. A block-local `base_terrain` filters replacement
  tiles and cannot alter the map-wide base. Clumping changes edge regularity;
  spacing excludes terrain types already present when a clump is generated.
- Elevation clumps write requested height, count, tile coverage, level spacing,
  and map-size/group scaling. They honor block-local base terrain and avoid
  player-land origins.
- Cliff generation consumes `create_cliffs` plus minimum/maximum count and
  length, curliness, inter-line spacing, and terrain distance. It runs after
  elevation and before terrain regardless of script source order. Candidate
  lines are committed only at full requested length; every tile observes map
  edge, player-land, pre-terrain water/resource, and prior-line clearances.
  Curl decisions and retry candidates share the evaluator's recovered MSVCRT
  stream, so identical script/context/seed inputs reproduce identical cliff
  topology. Later terrain painting changes cliff top surfaces without erasing
  their topology.
- Connections operate on recorded player and neutral land origins. Supported
  connection kinds select origin pairs, while
  `default_terrain_replacement`, `replace_terrain`, and `terrain_size`
  control path painting. `terrain_cost` drives deterministic weighted
  least-cost routing rather than being accepted and discarded.
- Common object creation/count/group/player/resource attributes. Neutral
  groups use seeded map-wide placement with player-distance and inter-group
  constraints. `group_variance` varies each group independently within classic
  bounds, and `second_object` overlays every placed primary.
  `max_distance_to_other_zones` constrains neutral group centers to a land
  zone. `set_gaia_unconvertible` reaches scenario placement, current scenario
  persistence, runtime conversion checks, and Save117 persistence.
- Supplied classic `temp_min_distance_group_placement`, bounded as the same
  per-generation minimum group-spacing field used by
  `min_distance_group_placement`. The temporary lifetime has no distinct
  representation once one object generation has been materialized.
- Weighted random branches consume recovered MSVCRT `rand()` values against a
  fixed 100-slot chance table. Weights are not normalized; an unclaimed tail
  selects no branch, matching classic `percent_chance` behavior.
- `#const`, conditional `#define`, nested `if`/`elseif`/`else`, and caller
  match definitions are evaluated case-insensitively. Lobby evaluation supplies
  one of `TINY_MAP`, `SMALL_MAP`, `MEDIUM_MAP`, `LARGE_MAP`, `HUGE_MAP`, or
  `GIGANTIC_MAP`; import callers can additionally supply mode symbols such as
  `REGICIDE` through `RmsEvaluationContext`.
- Caller-owned `#include` and `#include_drs` bodies expand recursively before
  parsing. DRS bodies may be keyed by case-insensitive filename or numeric
  resource ID. Cycles, missing inputs, invalid include forms, and nesting past
  the original 32-level ceiling fail atomically. Resolver-free parsing marks
  either include form map-affecting and nonplayable, preventing omitted content
  from silently producing a different map.
- Live `AOE_RMS_PATH` loading expands `#include` relative to each including
  file. `#include_drs` reads BINA resources by numeric ID from an explicitly
  configured, packaged `game_data/Data/gamedata*.drs` root. Expansion archives
  take precedence over base data. Runtime never probes parent workspace paths.
  Missing resources report including filename and line; cycles and excess
  nesting report distinct failures.

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
- Default limits: 1 MiB, 20,000 lines, 200,000 tokens, syntax nesting depth
  64, and include depth 32.
- Later-engine extensions and unresolved semantic object/terrain identities are
  preserved or rejected; they are never guessed.

Hermetic tests cover direct section-driven tile/elevation/cliff/entity placement,
common sections/directives, classic 100-slot random selection and remainder,
case-insensitive match/map-size conditions, filename/resource-ID includes and
include depth,
same-seed output identity, civilization flow, exact unsupported spans,
closed failure for malformed/oversized input, live simulation construction,
and current Scenario serialization. Cliff coverage fixes count and length,
edge and player-land avoidance, classic section ordering, impassability,
same-seed identity, and Scenario68/Save116 round trips.
Placement coverage exercises formerly silent directives in a combined script,
same-seed order, zone spacing, weighted routes, and Gaia
convertibility through scenario/runtime persistence.

Include regression additionally covers relative and duplicate filesystem
includes, block-comment suppression, DRS resource 54000, included constants
and definitions, source-ordered evaluation, same-seed identity, missing
resolver diagnostics, cycles, and depth failure. A read-only validation against
the seven supplied HD scripts that use `#include_drs random_map.def 54000`
confirmed every script consumes the expansion resource: Capricious and Moats
generate deterministic maps, while five scripts refuse on their later,
independently unsupported semantics instead of running with the include
discarded.

## Supplied-package audit

The supplied `original-assets-hd/app/Random` directory contains 15 complete
standalone `.rms` files. `ES@Canals_v2.rms` was inspected as the packet-9
package fixture. Its `temp_min_distance_group_placement 20` directive now
flows through bounded deterministic object placement. Its exact `if REGICIDE`
branch and other unsupported placement attributes remain retained with source
spans, so the complete script correctly remains inspection-only instead of
selecting a game-mode branch silently. No include dependency is required to
establish that refusal.

The complete supplied set was also counted read-only for placement vocabulary:
`random_placement` occurs in 15/15 scripts, `border_fuzziness` in 9/15,
`other_zone_avoidance_distance` in 10/15, `set_zone_by_team` in 5/15,
`terrain_cost` in 7/15, and `max_distance_to_other_zones` in 15/15. These
directives now enter their enclosing generation models. Any unknown later
map-affecting directive still makes the whole document nonplayable.

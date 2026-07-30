# Classic RMS inspection and bounded import

This module inspects a strict subset of classic *Age of Conquerors* random-map
scripts (`.rms`) and evaluates accepted scripts into reconstruction-native
Scenario66 maps. It does not claim full RMS compatibility or reproduce the
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
- Common land/terrain/elevation/cliff/connection creation commands and their
  distance, count, scale, border, clumping, and placement attributes.
- Common object creation/count/group/player/resource attributes.
- Supplied classic `temp_min_distance_group_placement`, bounded as the same
  per-generation minimum group-spacing field used by
  `min_distance_group_placement`. The temporary lifetime has no distinct
  representation once one object generation has been materialized.
- Weighted random branches. Selection uses a fixed seed mixer.
- `#include_drs` and `#include` are preserved but treated as non-map-affecting;
  no external include is loaded.

Evaluation maps accepted high-level terrain evidence onto the deterministic
native Arabia, Black Forest, Islands, or Rivers generators. Map-size and
civilization selections flow into the Scenario66 result. This is a bounded
semantic bridge, not an emulation of AoC tile placement.

## Refusal and limits

- Unknown map-affecting directives or sections make `playable()` false.
- Unsupported commands and bodies retain exact source text plus inclusive
  first/last line numbers for tooling.
- Malformed braces/comments/random blocks fail closed.
- Default limits: 1 MiB, 20,000 lines, 200,000 tokens, nesting depth 64.
- Unsupported `if`/`elseif`/`else`, constants, defines, external include
  expansion, named-user constants, and later-engine extensions are preserved
  or rejected; they are never guessed.

Hermetic tests cover common sections/directives, random selection,
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

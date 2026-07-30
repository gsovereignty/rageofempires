# Eight-player roster refactor map

## Current boundary

Simulation and custom formats model exactly two playable participants:
`Player::{blue, red}` plus `neutral`. Legacy scenario parsing can see larger
source tables, but conversion selects only source players 1 and 2. Multiplayer
is one host/one joiner over one TCP stream. Teams exist in lockstep config, but
only compare blue against red.

This is structural, not one isolated limit.

## Core identity and state

### `include/aoe/types.hpp`

- `Player` has only `blue`, `red`, `neutral`.
- `MatchOutcome` encodes color-specific `blue_victory` and `red_victory`.
- Units, buildings, objectives, triggers, effects, messages, projectiles, and
  commands store `Player`; these types can carry expanded IDs once codecs and
  validation stop assuming two values.

Target:

- Eight stable playable slots, zero-based internally.
- Preserve source-level aliases `blue == player1`, `red == player2`.
- Keep neutral outside playable index space.
- Replace color-specific outcome with winning-team/winning-player data. Keep
  legacy outcome adapter until old saves, campaign gates, and UI migrate.

### `include/aoe/simulation.hpp`, `src/simulation.cpp`

Paired fields requiring indexed player state:

- economy, age, technologies, civilization
- formation and farm-reseed queue
- controller state
- explored-map bitsets
- victory countdown/kind/last-tick
- Mayan resource remainder

`blue_red_diplomacy_` must become an 8x8 relation matrix. Self-relation should
be ally; neutral-world ownership must not receive economy/technology/controller
state. Many accessors currently use `blue ? blue-field : red-field`, so any new
player silently aliases red until every accessor is indexed.

Two-player algorithms requiring redesign:

- `mutable_statistics` maps red to index 1 and everything else to index 0.
- kill/raze attribution derives “opponent” by color inversion.
- resignation awards the single opposite color.
- conquest, score, time, relic, and wonder victory aggregate only two sides.
- allied victory is a special blue+red result rather than team elimination.
- countdown cancellation and objective completion inspect two players only.
- demo creation and default setup spawn two starts.

Target state should own:

```text
players[8]              per-player economy/tech/vision/controller/statistics
diplomacy[8][8]         directed relation, with explicit reciprocity policy
team_id[8]              lobby/scenario team, not inferred from diplomacy
active_slots bitset     empty/closed slots excluded from victory and lockstep
neutral world state     separate from playable players
```

Victory must operate on surviving teams: resignation/defeat removes one
participant; a team wins only when all hostile active teams are defeated or a
team-level objective completes. Attribution uses actual killer/owner, never an
inverted “other player.”

## Scenario and map generation

### `include/aoe/scenario.hpp`, `src/scenario.cpp`,
`src/scenario_editor.cpp`

`Scenario` has paired economy, age, civilization, technology, formation, and
one blue-red diplomacy field. Text grammar names blue/red. Save/load and
`create_simulation` copy paired values explicitly.

Replace with roster entries plus diplomacy/team data. Parser must continue
accepting blue/red keys as aliases for player 1/2 and writer needs an explicit
legacy mode or a new scenario-version grammar.

### `include/aoe/legacy_scenario.hpp`, `src/legacy_scenario.cpp`

Legacy metadata already exposes player arrays and diplomacy vectors (tests see
16-wide diplomacy rows), but conversion reads resources/settings from players
1 and 2 and maps only `[1].diplomacy[2]`. Convert source slots 1..8, preserve
empty/disabled slots, map source color/team/civilization, and retain Gaia as
neutral rather than playable participant.

### `include/aoe/random_map.hpp`, `src/random_map.cpp`,
`include/aoe/rms_import.hpp`, `src/rms_import.cpp`

Random-map placement uses two-element `starts`/`found` arrays and blue/red
spawns. Generalization needs requested active-player count, deterministic
slot-ordered start placement, minimum-distance validation across all starts,
and per-slot starting units/buildings. Seed behavior for existing two-player
maps must remain byte-for-byte deterministic.

## Statistics, serialization, replay

### `include/aoe/match_statistics.hpp`, `src/statistics_view.cpp`,
`include/aoe/statistics_view.hpp`

`players`, current score, and timeline score/population/gathered arrays all
have length 2. Accessor maps red to 1 and all others to 0. Comparison view names
blue/red and draws two series.

Expand storage to eight slots plus active-slot metadata. UI may page/filter
participants and aggregate team totals. Old two-column view remains when only
blue/red active.

### `src/save_game.cpp`, `include/aoe/save_game.hpp`

Save format writes paired controllers, formations, reseed queues, countdowns,
ages, civilizations, remainders, blue/red economies, one diplomacy value, and
two-element statistics timelines. Load mirrors these fields. Some player fields
use local `encode`, while objective/trigger/effect/message records also use
direct integer casts; this is a compatibility trap if enum ordinals change.

New save version should write:

- roster count and slot records
- active/controller/team/civilization/economy/age/formation per slot
- diplomacy matrix
- per-player explored tiles, technologies, statistics, countdowns
- neutral entities unchanged
- winning team/players instead of color outcome

Loader must upgrade old blue/red saves into slots 0/1. Writer should not emit
new-player data in old format.

### `src/game_command.cpp`

Replay parser validates players only in inclusive blue..red range and text
codec recognizes only color names. Command ownership otherwise already carries
`Player`. Add stable numeric slot tokens while accepting `blue`/`red`; bump
command schema and retain legacy decode.

### `src/legacy_recorded_game.cpp`,
`include/aoe/legacy_recorded_game.hpp`

Audit owner/command-player conversion against all original slots. Imported
records must preserve source slot identity instead of collapsing non-blue
owners.

## AI

### `include/aoe/computer_player.hpp`, `src/computer_player.cpp`

`ComputerPlayer` can store a `Player`, but defaults to red and strategic code
often computes one opponent by color inversion. Target selection, threat
assessment, countdown response, scouting, and final-enemy detection must query
all hostile players through diplomacy, score candidates deterministically, and
avoid allied/neutral targets. One AI instance is required per AI-controlled
active slot. Save/load already serializes one AI state at a time; game-level
ownership must serialize a collection keyed by slot.

Tests must cover two hostile players, allied players, team victory, target
switch after elimination, neutral Gaia, and deterministic tie-breaking.

## Network and lobby

### `include/aoe/multiplayer.hpp`, `src/multiplayer.cpp`

Hardcoded structures:

- config owns `blue` and `red` records
- session owns `Peer blue_`, `Peer red_`
- each tick owns `TurnPair {blue, red}`
- ready/start require both booleans
- config canonicalization serializes exactly two records
- player text codec accepts only blue/red
- turn hash comparison, command execution, disconnect, chat, and signal loops
  enumerate two colors

Replace with ordered roster vector/fixed array, peer map keyed by slot, and
turn bundle containing one frame per required connected/AI slot. Canonical
digest must sort by slot and include active/closed/controller/team fields.
Protocol version must increase; version 2 stays decodable for two-player peers
but cannot join expanded sessions.

### `include/aoe/multiplayer_transport.hpp`,
`src/multiplayer_transport.cpp`

Transport is a point-to-point driver with one `remote_slot_`, host=blue,
joiner=red, one stream, pairwise heartbeat/control ACK, pairwise allied routing,
and host dropping its sole peer. `LocalhostMultiplayerRuntime` likewise owns
one listener attachment and derives local/remote by color.

Eight players require host fan-out to up to seven connections (or a separate
relay abstraction), per-peer queues/heartbeat/reliability, broadcast turn/chat/
signal/control, and quorum rules. Host authority must assign slots; transport
identity cannot trust a frame's self-declared player. Dropping one peer should
resign, pause, transfer to AI, or close that slot according to explicit policy,
not terminate every other peer.

### `include/aoe/multiplayer_checkpoint.hpp`,
`src/multiplayer_checkpoint.cpp`

Save barrier stores blue/red optional submissions and envelope has two bundle
sequence fields. Replace with slot-keyed submissions; match only after every
required human peer submits the same tick/hash. Envelope persists roster and a
sequence per participating peer. Legacy two-submission envelope remains
readable.

## SDL/UI and assets

### `src/sdl_app.cpp`

Two-player assumptions span:

- lobby `blue_ready`/`red_ready`, host/join role, local/remote slot
- active player and observer switching
- paired blue/red sprite/composite caches for nearly every unit/building
- player-color selection and sheep/building variants
- blue/red resource and score labels
- two-series statistics/victory text
- multiplayer status, chat allies, signals, resign/drop flows
- default opponent AI and new-game setup

Sprite structs should index player-color variants by slot/color rather than
duplicate named fields. Missing player-color assets need explicit fallback
classification; never silently render players 3..8 as red. Lobby needs eight
rows with slot state, color, civilization, team, controller, ready, and peer.
Two-player layout remains unchanged for blue/red-only sessions.

### `src/ui_perspective.cpp`, `tests/ui_perspective_tests.cpp`

Opponent helper returns red for blue and blue for red. Replace single-opponent
API with `visible_players`, `hostile_players`, or relation queries. Selection
and command-panel code should use local controller slot, not color inversion.

## Other consumers

- `src/campaign.cpp`: scenario completion interprets blue/red outcome relative
  to campaign player. Move to winning-team membership while keeping adapter.
- `src/save_browser.cpp`: outcome labels and metadata need generic winner data.
- `src/settings.cpp`: audit stored local color/slot assumptions.
- `src/audio_system.cpp`: blue/red string hits appear largely asset naming;
  audit whether any event routing depends on owner.
- `include/aoe/legacy_assets.hpp`, `src/legacy_assets.cpp`,
  `include/aoe/legacy_dat.hpp`, `src/legacy_dat.cpp`: player-color metadata must
  expose all archive colors, not pair-only decoded variants.
- `include/aoe/campaign.hpp`, `include/aoe/random_map.hpp`,
  `include/aoe/rms_import.hpp`: public result/config types need roster-aware
  fields.

## Tests and gates

Existing two-player tests remain compatibility fixtures. Add:

1. Stable player wire-code golden tests, including neutral.
2. Eight-slot state isolation for economy, tech, visibility, formation, and
   statistics.
3. Full directed diplomacy matrix and allied trade/targeting.
4. Three-player FFA and 2v2 conquest/resignation.
5. Team wonder/relic/score/time victories with surviving allies.
6. Eight-start deterministic random maps and classic two-start golden seed.
7. Scenario/save/replay round trips for eight slots plus old-version upgrades.
8. AI target selection among multiple enemies and no allied/Gaia attack.
9. Lockstep 3+ peer turn barrier, hash mismatch, timeout/drop, reconnect/AI
   policy, allied chat/signal routing, and checkpoint quorum.
10. UI lobby, observer perspective, player-color rendering, score/stat paging,
    and blue/red layout snapshots.

Important existing suites: `simulation_tests`, `match_statistics_tests`,
`statistics_view_tests`, `legacy_scenario_tests`, `random_map_tests`,
`rms_import_tests`, `scenario_editor_tests`, `ui_perspective_tests`,
multiplayer transport/checkpoint tests, `multiplayer_signal_tests`, and SDL
localhost/signal smoke scripts.

## Staged migration

1. **Codec foundation.** Introduce one stable player wire codec and replace
   direct casts/ad-hoc blue-red encoders in save, scenario, replay, and
   lockstep. Preserve current wire values `blue=0`, `red=1`, `neutral=2`.
2. **Roster model.** Add maximum-eight roster/slot metadata without enabling
   extra slots. Keep blue/red aliases and two-player defaults.
3. **Indexed simulation state.** Move paired fields into arrays and diplomacy
   matrix. Keep old accessors/adapters; verify unchanged two-player hashes.
4. **Scenario/import/map.** Populate up to eight original slots and starts;
   retain legacy two-player grammar and seed behavior.
5. **Victory, teams, AI.** Replace opponent inversion and color outcomes with
   hostile-player/team algorithms.
6. **Save/replay/statistics.** Bump versions, add roster records and upgrade
   readers.
7. **Network session.** Generalize config, peers, turns, barriers, control, and
   checkpoints; bump protocol.
8. **Transport/lobby/UI/assets.** Host fan-out, eight-row lobby, observers,
   indexed player colors, generic winner/statistics views.
9. **Enable slots 3..8** only after every red-fallback ternary and two-element
   invariant is gone.

## Safest first code slice

Start with stage 1 only: a central explicit player wire codec plus golden tests,
then migrate existing serializers to it without changing enum members, save
bytes, protocol bytes, or behavior.

Do **not** expand `Player` first. Current code contains many `blue ? X : red`
fallbacks, so a new slot would silently mutate red state. Also changing
`neutral`'s ordinal would corrupt direct-cast save/scenario fields. Codec
centralization removes that hidden dependency and creates a reviewable gate
before roster expansion.

## Stage 1 source status

`include/aoe/player_codec.hpp` now owns stable numeric IDs, text names,
playable/color validation, and slot lookup. `tests/player_codec_tests.cpp`
contains compile-time and runtime golden checks. The isolated `save_game.cpp`
owner/technology/exploration encoder now uses this API with unchanged bytes.

Stage 2 also migrates replay command player fields and lockstep config/frame
player fields to the central codec. Existing numeric replay records remain
`0`/`1`; lockstep names remain `blue`/`red`; neutral remains rejected from
playable command/session positions. Golden replay and lockstep round trips cover
those contracts.

Deferred migrations, intentionally not mixed with active trigger/scenario work:

- `save_game.cpp`: objective, trigger condition/effect, message direct casts
  and corresponding reads.
- `scenario.cpp`: scenario grammar's line-reporting player parser.

These remain format-compatible today because current enum ordinals happen to
match wire IDs. They must move to explicit codec validation before enum growth.

Stage 3 adds an unintegrated `PlayerSlotId`/`TeamId`/`MatchRoster` model in
`include/aoe/player_roster.hpp`. It defines eight canonical slot names and
colors, neutral outside playable indexes, teams 1..4 or none, legacy blue/red
adapters, occupied human/computer controllers, cooperative shared-human
control, and global controller/slot uniqueness. Golden source tests live in
`tests/player_roster_tests.cpp`. Simulation and formats still use legacy
`Player`; enabling new slots remains forbidden.

Stage 4 adds isolated `RosterDiplomacy` in
`include/aoe/roster_diplomacy.hpp`: directed 8x8 ally/neutral/enemy stances,
symmetric team seeding, allied-victory/shared-vision policy flags,
cooperative same-slot semantics, strict occupied-slot mutation, and canonical
FNV-1a-tagged state hashing. Its legacy adapter seeds blue/red as enemies with
self-alliance and neutral unused slots, matching current defaults. Golden and
behavior tests live in `tests/roster_diplomacy_tests.cpp`; Simulation remains
unintegrated.

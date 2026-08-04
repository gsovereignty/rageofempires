# Content and surrounding-system fidelity

## Requirement boundary

Commercial scenario, campaign, saved-game, and recorded-game compatibility is
not a project requirement. Inspectors and bounded converters for these formats
are optional archaeology tools. Unsupported commercial records, incomplete
conversion, and lack of proprietary encoders do not count as release blockers
or required content gaps. Native formats define supported runtime
compatibility.

## Evidence boundary

The complete audited live root `/tmp/aoe-assets.grSVdf/app` contains no files
recognized by the inventory as campaign, scenario, or localization payloads.
It contains two loose audio files and two sound archives:

| Category | Files | Bytes |
| --- | ---: | ---: |
| Campaign | 0 | 0 |
| Scenario | 0 | 0 |
| Localization | 0 | 0 |
| Loose audio | 2 | 1,026,533 |
| Sound archive | 2 | 41,681,772 |

Exact paths, sizes, and SHA-256 values are in
`generated/live_content_assets_inventory.json`. Zero means “not present in
this audited root,” not “commercial distributions never contain it.”

## Implemented bounded systems

### Campaigns

`src/campaign.cpp` implements ordered campaigns, canonical path containment,
content digests, atomic contiguous progress, and victory-gated unlocks.
`src/legacy_campaign.cpp` parses `.cpn`, `.cpx`, and `.cpx2`, preserves complete
source bytes, supports exact reserialization, converts ordered embedded
scenarios into user-data native files, and feeds same progression/presentation
path. Unknown fields and unindexed gaps remain byte-exact.

Evidence: `CAMPAIGN_FIDELITY.md`, `CAMPAIGN_IMPORT_FIDELITY.md`, campaign tests
in `tests/simulation_tests.cpp`, and `tests/legacy_scenario_tests.cpp`.

### Multiplayer

Multiplayer is no longer absent. `src/multiplayer.cpp`,
`src/multiplayer_transport.cpp`, and `src/multiplayer_checkpoint.cpp` implement
a reconstruction-native protocol-3 lockstep core with two to eight occupied
slots, immutable roster/controllers/teams/directed-diplomacy session
metadata, input delay, RTT metrics, ordered chat, synchronized pause/speed,
matched-hash checkpoints, a configured multi-peer localhost star-relay
harness, wait/suspend/drop handling, and observer behavior after resignation.

Missing product layers are discovery, authentication, lobby/roster UI,
checkpoint transfer UI, moderation, multi-peer SDL/UI plumbing, reconnect,
host migration, and AI takeover. No commercial wire equivalence is claimed.
Evidence: `MULTIPLAYER_FIDELITY.md`, `../contracts/RESIGNATION_OBSERVER.md`,
`tests/multiplayer_signal_tests.cpp`, and SDL localhost/signal smoke scripts.

### Scenarios, triggers, editor, and random maps

Scenario64 stores bounded condition/effect vectors. Runtime evaluates
conditions with AND semantics and applies ordered effects transactionally.
Save110 preserves runtime vectors; Save107 introduced them, while Save106 and
older single-pair state migrates;
Replay64 remains deterministic. `ScenarioEditor` supports terrain/elevation,
placements, player state, match rules, objectives/triggers, validation,
save/load, and undo/redo.

Native random maps include Arabia, Black Forest, Islands, and Rivers.
`src/rms_import.cpp` parses a bounded classic RMS subset with exact unsupported
spans and deterministic evaluation into those generators. This is not AoC tile
generation emulation.

Classic SCX inspection decodes players, terrain/elevation, objects, and raw
triggers. Bounded conversion maps proved terrain/object IDs and common
multi-condition/effect semantics, preserving every unsupported raw record and
suppressing partial playable output when semantics are not lossless.

Evidence: `../contracts/TRIGGER_FIDELITY.md`, `SCENARIO_IMPORT_FIDELITY.md`,
`RMS_IMPORT_FIDELITY.md`, `src/scenario_editor.cpp`, and focused scenario,
editor, random-map, RMS, and legacy-scenario tests.

### Recorded games and classic AI

MGX support is beyond inspection-only: strict typed actions can convert to
Replay64 when timing/player/entity/unit/technology mappings are explicit and
the stream has no unsupported tail. Stop, integral move, train, research,
diplomacy, and canonical resignation are lossless mappings. Contextual primary
actions, fee-bearing tribute, unsupported multipurpose actions, and unknown
tails remain blockers.

Classic `.ai`/`.per` support parses constants, loads, random loads, rules,
facts, and actions with exact unsupported spans. A bounded fully understood
subset evaluates deterministically to typed intents. Package resolution,
conditional loads, full vocabulary, and higher-level producer/builder/target
policy remain incomplete.

Evidence: `RECORDED_GAME_IMPORT_FIDELITY.md`, `AI_SCRIPT_FIDELITY.md`,
`src/legacy_recorded_game.cpp`, `src/legacy_ai_script.cpp`, and their focused
tests.

### Localization, saves, and statistics

Localization is no longer compiled-English-only. `StringTable` provides strict
runtime keys and English fallback; locale negotiation loads bounded `.lang`
overrides; PE resource extraction decodes selected-language `RT_STRING`
blocks, converts UTF-16 to UTF-8, and reports unmapped commercial IDs.

Save browsing validates native version metadata and exposes safe summaries.
Authoritative match statistics track resources, tribute, creation/loss/kill/
raze, conversion, relic, technology, Wonder, age timing, current score, and
100-tick timeline snapshots; Save110 preserves them and the statistics view
renders them.

Evidence: `src/localization.cpp`, `tests/localization_tests.cpp`,
`src/save_browser.cpp`, `tests/save_browser_tests.cpp`,
`include/aoe/match_statistics.hpp`, `src/simulation.cpp`,
`src/save_game.cpp`, `src/statistics_view.cpp`,
`tests/match_statistics_tests.cpp`, and `tests/statistics_view_tests.cpp`.

### Audio

`AudioSystem` resolves user-owned loose music/ambience and expansion-ordered
DRS WAV resources, then provides deterministic playlist cycling and bounded
selection, movement, command, training, combat, death, completion, and
destruction playback. Missing assets degrade safely.

Remaining work: civilization music/voice/taunt policy, spatial panning and
attenuation, mixing categories and volume, focus/pause policy, subtitles, and
complete event coverage. Evidence: `AUDIO_ARCHIVE_FIDELITY.md`,
`src/audio_system.cpp`, audio tests, and the live inventory.

## Required remaining content gaps

1. Full unit/building/technology/civilization/effect catalog beyond the
   represented enums and generated matrix.
2. More than two runtime player slots and full remote multiplayer product
   flow.
3. Full commercial AI/RMS semantics and package/include resolution.
4. Native briefing/cinematic/editor UI workflows.
5. Complete runtime localization mapping, grammar/plural rules, font and
   layout validation, shipped language packs, and localized audio.
6. Complete reactive audio/mixing policy.
7. Known physically absent visual resources documented in
   `../assets/PROCEDURAL_BUILDING_ASSET_MAP.md` and
   `generated/renderer_asset_coverage.json`.

Commercial scenario/campaign/save/replay import coverage and proprietary
encoders are intentionally excluded from this list.

## Claims that still require original-runtime validation

Deterministic reconstruction contracts are implemented but not proof of
commercial equivalence: campaign unlocking; trigger polling/order/looping;
lockstep recovery/wire behavior; formation regrouping; trade fees/turnaround;
conversion probability/resistance; fog/Ballistics timing; garrison rules;
victory/team/resignation/Atheism semantics; and native random-map placement.
Their subsystem fidelity documents define exact validation boundaries.

## Bounded claims

- Native formats are not proprietary file/wire compatibility.
- Commercial import reports never silently promote partial semantics.
- Optional assets are read from user-owned files and are not bundled.
- Inventory presence does not prove a decoder or runtime behavior.
- Deterministic native behavior is not labeled commercially exact without
  reproducible original-runtime evidence.

# Reconstruction evidence log

This file keeps historical evidence separate from modern design choices.
Confidence describes interpretation, not decompiler correctness.

## Platform boundary

Observed binary evidence:

- PE32 x86 executable.
- Imports `d3d9.dll`, `DSOUND.dll`, `IMM32.dll`, `WINMM.dll`, and Win32 UI APIs.
- `Direct3DCreate9` call appears in recovered function `FUN_004d69b0`, near
  `AoK-HD-patched.c:137010`.
- Steam, zlib, file-dialog, registry, and version APIs cross executable boundary.

Interpretation: original release couples engine to early-2010s Windows desktop
APIs. Confidence: high.

Modern choice: `SdlApp` owns window, input, clock, and drawing. `Simulation`
contains no SDL types. SDL3 supplies native macOS Metal-backed presentation
without leaking platform concerns into game rules.

## Original sound metadata

Observed VER 5.7 DAT evidence:

- 506 conceptual sounds, each with an ID, playback delay, cache value, and
  zero or more 23-byte file records.
- Each file record contains a 13-byte internal filename, signed DRS resource
  ID, probability, civilization selector, and icon-set selector.
- Selector value `-1` is retained exactly as the generic/wildcard sentinel;
  the parser does not invent optional or ownership semantics.

Interpretation: sound IDs attached to units and graphics select conceptual
sounds; file records then choose original WAV resources using probability and
civilization/icon-set context. Confidence: high, validated against the legally
supplied HD DAT and the independent metadata extractor.

Modern choice: `LegacyDatFile::sounds()` preserves the complete stable-prefix
records and `LegacyDatFile::sound(id)` performs ID lookup without assuming
vector position. No audio is copied or distributed. `AudioSystem` and
`FrontendAudioEvents` now provide bounded deterministic selection and reactive
playback; complete original scheduling and mixing remain separate boundaries.

`LegacyWavResources` joins those signed resource IDs to exact `wav` entries in
`Data/sounds.drs`, `sounds_x1.drs`, and `sounds_x2.drs`. Expansion archives
take precedence for repeated IDs. Reads return complete validated RIFF/WAVE
bytes suitable for an in-memory SDL loader; missing IDs and mislabeled
non-RIFF payloads fail explicitly. Live installer validation resolves
Trebuchet resource 5366 and common land-training resource 5423.
`AudioSystem` now uses this resolver instead of opening only `sounds.drs`;
base-only roots still work. Live `sounds.drs` has 1021 WAV IDs and
`sounds_x1.drs` has 365, including 307 x1-only and 58 repeated IDs. Repeated
payloads are byte-identical in the validated root, while deterministic
synthetic tests prove override order with deliberately different bytes. See
`../fidelity/AUDIO_ARCHIVE_FIDELITY.md` for hashes, exact IDs, and reactive sound links.
Reactive playback now supplies blue listener civilization. Exact
civilization records precede generic `-1` records; highest probability and
then DAT order provide a bounded deterministic choice. This is not a claim
that original weighted-random playback used the same selection policy.

## Sheep interaction

Observed executable evidence:

- Binary SHA-256:
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`.
- `FUN_005290b0` at recovered address `0x005290b0` loads
  `capsheep.wav` immediately beside `capgaia.wav`, proving distinct sheep and
  general neutral-capture feedback in original runtime.
- Supplied DAT record 594 identifies Sheep, 100 food, and its herdable
  graphics/tasks; see `ECONOMY_ASSET_MAP.md`.

Interpretation: sheep are player-interactable herdables, and neutral sheep can
cross into player control. Confidence: high for capture capability and Sheep
identity; exact original click priority and capture radius remain unrecovered.

Modern choice: a visible neutral sheep can be single-click selected for
inspection. Villager contextual right-click claims and begins gathering it in
one deterministic command, so replay cannot observe an intermediate claimed
but unordered state. Focused SDL input coverage drives real left/right mouse
events through projection and command routing.

Known incompatibility: reconstruction captures on explicit villager
interaction, not passive proximity. Exact original passive capture radius and
timing remain unproved.

## Time and determinism

Observed evidence:

- Binary imports `GetTickCount`.
- Recovered listing contains many CRT `rand` calls.
- Savegame, multiplayer-save, scenario, and replay-related strings coexist.

Interpretation: original engine includes clock-driven and random behavior whose
deterministic boundary requires deeper tracing. Confidence: medium.

Modern choice: simulation advances only through explicit `update()` ticks.
Initial vertical slice contains no nondeterministic random source. This makes
tests and future lockstep/replay work reproducible.

## Movement and production

Observed evidence:

- Recovered game behavior and data vocabulary require terrain-aware unit
  movement, unit categories, ownership, resource costs, and unit creation.
- Anonymous decompiler variables do not expose a trustworthy original
  pathfinding or production-queue abstraction.

Interpretation: movement and production behavior can be reconstructed, but
claiming original internal class structure would exceed available evidence.
Confidence: high.

Modern choice: standalone A* pathfinding returns explicit tile routes.
`Simulation` owns deterministic town-center queues and economy transactions.
Both choices are readable replacements, not asserted translations of original
implementation.

Balance assumptions now live in `game_rules.cpp`. Unit health, attack, food
cost, training time, production building, building health, and wood cost are
named fields in one reviewable table. This prevents inferred values from being
silently embedded in simulation control flow.

Computer control lives in `ComputerPlayer`, outside `Simulation`. It reads
public state and submits explicit entity commands; it cannot mutate private
state or hijack human UI selection. Fixed command intervals and deterministic
target choice preserve replay-friendly behavior.

Match completion is an explicit `MatchOutcome`, derived from surviving units
and buildings. Commands and ticks stop after victory, defeat, or draw. Building
combat uses the same entity-command and damage boundaries as unit combat,
keeping end-state behavior testable without SDL.

Player actions are typed `GameCommand` values recorded against simulation
ticks. Replay rejects out-of-order input and fails loudly if a recorded command
becomes invalid, exposing deterministic divergence instead of silently
continuing. Text replay files make experimental sequences reviewable and
repeatable across reconstruction changes.

Scenario state is likewise separated from code. Versioned text records define
map dimensions, terrain regions, economy, units, and buildings. Parser
validation and semantic round-trip tests keep editable research fixtures
reproducible. CMake embeds the exact fixture in the native macOS app bundle.

The release bundle also embeds SDL3 in `Contents/Frameworks`. Its executable
uses `@rpath` rather than a Homebrew path, and a verification script checks
dependency closure, resources, arm64 architecture, property list, and ad-hoc
signature. This preserves a reproducible macOS artifact after build tools move.

## Persistence

Observed evidence:

- Strings include `savegame\`, `savegame\multi\`, `scenario\`, and
  `scenario.inf`.
- Installer creates `app/savegame/multi/`.

Interpretation: persistence distinguishes ordinary saves, multiplayer state,
and scenario content. Confidence: high.

Modern choice: versioned, line-oriented `AOE-ARCHAEOLOGY-SAVE` format isolates
persistence behind `save_game` functions. It deliberately does not claim
compatibility with proprietary save files.

## Domain model

Observed evidence:

- Game resources and strings establish maps, scenarios, villagers, military
  units, resources, combat, and player ownership.
- Decompiled output lacks trustworthy original aggregate types and names.

Interpretation: reconstructing recovered anonymous structures directly would
preserve decompiler mistakes and produce unreadable code. Confidence: high.

Modern choice: small explicit types (`TilePosition`, `Unit`, `Economy`,
`GameMap`) model only behavior already implemented and tested. New fields should
be added from behavioral need plus recorded evidence, not speculation.

## Traceability rule

Every future reconstructed subsystem should record:

1. Binary hash and address or supporting data-file evidence.
2. Observed behavior.
3. Interpretation and confidence.
4. Modern implementation choice.
5. Known incompatibilities.

This prevents readable modernization from being mistaken for recovered original
source architecture.

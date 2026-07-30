# Next implementation targets

## Audit boundary

This audit reflects live source, hermetic tests, and supplied-asset reports on
2026-07-29. Shipped reconstruction-native systems are not listed as missing:
eight-slot lockstep/session/checkpoints and configured localhost TCP relay,
two-peer SDL chat,
Scenario66 multi-action triggers,
Save110/Replay64, campaign progression, scenario editor undo/redo, four native
random-map generators plus bounded RMS import, executable bounded classic AI
scripts, localization negotiation and PE `RT_STRING` extraction, save browser,
match statistics/timeline views, observer-after-resignation, and MGX action
conversion all have source and focused tests.

Commercial scenario, campaign, saved-game, and recorded-game compatibility is
an explicit non-requirement. Legacy-format inspectors and bounded converters
are optional research tooling. Coverage gaps in those tools are excluded from
required implementation rankings and completion criteria.

## Deconstructed reference codebase

Use the sibling [`../decompiled/`](../decompiled/) corpus when researching
original runtime structure or behavior:

- [`../decompiled/AoK-HD-patched.c`](../decompiled/AoK-HD-patched.c) is the
  primary 10,258-function C-like listing recovered from the supplied game
  executable.
- [`../decompiled/function-index.tsv`](../decompiled/function-index.tsv) maps
  recovered function addresses to stable generated names.
- [`../decompiled/AoK-HD-patched.strings.txt`](../decompiled/AoK-HD-patched.strings.txt)
  provides searchable UI, file, rule, networking, and diagnostic strings.
- [`../decompiled/pe-metadata-and-imports.txt`](../decompiled/pe-metadata-and-imports.txt)
  records executable sections and imported platform APIs.
- [`../decompiled/ghidra-project/`](../decompiled/ghidra-project/) supports
  cross-references, call graphs, disassembly checks, and type refinement.
- [`../decompiled/README.md`](../decompiled/README.md) defines corpus
  provenance, limitations, and reproduction workflow.

Target research should begin with the function index and strings, narrow to
relevant functions in `AoK-HD-patched.c`, then confirm important claims through
Ghidra cross-references or disassembly. Catalog values should prefer supplied
DAT/archive evidence over inferred decompiler fields. Generated identifiers,
types, loops, and aggregate layouts are hypotheses rather than original source.
Record accepted findings in the relevant fidelity document and
[`ARCHAEOLOGY.md`](ARCHAEOLOGY.md) using binary hash, address, observation,
interpretation/confidence, modern implementation choice, and known
incompatibilities.

Classification matters:

- **Missing system**: no complete player-facing implementation exists.
- **Partial compatibility**: a strict bounded implementation exists, but valid
  commercial content remains outside its mapping.
- **Runtime-unvalidated contract**: deterministic native behavior exists; exact
  commercial-runtime equivalence has not been demonstrated.
- **Absent supplied assets**: known resource is physically missing from audited
  archives and needs a documented fallback, not guessed decoding.

## Prioritization rules

Implementation order favors:

1. bounded vertical slices that produce visible match value;
2. work built on existing tested seams rather than new service infrastructure;
3. prerequisites that unlock several later surfaces;
4. native behavior that can be verified hermetically;
5. commercial-runtime archaeology only when authoritative evidence exists.

This makes catalog completion the first dependency: game rules, civilization
availability, simulation, AI, renderer bindings, audio, save/replay durability,
and UI all consume that catalog. Player-facing catalog surfaces come next
because they make new content discoverable and give later editor and lobby work
reusable SDL, localization, input, and accessibility patterns. Internet
multiplayer remains important, but follows bounded native slices because
discovery, identity, deployment, moderation, and recovery introduce several
new product and service boundaries at once.

## Ranked remaining fidelity gaps

| Rank | Classification | Remaining gap and user impact | Authoritative evidence |
|---:|---|---|---|
| 1 | Missing system | **Remaining commercial gameplay catalog.** Reconstruction represents 96 units, 27 buildings, and 158 technologies, not the complete classic/AoC construction, unit, hidden-gate, upgrade, civilization, projectile, and effect graph. Missing entries narrow native match variety. The Celtic Woad Raider family is now complete. | `include/aoe/types.hpp`, `src/game_rules.cpp`, `src/technology_tree.cpp`; `generated/civ_tech_tree_matrix.json`, `CIV_TECH_TREE_MATRIX.md`, `WOAD_RAIDER_ASSET_MAP.md`, and `UI_ICON_EVIDENCE.md` give current bounded counts and unresolved definitions/icons. |
| 2 | Missing system | **Complete frontend surfaces.** Current SDL HUD exposes core play, objectives/messages, campaign state, random maps, multiplayer signals, statistics, localization, and save browsing. Missing surfaces include full technology/civilization browsers, native briefing/cinematics, editor UI, robust lobby/accessibility workflows, input rebinding, and complete original panel behavior. A technology/civilization browser is the first slice because it exposes catalog work and establishes reusable UI patterns. | `src/sdl_app.cpp`, `src/command_panel.cpp`, `src/statistics_view.cpp`, `src/save_browser.cpp`; SDL smoke scripts and `docs/`. |
| 3 | Runtime-unvalidated contract | **Commercial AI behavior.** Native economy, production, scouting, combat, trade, religion, victory, and deterministic `.per` intent execution exist, but producer/builder/placement/army-target policy and full Genie rule vocabulary/package loading are incomplete; native policy is not proved commercially equivalent. | `src/computer_player.cpp`, `src/legacy_ai_script.cpp`; `tests/legacy_ai_script_tests.cpp`, extensive AI cases in `tests/simulation_tests.cpp`; `AI_FIDELITY.md`, `AI_SCRIPT_FIDELITY.md`. |
| 4 | Missing system | **Complete reactive audio and mixing.** Archive-backed music, ambience, and bounded event sounds exist. Civilization/voice/taunt policy, spatial attenuation/panning, mixing buses, category volume, focus/pause behavior, subtitles, and complete event-to-sound coverage remain. This is bounded, match-wide value on an existing subsystem. | `src/audio_system.cpp`, audio tests, `AUDIO_ARCHIVE_FIDELITY.md`, `generated/live_content_assets_inventory.json`. |
| 5 | Missing system | **Complete localization product integration.** Native `.lang` negotiation, English fallback, strict keys, UTF-8 validation, and PE `RT_STRING` extraction exist. Missing pieces are a complete runtime-key catalog, plural/grammar rules, font fallback, localized layout testing, localized voice/audio selection, and shipped language packs. Finish reusable localization/layout behavior before multiplying large SDL surfaces. | `include/aoe/localization.hpp`, `src/localization.cpp`, `tests/localization_tests.cpp`. |
| 6 | Missing system | **Commercial-grade editor and content authoring.** `ScenarioEditor` supports terrain/elevation, placements, player state, rules, objectives/triggers, validation, save/load, and undo/redo, but has no SDL authoring surface, unit groups, cinematic/briefing authoring, embedded AI/RMS packaging, or polished native authoring workflow. Build its SDL surface after the smaller catalog browser establishes shared frontend patterns. Proprietary import/export compatibility is outside scope. | `include/aoe/scenario_editor.hpp`, `src/scenario_editor.cpp`, `tests/scenario_editor_tests.cpp`; `SCENARIO_EDITOR.md`. |
| 7 | Missing system | **Remote multiplayer product flow.** Native protocol-3 session metadata, deterministic turns, command sources, hashes, checkpoints, and a configured localhost star relay support up to eight occupied slots. SDL/environment flow remains one host plus one joiner. Discovery, authentication, multi-peer lobby/roster UI, Internet transport, checkpoint transfer UI, reconnect, host migration, moderation, and AI takeover are absent or intentionally unsupported. First extend the existing localhost flow to a multi-peer lobby; defer Internet services until deployment and identity contracts exist. | `src/multiplayer.cpp`, `src/multiplayer_transport.cpp`, `src/multiplayer_checkpoint.cpp`; `multiplayer_roster_tests`, `multiplayer_transport_roster_tests`, multiplayer signal/SDL localhost tests; `MULTIPLAYER_FIDELITY.md`. |
| 8 | Runtime-unvalidated contract | **Cross-cutting match semantics.** Formation regrouping, trade payout/fees, fog and Ballistics timing, garrison acceptance/ejection/projectiles, religious conversion probability/resistance, victory precedence/team aggregation/resignation, and Atheism use deterministic native rules without full original-runtime validation. Do not rewrite these systems without reproducible original-runtime evidence, so evidence capture—not speculative code—is the next action. | `FORMATION_FIDELITY.md`, `TRADE_ASSET_MAP.md`, `DEFENSIVE_ASSET_MAP.md`, `GARRISON_ASSET_MAP.md`, `RELIGIOUS_ASSET_MAP.md`, `VICTORY_ASSET_MAP.md`; corresponding focused cases in `tests/simulation_tests.cpp`. |
| 9 | Partial compatibility | **Full RMS and classic AI package semantics.** Bounded RMS evaluation deliberately maps accepted scripts onto four native generators; it does not emulate AoC tile placement, conditional constants, include expansion, or later directives. AI loads and conditional package resolution remain inspection-only until caller supplies package context. This remains optional compatibility work unless authoritative package context becomes available. | `src/random_map.cpp`, `src/rms_import.cpp`, `src/legacy_ai_script.cpp`; `tests/random_map_tests.cpp`, `tests/rms_import_tests.cpp`, `tests/legacy_ai_script_tests.cpp`; `RMS_IMPORT_FIDELITY.md`, `AI_SCRIPT_FIDELITY.md`. |
| 10 | Absent supplied assets | **Known visible archive gaps.** Exact rendering is impossible for absent farm standing/component SLPs 419/417/418, Town Center layers 890/896/897/899/908/909/910/911, Sheep attack 3623, and palisade-gate N1 components 4877/4888. Fish Trap damage roots 5357–5359 contain no drawable layer; every other represented damage family now has exact runtime-bound art. | `PROCEDURAL_BUILDING_ASSET_MAP.md`, `RENDERER_ASSET_COVERAGE.md`, `BUILDING_DAMAGE_RUNTIME_EVIDENCE.md`, and generated renderer/damage catalogs. |

## Runtime validation backlog

These contracts should not be relabeled “missing”: code and tests exist.
Remaining work requires reproducible original-runtime capture or authoritative
format evidence.

- Trigger polling/priority/loop timing and newly represented ordered atomic
  effects: `TRIGGER_FIDELITY.md`.
- Campaign unlock/progress policy: `CAMPAIGN_FIDELITY.md`.
- Multiplayer wire equivalence and recovery policy: `MULTIPLAYER_FIDELITY.md`.
- Formation, trade, religion, garrison, fog/projectiles, victory, and team
  semantics: subsystem fidelity documents listed at rank 8.
- Random-map placement equivalence: `RMS_IMPORT_FIDELITY.md` explicitly calls
  its evaluator a semantic bridge, not commercial generation emulation.

## Recommended next three

1. **Native catalog vertical slice**: choose one missing family supported by
   current DAT, graphics, icon, and audio evidence, then implement rules,
   civilization availability, simulation, save/replay durability, AI use,
   rendering, sound, UI, and focused tests.
2. **Technology/civilization browser**: expose represented availability,
   prerequisites, costs, upgrades, and missing-evidence states through one
   localized, keyboard-usable SDL surface with smoke coverage.
3. **Matched visual fidelity slice**: replace one high-frequency procedural
   fallback related to represented gameplay with proved archive-backed
   rendering; keep documented fallback behavior where supplied assets are
   absent.

Commercial-format import work may continue as optional archaeology research,
but it is not part of this required priority list.

## Standalone implementation packets

Each packet below is independently runnable. It repeats its evidence inputs,
code seams, verification command, and stop boundary so an implementer need not
infer dependencies from another packet. In every packet, “original binary”
means [`../Crack/AoK HD.exe`](../Crack/AoK%20HD.exe), SHA-256
`e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`,
as recorded in [`../decompiled/MANIFEST.md`](../decompiled/MANIFEST.md).

### 1. Native catalog family

- [x] Select and implement one missing unit/building/technology family.
- **Reconstruction inputs:** `include/aoe/types.hpp`, `src/game_rules.cpp`,
  `src/technology_tree.cpp`, `src/simulation.cpp`,
  `src/computer_player.cpp`, `src/sdl_app.cpp`,
  `generated/civ_tech_tree_matrix.json`,
  `generated/ui_icon_catalog.json`, `CIV_TECH_TREE_MATRIX.md`,
  `UI_ICON_EVIDENCE.md`, and family-specific `*_ASSET_MAP.md`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  [`../decompiled/AoK-HD-patched.c`](../decompiled/AoK-HD-patched.c);
  [`../decompiled/function-index.tsv`](../decompiled/function-index.tsv);
  technology-sheet dispatch `FUN_005c6750`, technology-record reader
  `FUN_00517560`, unit-sheet dispatch `FUN_005c7560`, and button renderer
  `FUN_005c5e40` documented in `UI_ICON_EVIDENCE.md`. Use Ghidra
  cross-references from selected DAT IDs, graphic IDs, sound IDs, icon sheet
  IDs 50729/50730, and any matching strings. DAT/archive records—not inferred
  decompiler layouts—are authoritative for catalog values.
- **Asset/data inputs:** user-owned `empires2_x1_p1.dat`, `graphics.drs`,
  `interfac.drs`, `sounds.drs`, `sounds_x1.drs`, and `sounds_x2.drs`, resolved
  through `AOE_ASSET_ROOT`. Record exact included/excluded DAT IDs and absent
  archive resources before editing enums.
- **Done when:** rules, civilization gates, simulation, AI, save/replay,
  graphics, icons, audio, command UI, generated reports, and focused tests all
  cover the family. Run `ctest --test-dir build --output-on-failure` plus
  relevant report generators. Stop rather than inventing any unresolved DAT,
  graphic, sound, icon, or runtime binding.

### 2. Technology/civilization browser

- [x] Add one localized, keyboard-usable SDL browser for represented
  prerequisites, costs, upgrades, and civilization availability.
- **Reconstruction inputs:** `src/sdl_app.cpp`, `src/command_panel.cpp`,
  `src/technology_tree.cpp`, `src/localization.cpp`,
  `generated/civ_tech_tree_matrix.json`, `generated/ui_icon_catalog.json`,
  `CIV_TECH_TREE_MATRIX.md`, `UI_ICON_EVIDENCE.md`,
  `HUD_LAYOUT_FIDELITY.md`, and `UI_FONT_EVIDENCE.md`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  string
  `One Button Tech Tree Screen` at
  [`../decompiled/AoK-HD-patched.strings.txt:30659`](../decompiled/AoK-HD-patched.strings.txt);
  screen construction near
  [`../decompiled/AoK-HD-patched.c:120803`](../decompiled/AoK-HD-patched.c);
  event handler `FUN_004c8000` near
  [`../decompiled/AoK-HD-patched.c:125823`](../decompiled/AoK-HD-patched.c);
  font slot `RGE_FONT_TECH_TREE_NODE` near
  [`../decompiled/AoK-HD-patched.c:162992`](../decompiled/AoK-HD-patched.c);
  icon dispatch functions listed in packet 1. Follow these through
  [`../decompiled/ghidra-project/`](../decompiled/ghidra-project/) before
  asserting original layout or navigation.
- **Asset/data inputs:** user-owned `interfac.drs`, DAT metadata, runtime
  localization catalogs, and any explicitly supplied font files. Unknown
  framing, navigation, and fonts remain labeled reconstruction-native.
- **Done when:** browser is reachable without environment-only controls,
  keyboard navigation and focus are visible, missing-evidence states are
  explicit, all text uses localization keys, layout survives representative
  long strings, and SDL smoke coverage plus `aoe_technology_tree_tests`,
  `aoe_ui_assets_tests`, and `aoe_localization_tests` pass.

### 3. Native AI policy and classic rule vocabulary

- [x] Extend producer, builder, placement, and army-target policy over the
  represented catalog, then widen `.per` vocabulary only where evidence maps
  cleanly to typed intents.
- **Reconstruction inputs:** `src/computer_player.cpp`,
  `src/legacy_ai_script.cpp`, `include/aoe/computer_player.hpp`,
  `include/aoe/legacy_ai_script.hpp`, `AI_FIDELITY.md`,
  `AI_SCRIPT_FIDELITY.md`, `CIV_TECH_TREE_MATRIX.md`, and AI cases in
  `tests/simulation_tests.cpp`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  [`../decompiled/AoK-HD-patched.c`](../decompiled/AoK-HD-patched.c),
  [`../decompiled/AoK-HD-patched.strings.txt`](../decompiled/AoK-HD-patched.strings.txt),
  function index, and Ghidra project. Search strings and cross-references for
  each proposed rule token, unit/building/technology ID, and strategy setting.
  Also retain the first-party *Computer Player Strategy Builder Guide*
  boundaries cited in `AI_FIDELITY.md` and `AI_SCRIPT_FIDELITY.md`. Absence of
  reviewed commercial AI package files means native policy must remain
  explicitly reconstruction-native.
- **Asset/data inputs:** validated DAT metadata and any independently supplied
  `.ai`/`.per` package used as a bounded fixture; never silently resolve
  conditional loads without package context.
- **Done when:** choices use visible/explored state, normal command/payment
  paths, stable tie-breaks, civilization gates, and durable controller state.
  Run `aoe_core_tests` and `aoe_legacy_ai_script_tests`, including save/reload
  equivalence. Stop commercial-equivalence claims where executable/package
  evidence is absent.

### 4. Reactive audio and mixing

- [x] Add audio buses and category-volume controls before widening
  civilization, voice, and taunt policy.
- **Reconstruction inputs:** `src/audio_system.cpp`,
  `include/aoe/audio_system.hpp`, frontend audio-event code,
  `AUDIO_ARCHIVE_FIDELITY.md`, `ARCHAEOLOGY.md`, audio tests, and
  `generated/live_content_assets_inventory.json`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  `DSOUND.dll` and `WINMM.dll`
  imports in
  [`../decompiled/pe-metadata-and-imports.txt`](../decompiled/pe-metadata-and-imports.txt);
  `Music Volume` paths near
  [`../decompiled/AoK-HD-patched.c:113799`](../decompiled/AoK-HD-patched.c)
  and `FUN_004ed3f0` near line 158193; `Sound Volume` read/application near
  lines 161010 and 161290; matching strings at
  [`../decompiled/AoK-HD-patched.strings.txt:30580`](../decompiled/AoK-HD-patched.strings.txt)
  and line 30938. Trace `FUN_004e0f10`, `FUN_004e1030`, and
  `FUN_004e2160` in Ghidra before claiming scale or persistence equivalence.
- **Asset/data inputs:** user-owned sound DRS archives and validated DAT sound
  records. Preserve civilization selector, probability, and missing-resource
  evidence separately from reconstruction mixing policy.
- **Done when:** bus/category values persist, mute and focus/pause behavior are
  deterministic, missing archives fail softly, and focused audio tests plus
  SDL smoke coverage pass. Unknown original attenuation, panning, or weighted
  selection stays documented rather than guessed.

### 5. Localization, font fallback, and localized layout

- [x] Complete runtime keys used by new surfaces, font fallback, plural/grammar
  boundaries, and long-string layout tests.
- **Reconstruction inputs:** `src/localization.cpp`,
  `include/aoe/localization.hpp`, `src/sdl_app.cpp`,
  `tests/localization_tests.cpp`, `UI_FONT_EVIDENCE.md`, and all current
  `.lang` resources.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  locale DLL lookup near
  [`../decompiled/AoK-HD-patched.c:144954`](../decompiled/AoK-HD-patched.c);
  `<locale>` language-file strings at
  [`../decompiled/AoK-HD-patched.strings.txt:30864`](../decompiled/AoK-HD-patched.strings.txt);
  font-family/resource strings at lines 30964–31017; font parser
  `FUN_004f3010` and setup function at `0x004f3b30` documented in
  `UI_FONT_EVIDENCE.md`; PE `RT_STRING` resources and GDI font imports in the
  PE metadata.
- **Asset/data inputs:** supplied language DLL/text files and font files only.
  Keep English and platform-font fallback explicit when referenced files are
  absent.
- **Done when:** strict-key validation, English fallback, UTF-8 handling,
  representative plural paths, glyph fallback, and long localized layouts are
  tested. Run `aoe_localization_tests` and all affected SDL smoke tests. Do not
  claim exact font metrics without matching font bytes and resource values.

### 6. Scenario editor SDL surface

- [x] Build an SDL authoring surface over the existing `ScenarioEditor`
  controller.
- **Reconstruction inputs:** `include/aoe/scenario_editor.hpp`,
  `src/scenario_editor.cpp`, `src/sdl_app.cpp`,
  `tests/scenario_editor_tests.cpp`, `SCENARIO_EDITOR.md`,
  `HUD_LAYOUT_FIDELITY.md`, and `resources/editor-roundtrip.scenario`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  `Scenario Editor Screen`,
  `Scenario Editor Menu`, and `ScenarioEditorInfo` strings near
  [`../decompiled/AoK-HD-patched.strings.txt:30641`](../decompiled/AoK-HD-patched.strings.txt)
  and lines 31273–31279; screen paths near
  [`../decompiled/AoK-HD-patched.c:117823`](../decompiled/AoK-HD-patched.c),
  353191, and 356417; RTTI string
  `TRIBE_Scenario_Editor_Panel_Object` near strings line 35461. Use Ghidra
  cross-references to bound proved panels and actions.
- **Asset/data inputs:** supplied interface DRS and cursor/HUD evidence only.
  Existing audit does not prove original editor palettes, grouping UI, or
  panel geometry; label those choices reconstruction-native.
- **Done when:** terrain/elevation, placement/removal, player state, objectives,
  triggers, validation, save/load, undo/redo, keyboard focus, localization, and
  accessibility operate through SDL. Run `aoe_scenario_editor_tests` and a
  dedicated editor smoke test with the round-trip fixture.

### 7. Local multi-peer lobby and recovery UI

- [x] Expand localhost host/join into a multi-peer lobby, roster,
  checkpoint-transfer UI, and reconnect flow over the existing native
  protocol.
- **Reconstruction inputs:** `src/multiplayer.cpp`,
  `src/multiplayer_transport.cpp`, `src/multiplayer_checkpoint.cpp`,
  roster/diplomacy/session code, `MULTIPLAYER_FIDELITY.md`, and all
  `multiplayer_*`, `player_roster`, codec, and signal tests.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  lobby/join strings at
  [`../decompiled/AoK-HD-patched.strings.txt:30365`](../decompiled/AoK-HD-patched.strings.txt),
  30955, 31281, and 31292; lobby browser construction near
  [`../decompiled/AoK-HD-patched.c:336570`](../decompiled/AoK-HD-patched.c);
  join-screen construction near line 338388; host migration/disconnect/save
  strings near strings lines 30131–30318 and 31178–31181; Steam lobby RTTI
  strings at lines 35274–35286. Correlate through Ghidra, the Microsoft manual,
  and commit-pinned packet observations listed in `MULTIPLAYER_FIDELITY.md`.
- **Asset/data inputs:** none beyond local processes and fixtures. Original
  wire compatibility is not required and must not be inferred from screen or
  diagnostic strings.
- **Done when:** up to eight occupied slots can join the localhost relay,
  readiness/roster changes replicate, checkpoint transfer and bounded reconnect
  are visible, hashes remain deterministic, and all multiplayer tests plus
  multi-process SDL smoke tests pass. Do not implement Internet transport in
  this packet.

### 8. Cross-cutting runtime semantics capture

- [x] Capture one unresolved formation, trade, religion, garrison,
  fog/projectile, victory, or team semantic before changing native behavior.
- **Reconstruction inputs:** the selected subsystem source and focused
  simulation tests plus `FORMATION_FIDELITY.md`, `TRADE_ASSET_MAP.md`,
  `DEFENSIVE_ASSET_MAP.md`, `GARRISON_ASSET_MAP.md`,
  `RELIGIOUS_ASSET_MAP.md`, and `VICTORY_ASSET_MAP.md`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  function index, strings,
  full decompiled listing, and Ghidra project. Start from selected DAT
  unit/technology/effect/projectile IDs and observable UI strings, record exact
  virtual addresses and callers/callees, then corroborate with a reproducible
  original-runtime capture or disassembly. Existing examples of required
  evidence quality are `BUILDING_DAMAGE_RUNTIME_EVIDENCE.md`,
  `MINIMAP_RUNTIME_EVIDENCE.md`, and `SELECTION_FEEDBACK_RUNTIME_EVIDENCE.md`.
- **Asset/data inputs:** matching user-owned DAT/DRS inputs and a documented
  runtime-capture setup for the exact binary hash.
- **Done when:** evidence note records binary hash, address range, setup,
  observation, interpretation/confidence, and incompatibilities; a minimal
  fixture reproduces the result. If evidence contradicts native behavior,
  update code and focused tests; otherwise make no speculative code change.

### 9. RMS and classic AI package semantics

- [x] Extend one bounded RMS directive or AI package-resolution behavior only
  with complete caller/package context.
- **Reconstruction inputs:** `src/rms_import.cpp`,
  `src/random_map.cpp`, `src/legacy_ai_script.cpp`,
  `RMS_IMPORT_FIDELITY.md`, `AI_SCRIPT_FIDELITY.md`, and their focused tests.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  search
  [`../decompiled/AoK-HD-patched.strings.txt`](../decompiled/AoK-HD-patched.strings.txt)
  for the selected directive, map filename, rule token, or load form; follow
  cross-references in the full listing and Ghidra project. Retain pinned grammar
  and public-script references already named in both fidelity documents because
  decompiled strings alone do not prove grammar or evaluation semantics.
- **Asset/data inputs:** an independently supplied complete `.rms` or
  `.ai`/`.per` package including all referenced files and selection context.
- **Done when:** parser preserves exact source spans, evaluation is bounded and
  deterministic, unknown branches/includes fail closed, and
  `aoe_rms_import_tests`, `aoe_random_map_tests`, and
  `aoe_legacy_ai_script_tests` pass. No context means inspection-only, not an
  invented branch choice.

### 10. Visible archive fallback

- [x] Replace one high-frequency procedural fallback only where supplied
  archive evidence is complete; otherwise preserve and document fallback.
- **Reconstruction inputs:** renderer loading/drawing code,
  `PROCEDURAL_BUILDING_ASSET_MAP.md`, `RENDERER_ASSET_COVERAGE.md`,
  `BUILDING_DAMAGE_RUNTIME_EVIDENCE.md`,
  `generated/procedural_building_dat_metadata.json`, and
  `generated/renderer_asset_coverage.json`.
- **Original references:** `../Crack/AoK HD.exe`, SHA-256
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`;
  damage-record loader
  `FUN_00575420`, state updater `FUN_00589490`, attachment path
  `FUN_004eaf00`, animation paths `FUN_004eb870`/`FUN_004ebb90`, and death
  cleanup `FUN_0059a7e0`, all pinned by address in
  `BUILDING_DAMAGE_RUNTIME_EVIDENCE.md`; full listing, function index, and
  Ghidra project for call-site confirmation.
- **Asset/data inputs:** user-owned DAT plus `graphics.drs`. Current known
  absent resources include Farm 417–419, Town Center layers
  890/896/897/899/908/909/910/911, Sheep attack 3623, gate components
  4877/4888, and Fish Trap damage roots 5357–5359.
- **Done when:** exact SLP/frame/hotspot/delta/state bindings are evidenced,
  procedural behavior remains for every missing component, generated coverage
  reports are refreshed, and renderer/building-damage tests plus relevant SDL
  smoke tests pass. Never substitute visually similar unproved art.

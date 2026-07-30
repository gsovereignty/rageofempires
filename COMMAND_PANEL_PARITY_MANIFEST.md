# Command-panel parity manifest

## Scope and evidence

This audit compares the reconstructed selection/command panel with:

1. the supplied 2013 executable's static-analysis corpus;
2. the parsed `empires2_x1_p1.dat` structure;
3. command paths already implemented in the reconstruction.

It covers commands exposed after selecting units or buildings. It does not
claim pixel-perfect layout, hotkeys, button slots, or icon frames unless direct
evidence exists.

Icons and sprites are part of command parity, not optional polish. A command
is incomplete until its exact archive, resource ID, frame, palette, dimensions,
state treatment, and fallback behavior are recorded in the asset manifest.

Evidence labels:

- **PROVEN**: reconstruction contradicts an implemented command path or direct
  corpus evidence.
- **PARTIAL**: feature exists, but panel exposure, eligibility, or roster is
  incomplete.
- **UNPROVED**: plausible original-game behavior without enough local evidence;
  do not implement as parity work until runtime or binary evidence is added.

Reference evidence:

- `../decompiled/AoK-HD-patched.strings.txt` contains original action classes
  for Build, Repair, Convert, Heal, Pickup Relic, Deliver Relic, Pack, Unpack,
  Trade, Transport, Gather, Guard, and related actions.
- `../original-assets-alternative/empires2_dat_schema.json` proves each unit
  header owns a variable list of 59-byte `UnitCommand` records. Those records
  are not field-decoded yet, so they prove data-driven per-unit capabilities,
  but not exact UI slot or icon assignments.
- Reconstruction command implementations are in `src/simulation.cpp`;
  panel generation is in `src/command_panel.cpp`; mouse dispatch is in
  `src/sdl_app.cpp`.

## Proven icon-sheet inventory

| Purpose | Original filename | DRS SLP | Frames | Frame size | Mapping proof |
|---|---|---:|---:|---:|---|
| Actions/commands | `btncmd.shp` | 50721 | 69 | 36x36 | Executable load proves sheet role; most command-to-frame meanings remain unproved |
| Technologies | `btntech.shp` | 50729 | 118 | 36x36 | Executable load and identity dispatch from DAT technology icon field |
| Units/training | `ico_unit.shp` | 50730 | 134 | 36x36 | Executable load and identity dispatch from DAT unit `button_icon` field |

All three sheets decode with interface palette resource 50500 from
`Data/interfac.drs`. Proprietary SLP or palette payloads must not be committed;
runtime loads them through `AOE_ASSET_ROOT`.

Current exact training-icon bindings:

| Unit | SLP | Frame |
|---|---:|---:|
| Villager | 50730 | 15 |
| Militia | 50730 | 8 |
| Spearman | 50730 | 31 |
| Archer | 50730 | 17 |
| Skirmisher | 50730 | 20 |
| Scout Cavalry | 50730 | 64 |
| Knight | 50730 | 1 |
| Camel Rider | 50730 | 78 |
| Battering Ram | 50730 | 74 |
| Mangonel | 50730 | 27 |
| Scorpion | 50730 | 80 |
| Monk | 50730 | 33 |
| Fishing Ship | 50730 | 24 |
| Galley | 50730 | 87 |
| Transport Ship | 50730 | 95 |
| Woad Raider / Elite Woad Raider | 50730 | 47 |

These mappings come from the validated VER 5.7 DAT icon field plus executable
identity dispatch. They may be used as exact bindings. Other trainable units
must obtain their DAT icon value through the same pipeline; do not select
frames by visual resemblance.

For technologies, use `ui_icons::technology(dat_icon_id)` and SLP 50729.
Technology frames must come from each technology's decoded DAT `icon_id`.

## Required fixes

### CP-001 — Villager build commands absent

- Status: **PROVEN**
- Priority: P0
- Current: selecting a villager produces only Stop and Garrison.
- Expected: villager selection must expose construction entry points and
  civilization/age-eligible buildings.
- Evidence: `Simulation::construct_building_at`, `ConstructBuildingCommand`,
  placement preview, and keyboard `set_build_mode(...)` already implement the
  behavior. Original corpus contains `TRIBE_Action_Build`.
- Cause: `PanelCommand` has no build-menu or construct-building variants, and
  `build_selection_panel` has no villager branch.
- Fix:
  1. add `open_economic_buildings`, `open_military_buildings`, and
     `construct_building` panel commands;
  2. add optional `BuildingKind` payload to `CommandButtonModel`;
  3. derive availability from `civilization_has_building`, age, prerequisites,
     and resource/placement rules;
  4. route click dispatch through `ConstructBuildingCommand`, not a second
     construction implementation.
- Icon work: prove Build-page action frames in SLP 50721. Building entries may
  require civilization-specific building icon sheets rather than 50730;
  decode executable subtype-2/10 dispatch before binding them.
- Tests: villager versus scout command sets; age/civilization filtering;
  selecting a build button enters placement mode; placement records replay.

### CP-002 — Villager Repair button absent

- Status: **PROVEN**
- Priority: P0
- Current: repair works only through contextual world interaction; no selected
  villager Repair command exists.
- Expected: villager panel exposes Repair target mode.
- Evidence: `Unit::repair_target_id` and
  `villagers_repair_buildings_with_persistent_costs` prove implemented repair
  semantics. Original corpus contains `TRIBE_Action_Repair`.
- Cause: no `PanelCommand::repair` and no pending repair-target mode in SDL UI.
- Fix: add Repair command, target cursor/mode, eligibility for villagers, and
  dispatch through existing contextual command path.
- Icon work: identify Repair action code and exact 50721 frame through
  executable control flow or original runtime capture. No guessed wrench/hammer
  frame.
- Tests: button exists only for valid builders; damaged friendly building
  accepted; invalid/enemy/full-health target rejected; replay remains
  deterministic.

### CP-003 — Monk commands absent

- Status: **PROVEN**
- Priority: P0
- Current: monks receive Stop, Garrison, and possibly generic military-derived
  commands, but no Convert, Heal, Pick Up Relic, or Drop/Return Relic buttons.
- Expected: monk-specific command set reflects carried-relic state and target
  type.
- Evidence: `Simulation::command_convert`, `command_heal`,
  `command_collect_relic`, and `command_deposit_relic` exist. Original corpus
  names Convert, Heal, Pickup Relic, and Deliver Relic action classes.
- Cause: corresponding `PanelCommand` values and click dispatch are missing.
- Fix: add monk commands and state-dependent eligibility. Reuse existing
  simulation methods.
- Icon work: independently bind Convert, Heal, Pick Up Relic, and
  Drop/Return Relic to 50721 frames. Carried-relic state must select the proved
  action icon, not reuse monk training frame 33.
- Tests: normal monk, relic-carrying monk, invalid target, conversion/healing
  target modes, monastery deposit.

### CP-004 — Trebuchet Pack/Unpack absent

- Status: **PROVEN**
- Priority: P0
- Current: trebuchets are deliberately excluded from generic military orders,
  leaving no Pack or Unpack control.
- Expected: packed and unpacked trebuchets expose opposite transform commands.
- Evidence: `Simulation::command_pack_trebuchet` and both trebuchet unit kinds
  exist. Original corpus contains Pack, Unpack, and Unit Transform actions.
- Fix: add state-specific Pack/Unpack buttons and click dispatch.
- Icon work: prove distinct Pack and Unpack 50721 frames, including whether
  packed/unpacked state changes icon or only availability.
- Tests: exact command differs by state; transformation preserves ownership,
  HP rules, selection, save, replay, and multiplayer command encoding.

### CP-005 — Trade route command absent

- Status: **PROVEN**
- Priority: P0
- Current: trade carts/cogs can enter trade-route targeting through a keyboard
  path, but selection panel has no button.
- Expected: trade units expose a Trade/Set Trade Route command.
- Evidence: `Simulation::command_trade_route` and `pending_trade_route` exist.
  Original corpus contains Trade and Offboard Trade actions.
- Fix: add trade-route command for eligible land and naval trade units.
- Icon work: prove Trade/Set Trade Route action frame. Unit portraits/training
  icons remain 50730 and must not substitute for action art.
- Tests: market/dock endpoint compatibility, diplomacy rules, invalid target,
  replay.

### CP-006 — Transport embark/disembark controls absent

- Status: **PROVEN**
- Priority: P1
- Current: simulation implements embark/disembark, but transport selection has
  no unload command and passengers have no explicit embark control.
- Expected: transport ships expose unload/disembark behavior; eligible units
  can enter compatible transports.
- Evidence: `Simulation::command_embark` and `command_disembark` exist.
  Original corpus contains Enter and Transport actions.
- Fix: add transport-aware buttons, capacity state, and target mode.
- Icon work: prove Enter Transport and Unload/Disembark action frames and any
  passenger-count overlay/chrome behavior.
- Tests: full transport disabled state, land/water target validation,
  multi-unit embark, unload placement failure.

### CP-007 — Fishing ship Fish Trap construction absent

- Status: **PROVEN**
- Priority: P1
- Current: fish-trap placement exists as a keyboard/build-mode path, but the
  fishing-ship panel cannot expose it.
- Expected: eligible fishing ship exposes Fish Trap construction.
- Evidence: `BuildingKind::fish_trap`, placement mode, and construction command
  already exist.
- Fix: use the CP-001 building payload for fishing ships, restricted to fish
  traps and valid water placement.
- Icon work: bind Fish Trap building button through the proved building-icon
  dispatch, not ordinary unit SLP 50730.
- Tests: fishing ship versus military ship command set; cost and placement.

### CP-008 — Building research panel is dead

- Status: **PROVEN**
- Priority: P0
- Current: `PanelCommand::research` and `CommandButtonModel::technology` exist,
  but `build_selection_panel` never emits research buttons and SDL click
  dispatch never handles them.
- Expected: eligible technologies appear at their owning buildings with
  prerequisite, civilization, age, already-researched, and busy-state checks.
- Evidence: `ResearchTechnologyCommand` and extensive keyboard research paths
  already exist.
- Fix: generate research buttons from one canonical building/technology
  eligibility source and dispatch `ResearchTechnologyCommand`.
- Icon work: use SLP 50729 and exact decoded technology `icon_id`; load required
  frames dynamically or generate a complete proved frame set.
- Tests: representative economic, military, university, monastery, castle,
  and age technologies; prerequisites and completed research.

### CP-009 — Building production roster is incomplete

- Status: **PARTIAL**
- Priority: P0
- Current: panel hard-codes a small roster:
  villager; militia/spearman; archer/skirmisher; scout/knight/camel; three siege
  units; monk; fishing ship/galley/transport; woad raider.
- Missing implemented examples include trade cart/cog, cavalry archer,
  hand cannoneer, petard, trebuchet, unique units other than Woad Raider,
  upgraded-line products where applicable, and other units accepted by
  `QueueUnitCommand`.
- Cause: a second, incomplete training table lives in
  `src/command_panel.cpp`.
- Fix: introduce canonical `available_units_at(building, player)` and use it
  from keyboard handling, AI checks, validation, and panel generation.
- Icon work: extend `training_unit` from hard-coded 16-unit switch to decoded
  DAT-backed bindings for every roster entry. Exact binding is SLP 50730 plus
  unit `button_icon`; building subtypes remain separate.
- Tests: table-driven matrix for every `BuildingKind`, civilization, and age.

### CP-010 — Generic building buttons shown on ineligible buildings

- Status: **PROVEN**
- Priority: P1
- Current: every selected building receives Rally Point, Ungarrison, and Cancel
  Last, including walls, farms, houses, resource camps, and other buildings
  that cannot use one or more actions. Buttons may be disabled, but still
  consume slots and misrepresent capabilities.
- Expected: commands are emitted only when building supports them; transiently
  unavailable valid commands may remain disabled.
- Fix: add explicit building capability predicates:
  `supports_rally`, `supports_garrison`, `supports_production`,
  `supports_research`.
- Tests: wall, farm, house, tower, town center, production building, monastery,
  dock, and castle matrices.

### CP-011 — Unit Garrison eligibility is over-broad

- Status: **PARTIAL**
- Priority: P1
- Current: `supports_garrison(UnitKind)` is a hand-written exclusion list. It
  treats nearly every non-ship, non-siege unit as eligible without considering
  destination compatibility, civilization rules, or current state.
- Expected: command visibility and target acceptance use the same canonical
  `can_garrison(unit, building)` rules.
- Fix: expose a coarse `can_garrison_anywhere(unit)` capability derived from
  unit class, then validate exact destination through `can_garrison`.
- Tests: infantry, villager, cavalry, monk, trade cart, siege, ship, packed
  unit, already-garrisoned unit, incompatible building.

### CP-012 — Mixed-selection commands depend on first selected unit

- Status: **PROVEN**
- Priority: P0
- Current: panel finds only `selected_unit()` and uses that unit's kind to
  decide commands for the entire `selected_units()` group. Formation visibility
  likewise depends on the first unit.
- Harm: reversing selection order can change visible commands for the same
  group; a mixed villager/military group may expose invalid military commands
  or hide valid subset commands.
- Fix: build a selection capability aggregate. Define, per command, whether it
  requires all selected units, any eligible selected unit, or a homogeneous
  selection. Dispatch only to eligible members.
- Tests: same IDs in different orders produce identical panel; mixed
  villager/scout, monk/infantry, siege/infantry, land/naval groups.

### CP-013 — Selected garrisoned unit still receives active commands

- Status: **PROVEN**
- Priority: P1
- Current: status becomes `GARRISONED`, but Stop, military, stance, and other
  commands are still generated from unit kind.
- Expected: garrison state changes available commands; ordinary world-targeted
  orders should not be presented as immediately executable.
- Fix: add state-aware capability filtering and a route to the containing
  building/ungarrison flow.
- Tests: selected garrisoned villager, archer, monk, and transport passenger.

### CP-014 — Command capacity and documented layout disagree

- Status: **PROVEN**
- Priority: P1
- Current: documentation says a 4x3 grid, while model/render/hit-testing allow
  15 buttons. `add()` silently drops every command after 15.
- Harm: adding villager buildings or full technology rosters will truncate
  valid commands based on insertion order.
- Fix: decide and document exact grid dimensions, then implement pages/submenus
  and deterministic slots. Never silently discard commands.
- Sprite work: page buttons, selected page, hover, pressed, disabled, and
  unavailable visuals need separate provenance. Frames 36/37 of 50721 are
  action artwork, not reusable button chrome.
- Tests: overflow roster, submenu navigation, hit-testing, stable ordering.

### CP-015 — Panel and keyboard command systems can diverge

- Status: **PROVEN**
- Priority: P0
- Current: availability and action mapping are duplicated across
  `src/command_panel.cpp` and a large SDL key-event switch. Several actions are
  keyboard-only; future changes can update one path but not the other.
- Fix: create a command registry containing:
  command ID, subject capability, state eligibility, label/localization key,
  hotkey, icon binding, target mode, and `GameCommand` factory. Panel clicks
  and keyboard shortcuts must invoke the same registry entries.
- Asset rule: registry stores an evidence-bearing `ui_icons::Binding`, never a
  naked frame integer.
- Tests: for each registered command, mouse and hotkey produce equivalent
  `GameCommand`/simulation state and replay record.

### CP-016 — Tests encode scout/building examples, not parity

- Status: **PROVEN**
- Priority: P0
- Current: `tests/command_panel_tests.cpp` tests scout stances/formations and
  town-center villager production. It has no villager, monk, trade, transport,
  fishing, trebuchet, mixed-selection, research, or ineligible-building matrix.
- Fix: replace example-only coverage with table-driven command-set contracts.
- Acceptance: every supported `UnitKind` and `BuildingKind` appears in at least
  one panel capability case; every `PanelCommand` has generation and dispatch
  coverage.
- Asset acceptance: every visible button has either an exact binding or an
  explicit `unproved` marker with procedural fallback. Tests verify SLP/frame
  bounds and forbid guessed bindings from being labeled exact.

## Secondary discrepancies

### CP-017 — Activity status omits implemented work states

- Status: **PROVEN**
- Priority: P2
- Current status chain reports only garrison, attack-ground, attack-move,
  patrol, guard, moving, or idle.
- Missing states include gathering, building, repairing, healing, converting,
  carrying/returning relic, trading, fishing, packing, and transport activity.
- Fix: derive display state from canonical unit action/order state.

### CP-018 — Delete command absent from panel

- Status: **PARTIAL**
- Priority: P2
- Current: simulation has unit/building deletion paths, but panel has no Delete
  command.
- Exact original eligibility, confirmation behavior, and hotkey/slot need
  runtime evidence before parity details are claimed.

### CP-019 — Exact icon and slot mapping remains unproved

- Status: **UNPROVED**
- Priority: evidence task
- Current action icon frames have been assigned in code, but local evidence does
  not yet prove every command-to-frame mapping or original slot.
- Required evidence: decode UnitCommand fields and relevant UI tables, or
  capture original runtime panels for representative selections.
- Do not treat familiar-looking artwork or remembered AoE II layouts as proof.

### CP-021 — Current action icons are mislabeled as if proven

- Status: **PROVEN**
- Priority: P0
- Current: `src/command_panel.cpp` assigns frames 0-16 from SLP 50721 directly
  to named commands such as Attack Move, Patrol, Guard, Garrison, stances,
  formations, Rally, Ungarrison, and Cancel. Renderer loads those same 17
  frames.
- Contradiction: `UI_ICON_EVIDENCE.md` states most 50721 meanings are unproved.
  Only raw executable dispatch is proved for action `0x7c` to frame 12,
  `0x7d` to frame 13, and action `0x65` to frame 30/31; those action codes do
  not yet have a proved semantic bridge to reconstruction commands.
- Harm: buttons can display valid original artwork for the wrong action while
  model and tests imply correctness.
- Fix:
  1. replace `action_archive_icon_id` with evidence-bearing `Binding`;
  2. mark existing semantic mappings unproved until traced;
  3. render procedural/text fallback for unproved mappings;
  4. add an evidence table connecting original action code, semantic command,
     SLP, frame, and proof citation.
- Tests: no named action may report `exact` without an evidence-table row.

### CP-022 — Runtime preloads only a partial, hard-coded frame set

- Status: **PROVEN**
- Priority: P0
- Current: SDL preloads action frames 0-16 and fifteen unit frames. Newly added
  commands, units, and technologies will silently fall back even when exact
  archive bindings exist.
- Fix: gather exact bindings from generated panel commands each frame or load
  and cache the complete bounded sheets. Cache key must include SLP, frame, and
  palette identity.
- Tests: late-discovered frame, technology frame 117, full production roster,
  missing archive, corrupt frame, and cache reuse.

### CP-023 — Disabled/pressed/selected icon treatment is reconstructed

- Status: **PARTIAL**
- Priority: P1
- Current: one icon texture is RGB-modulated for disabled state and shifted by
  one pixel when pressed; procedural bevel supplies chrome.
- Evidence: executable flow proves pressed state keeps the same action
  sheet/frame, but exact chrome, tint, shadow, offsets, disabled rendering, and
  selected-page visuals remain unproved.
- Fix: retain behavior as explicit reconstruction fallback. Capture original
  runtime states before claiming visual parity.
- Tests: state changes never mutate source texture; fallback works with and
  without archive.

### CP-024 — Building command icon dispatch is missing

- Status: **PROVEN**
- Priority: P0
- Current: model supports unit and action icon fields only. No building-icon
  binding type exists.
- Evidence: executable dispatch for ordinary records uses 50730, while
  subtypes 2 and 10 select civilization-indexed building sheets.
- Fix: decode and model building icon sheet selection by civilization and
  subtype. Add evidence-bearing building binding to construction buttons.
- Tests: same building across architecture sets; civilization switch; missing
  sheet fallback; fish trap and walls/gates.

### CP-025 — Icon manifest lacks per-command completion tracking

- Status: **PROVEN**
- Priority: P1
- Current: `generated/ui_icon_catalog.json` inventories command frames but does
  not map reconstruction `PanelCommand` values to proved frames.
- Fix: generate `generated/command_icon_manifest.json` with:
  `panel_command`, original action code, SLP ID, frame, palette ID, dimensions,
  evidence class, evidence citation, runtime state, and fallback.
- Gate: CI rejects duplicate semantic bindings, out-of-range frames, missing
  citations for exact mappings, or visible commands absent from manifest.

## Sprite/icon completion checklist

For every command added by CP-001 through CP-018:

1. Determine icon category: action, unit/training, technology, or building.
2. Record original DAT/action identifier; never start from visual inspection.
3. Trace executable sheet selection and frame transformation.
4. Record SLP resource ID, frame index, palette resource 50500, dimensions,
   and frame count.
5. Decode from user-supplied `Data/interfac.drs`.
6. Compare decoded frame with an original runtime capture at native scale.
7. Record normal, hover, pressed, selected, disabled, and unavailable states.
8. Add exact binding only after evidence citation exists.
9. Keep procedural/text fallback when archive or proof is absent.
10. Add contract test plus SDL capture for exact and fallback paths.

### CP-026 — Exact command visibility rules remain partly unproved

- Status: **UNPROVED**
- Priority: evidence task
- Questions needing runtime/data proof:
  whether contextual-only actions also have explicit buttons; exact page
  nesting; multi-selection intersection/union rules; disabled versus hidden
  behavior; selected enemy/ally panel behavior; exact hotkeys.

## Recommended implementation order

1. CP-015 command registry and CP-012 selection capability aggregation.
2. CP-021 evidence-safe action bindings and CP-025 generated icon manifest.
3. CP-022 dynamic icon cache and CP-024 building-icon dispatch.
4. CP-014 paging/submenus and removal of silent truncation.
5. CP-001, CP-002, CP-003, CP-004, CP-005, CP-008.
6. CP-009, CP-010, CP-011, CP-013.
7. CP-006, CP-007, CP-017, CP-018.
8. CP-019, CP-023, and CP-026 runtime evidence before exact visual parity.
9. CP-016 matrix tests throughout, not as a final cleanup.

## Definition of done

- Villager and cavalry selection produce intentionally different, tested
  command sets.
- Every implemented simulation action intended for player use has a discoverable
  panel path or an explicitly documented contextual-only rule.
- Command visibility is independent of selection order.
- No valid command disappears because of a fixed vector cap.
- Mouse and keyboard routes share eligibility and dispatch logic.
- All unit/building command sets are covered by table-driven tests.
- Every visible command has an evidence-bearing icon binding or explicit
  procedural fallback.
- Unit and technology buttons use exact DAT-derived frames.
- Action and building buttons are never labeled exact from visual guesses.
- Generated command-icon manifest passes bounds, citation, and coverage gates.
- Exact-original claims cite decoded data, decompiled control flow, or runtime
  captures; unresolved details remain labeled unproved.

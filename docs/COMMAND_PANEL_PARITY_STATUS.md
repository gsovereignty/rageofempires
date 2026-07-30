# Command-panel parity status

This document records the implemented reconstruction contract. It does not
upgrade unresolved original-game behavior to exact parity.

| Item | Reconstruction status | Contract |
|---|---|---|
| CP-001 | Implemented | Villagers open economic, military, and defensive construction pages; placement uses the existing replayable construction path. |
| CP-002 | Implemented | Eligible villagers expose Repair and enter the canonical contextual repair target mode. |
| CP-003 | Implemented | Monks and missionaries expose Convert, Heal, and state-dependent relic actions. |
| CP-004 | Implemented | Packed and deployed trebuchets expose opposite replayable transforms. |
| CP-005 | Implemented | Trade carts and trade cogs expose endpoint targeting. |
| CP-006 | Implemented | Land units expose embark targeting; transports expose state-dependent unload. |
| CP-007 | Implemented | Fishing ships expose age/civilization-filtered Fish Trap placement. |
| CP-008 | Implemented | Building research pages use simulation validation and replayable research commands. |
| CP-009 | Implemented for supported simulation roster | Every base trainable reconstruction unit, enabled unique unit, trade unit, naval unit, siege unit, and castle unit is generated from the building roster and checked through `queue_unit_at`. |
| CP-010 | Implemented | Rally, garrison, production, and research entries use explicit building capability predicates. |
| CP-011 | Implemented reconstruction rule | Visibility uses coarse unit-class eligibility; target acceptance remains the canonical exact simulation check. |
| CP-012 | Implemented | Capability aggregation is ID-sorted and independent of selection order; actions dispatch only through simulation validation. |
| CP-013 | Implemented | A fully garrisoned unit selection exposes no ordinary active commands. |
| CP-014 | Implemented | The grid is 5x3. Root, construction, production, and research overflow is deterministic and navigable; no insertion helper truncates commands. |
| CP-015 | Implemented shared activation route | Visible, enabled panel hotkeys enqueue the same button activation used by the mouse. Production/research slots receive deterministic reconstructed page hotkeys. Modifier-only legacy/debug commands remain separate because they are not panel entries. |
| CP-016 | Implemented capability matrix | Every `UnitKind` and `BuildingKind` is selected in a table-driven panel contract. Focused cases cover villager, monk, trade, both trebuchet states, fishing, transport, wall, town-center production/research, mixed selection order, overflow navigation, exact dynamic icons, and procedural fallbacks. Manifest coverage separately gates every `PanelCommand`. |
| CP-017 | Implemented | Status text covers gathering, building, repair, heal, conversion, relic, trade, fishing, packing, transport, attack, and movement states available in the model. |
| CP-018 | Contextual-only; original panel behavior unproved | Deletion remains an existing keyboard/context action. No panel button, confirmation rule, slot, or hotkey is claimed without the runtime evidence required by the parity manifest. |
| CP-019 | Evidence-blocked by design | Action/slot semantics remain `unproved`; the renderer uses labeled procedural fallback. |
| CP-021 | Implemented | Named actions cannot carry guessed exact archive bindings. |
| CP-022 | Implemented | Complete bounded unit and technology sheets are cached; corrupt/missing external archives fail to procedural rendering. |
| CP-023 | Explicit reconstruction fallback | Disabled modulation, pressed offset, bevel, and page treatment are documented as reconstructed, not original. |
| CP-024 | Evidence-blocked by design | Construction buttons carry a typed building payload but no guessed exact icon. Civilization/subtype sheet selection needs decoded evidence before binding. |
| CP-025 | Implemented | `generated/command_icon_manifest.json` covers every `PanelCommand`; the build gate rejects missing, duplicate, out-of-range, or unsupported exact claims. |
| CP-026 | Evidence-blocked by design | Exact original nesting, mixed-selection rules, hidden/disabled behavior, and hotkeys remain unproved. The reconstruction behavior above is deterministic and tested. |

## Icon contract

- Unit and technology entries use generated, evidence-bearing bindings.
- Action and building entries use procedural/text art unless an exact
  independently cited binding exists.
- `generated/command_icon_manifest.json` is the machine-readable authority.
- Pressed, selected, hover, disabled, and page chrome remain reconstruction
  behavior until original runtime evidence is captured.

## Verification

The clean-room Release configuration builds without parent-workspace input.
Its CTest suite, icon/manifest guards, self-containment guard, and macOS bundle
runtime smoke must all pass before this status can be considered current.

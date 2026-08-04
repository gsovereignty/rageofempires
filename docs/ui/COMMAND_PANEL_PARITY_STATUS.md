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
| CP-014 | Implemented | The grid is 5x3 with sparse per-command slots. Rendering, hover, mouse activation, and hotkey activation resolve the same slot. Root, construction, production, and research overflow remains deterministic and navigable. |
| CP-015 | Implemented shared activation route | Visible, enabled panel hotkeys enqueue the same button activation used by the mouse. Production/research slots receive deterministic reconstructed page hotkeys. Modifier-only legacy/debug commands remain separate because they are not panel entries. |
| CP-016 | Implemented capability matrix | Every `UnitKind` and `BuildingKind` is selected in a table-driven panel contract. Focused cases cover villager, monk, trade, both trebuchet states, fishing, transport, wall, town-center production/research, mixed selection order, overflow navigation, exact dynamic icons, and procedural fallbacks. Manifest coverage separately gates every `PanelCommand`. |
| CP-017 | Implemented | Status text covers gathering, building, repair, heal, conversion, relic, trade, fishing, packing, transport, attack, and movement states available in the model. |
| CP-018 | Implemented for Villager-only selection | Original runtime evidence proves Delete at slot 3 with `btncmd` frame 59. It dispatches the existing replayable entity-deletion path. Town Center runtime screenshots instead show frame 45 at slot 4 and Town Bell at slot 14; no skull Delete tile is shown. |
| CP-019 | Evidence-blocked by design | Action/slot semantics remain `unproved`; the renderer uses labeled procedural fallback. |
| CP-021 | Implemented | Named actions cannot carry guessed exact archive bindings. |
| CP-022 | Implemented | Complete bounded unit and technology sheets are cached; corrupt/missing external archives fail to procedural rendering. |
| CP-023 | Implemented from executable/archive | `btngame` 50751 frames 36/37 provide normal/pressed chrome, pressed icons move one pixel, hover retains normal chrome, and disabled controls are hidden. |
| CP-024 | Implemented | Construction buttons use exact DAT `button_icon` frames through executable-proved building-subtype dispatch. Installed architecture-sheet resources 50705–50708 are byte-identical; runtime uses canonical sheet 50706 and retains text fallback. |
| CP-025 | Implemented | `generated/command_icon_manifest.json` covers every `PanelCommand`; the build gate rejects missing, duplicate, out-of-range, or unsupported exact claims. |
| CP-026 | Evidence-blocked by design | Exact original nesting, mixed-selection rules, hidden/disabled behavior, and hotkeys remain unproved. The reconstruction behavior above is deterministic and tested. |
| CP-027 | Implemented | Town Center DAT positions are Villager 1, Loom 6, Wheelbarrow/Hand Cart 7, Town Watch/Patrol 8, and age-up 11. Production, research, and age-up commands render directly in sparse slots instead of proxy submenus. |
| CP-028 | Implemented for Villager-only selection | Original runtime screenshot proves economic 0, military 1, repair 2, delete 3, garrison 4, and stop 9 with exact `btncmd` frames; see `docs/evidence/VILLAGER_COMMAND_PANEL_EVIDENCE.md`. |
| CP-029 | Visual parity implemented; activation pending | Five original-HD runtime captures prove Town Center frame 45 at slot 4 and Town Bell at slot 14 with frame 49. Frame 45 remains semantically classified as a best-fit Ungarrison reconstruction and retains its replayable path. Rally/skull substitutes were removed. Town Bell is presently a visual command only. |

## Icon contract

- Unit and technology entries use generated, evidence-bearing bindings.
- Action entries use procedural/text art unless an exact independently cited
  binding exists. Building entries use exact DAT-indexed sheet frames.
- `generated/command_icon_manifest.json` is the machine-readable authority.
- Normal, pressed/selected, hover, and disabled command-control states follow
  `FUN_005c5e40`/`FUN_005c6050`; page semantics remain bounded by evidence.

## Verification

The clean-room Release configuration builds without parent-workspace input.
Its CTest suite, icon/manifest guards, self-containment guard, and macOS bundle
runtime smoke must all pass before this status can be considered current.

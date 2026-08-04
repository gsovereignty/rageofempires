# Documentation index

Documentation grouped by lifetime and purpose.

## Guides

Reader-facing setup, controls, architecture, content, and status:
[guide/](guide/).

- [Gameplay automation](guide/GAMEPLAY_AUTOMATION.md)
- [Automated full-match playthrough](guide/AUTOMATED_PLAYTHROUGH.md)

## Contracts

Durable reconstruction-native behavior:
[contracts/](contracts/).

- Match statistics
- Resignation and observer state
- Scenario editor
- State-file validation
- Trigger evaluation

## Fidelity

Bounded findings about commercial formats and behavior:
[fidelity/](fidelity/).

- [Movement presentation timing](fidelity/MOVEMENT_PRESENTATION_FIDELITY.md)
- [Main-menu fidelity](fidelity/MAIN_MENU_FIDELITY.md)

## Assets

- [HD and 1999 data-source manifest](assets/DATA_SOURCE_MANIFEST.md)

Sprite, technology, civilization, renderer, and asset-coverage mappings:
[assets/](assets/).

## Evidence

- [Market standing runtime evidence](evidence/MARKET_STANDING_RUNTIME_EVIDENCE.md)
  isolates fixed completed-Market frames from moving Trade Cart overlap.
- [Sheep player movement](evidence/SHEEP_PLAYER_MOVEMENT.md) records command,
  pathing, replay/save, SDL, and original-evidence findings.

Pinned static-analysis and observed-runtime evidence:
[evidence/](evidence/).

- [Villager gathering retry evidence](evidence/VILLAGER_GATHERING_RETRY_EVIDENCE.md)

## UI

Focused interaction and presentation contracts:
[ui/](ui/).

## Audits

Date-stamped snapshots that may become stale:
[audits/](audits/).

- [Decompiled gameplay and character-behavior audit](audits/2026-08-04-DECOMPILED-GAMEPLAY-BEHAVIOR-AUDIT.md)
- [Pointer-coordinate audit](audits/2026-08-04-pointer-coordinate-audit.md)

## Self-containment

[SELF_CONTAINMENT.md](SELF_CONTAINMENT.md) defines product resource and
runtime-isolation contract.

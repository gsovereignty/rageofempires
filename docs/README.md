# Documentation index

Documentation grouped by lifetime and purpose.

## Guides

Reader-facing setup, controls, architecture, content, and status:
[guide/](guide/).

- [Gameplay automation](guide/GAMEPLAY_AUTOMATION.md)
- [Automated full-match playthrough](guide/AUTOMATED_PLAYTHROUGH.md)

## Contracts

- [Minimap modes](contracts/MINIMAP_MODES.md)

- [Commercial multiplayer adapter ABI](contracts/COMMERCIAL_MULTIPLAYER_ADAPTER.md)
  defines opt-in licensed-service loading without SDK redistribution.

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
- [Civilization technology-tree fidelity](fidelity/TECHNOLOGY_TREE_FIDELITY.md)
- [Building body-state fidelity](fidelity/BUILDING_BODY_STATE_FIDELITY.md)
- [Fog-of-war rendering fidelity](fidelity/FOG_RENDERING_FIDELITY.md)

## Assets

- [HD and 1999 data-source manifest](assets/DATA_SOURCE_MANIFEST.md)

Sprite, technology, civilization, renderer, and asset-coverage mappings:
[assets/](assets/).

## Evidence

- [Tower damage presentation evidence](evidence/TOWER_DAMAGE_PRESENTATION_EVIDENCE.md)
  binds Watch/Guard/Keep bodies and damage roots across every civilization.
- [Supplied HD multiplayer protocol evidence](evidence/COMMERCIAL_MULTIPLAYER_PROTOCOL_EVIDENCE.md)
  pins recovered Steam P2P outer records and exact interoperability blockers.
- [Frontend game-mode evidence](evidence/FRONTEND_GAME_MODES_EVIDENCE.md)
  proves restored Learn to Play, Regicide, Death Match, and retired Zone
  contracts plus persistence and background SDL coverage.
- [Villager death presentation evidence](evidence/VILLAGER_DEATH_PRESENTATION_EVIDENCE.md)
  proves exact VMBAS_DN frames, hotspots, corpse hold, and unit/rubble split.
- [Market standing runtime evidence](evidence/MARKET_STANDING_RUNTIME_EVIDENCE.md)
  isolates fixed completed-Market frames from moving Trade Cart overlap.
- [Trade Cart selection portrait evidence](evidence/TRADE_CART_SELECTION_PORTRAIT_EVIDENCE.md)
  proves exact DAT/interface icon dispatch and background visual regression.
- [Naval selection portrait evidence](evidence/NAVAL_SELECTION_PORTRAIT_EVIDENCE.md)
  proves Galley-line and Transport Ship interface artwork in production HUD.
- [Sheep player movement](evidence/SHEEP_PLAYER_MOVEMENT.md) records command,
  pathing, replay/save, SDL, and original-evidence findings.

Pinned static-analysis and observed-runtime evidence:
[evidence/](evidence/).

- [Villager gathering retry evidence](evidence/VILLAGER_GATHERING_RETRY_EVIDENCE.md)

## UI

- [Minimap mode controls](ui/MINIMAP_MODES_UI.md)

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

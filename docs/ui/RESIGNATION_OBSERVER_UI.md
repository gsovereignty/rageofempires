# Resignation observer UI

State semantics:
[resignation and observer contract](../contracts/RESIGNATION_OBSERVER.md).

`Ctrl+Shift+R` or the visible `RESIGN` button submits the typed resignation
command through the same
single-player/replay or multiplayer lockstep command route as other gameplay
actions. After committed resignation, local controller becomes read-only.

Observer perspective:

- shows an `OBSERVER` badge;
- unlocks controller-aware full-map terrain, units, effects, projectiles,
  buildings, and minimap visibility only after controller state changes;
- preserves camera movement, zoom, minimap navigation, statistics, and other
  read-only overlays;
- disables command-panel buttons and rejects world commands through existing
  command validation;
- prevents opening or sending multiplayer chat.

Before resignation, rendering still uses the local player's normal fog and
exploration state. No observer visibility is inferred from match outcome
alone.

Proof combines authoritative `aoe_resignation_observer_tests` (lockstep
post-resign command/chat rejection, perspective boundary, replay, Save110)
with `observer_ui_sdl_smoke`. Visual capture:
`/tmp/aoe-observer-ui.png`.

# Selection and command panel

Selected units and buildings now use a compact information area plus a 5x3
command grid instead of the dense bottom hotkey sentence. Unit summaries show
name, current/maximum HP, activity, carried resource, and garrison state.
Building summaries show HP, construction or production progress, garrison
count, queue activity, and research status.

Buttons preserve their keyboard labels and add mouse activation, hover, and
disabled states. Unit actions include stop, attack-move, attack-ground,
patrol, guard, garrison guidance, stance cycling, and four formations.
Building actions include rally guidance, ungarrison, production cancellation,
and available training. Cancellation uses the existing command path, including
its tested refund behavior. Existing keyboard handling remains unchanged.

Villagers expose economic and military build pages. Military construction
opens a separate defenses page so no page exceeds 15 cells. Fishing ships
expose Fish Trap directly. Construction buttons carry `BuildingKind` and enter
the same placement/`ConstructBuildingCommand` path used by hotkeys.

No action-sheet frame identity has been proven. Command buttons therefore use
procedural beveled cells and the model leaves every archive icon ID empty.
Mapped archive HUD and portrait-frame assets are used only when they decode;
fallback rendering remains complete.

Proof:

- `command_panel_tests`: unit/building summaries, 5x3 bounds, training and
  disabled cancel states, and absence of guessed archive icon IDs.
- `/tmp/aoe-command-panel-exact.png`
- `/tmp/aoe-command-panel-fallback.png`

`AOE_COMMAND_PANEL=unit|building` selects deterministic capture subjects.

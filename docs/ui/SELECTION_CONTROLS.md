# Selection controls and control groups

- `Ctrl+1` through `Ctrl+9` replace a control group with the current owned
  unit or building selection. `Ctrl+Shift+number` extends it.
- `1` through `9` prune dead, invalid, foreign, and garrisoned IDs before
  recall. A second press within 400 ms centers the camera on the first unit or
  production building.
- Shift-click toggles one owned unit without disturbing the rest. Double-click
  selects the same owned type only where visible to the local player.
- Drag selection remains local-player-only and excludes garrisoned units.
  Minimap clicks and UI clicks never start a world-selection drag.
- `.` cycles idle Villagers and `,` cycles idle military; Shift selects all in
  the current idle category.

Groups are local UI state, not deterministic simulation state. They are not
written into project saves or multiplayer frames and are cleared when a save,
scenario, replay, or rematch replaces the simulation. Building groups keep the
production building ID and recall it while valid.

Pure model tests cover Shift toggling, double-click ownership/visibility,
drag candidate filtering, and invalid group cleanup. SDL smoke uses
`AOE_COMMAND_PANEL=unit AOE_SELECTION_PROOF=1`; proof capture is
`/tmp/aoe-selection-controls.png`.

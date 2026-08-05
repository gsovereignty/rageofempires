# Ordinary Match Setup

Ordinary Single Player random-map setup exposes all eight stable player-color
slots: blue, red, green, yellow, cyan, purple, gray, and orange. Blue begins as
the local human, red as a computer, and remaining slots as closed.

Setup keys:

- `S`: select next color slot.
- `P`: toggle selected non-local slot between Computer and Closed.
- `C`: cycle selected slot through all 18 playable civilizations.
- `T`: cycle No Team and teams 1 through 4.
- `D`: cycle shared computer difficulty.
- `V`: cycle Conquest, Wonder, and Relic victory.
- `Enter`: generate and start configured match.

Starting a match materializes occupied slots into `ScenarioRosterEntry`, emits
the complete directed diplomacy matrix, assigns each occupied slot a separated
map start, and creates its Town Center, villagers, scout, and nearby starting
resources. Same-team slots begin allied; other occupied slots begin enemies.
Closed slots have no controller, economy, start, or entities.

Scenario roster serialization, save games, multiplayer canonical settings,
eight-color rendering, runtime diplomacy, and roster victory evaluation retain
these stable slots after setup. Scenario-editor presentation remains separate
and is not claimed by this contract.

Original evidence boundary: recovered setup structures in
`AoK-HD-patched.c` carry eight indexed participants and numbered team values;
commercial player colors and scenario player records use stable indexed slots.
No decompiled or commercial payload is required at runtime.

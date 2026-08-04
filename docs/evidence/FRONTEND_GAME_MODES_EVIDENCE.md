# Frontend game-mode evidence

## Original boundaries

The supplied executable names Learn to Play, Regicide, Death Match, and Zone.
Supplied `Readme.rtf` names `http://www.zone.com/`, describes launching a
multiplayer match from the Zone, and therefore proves Zone was an external
matchmaking launch surface rather than a distinct simulation mode. That
service is retired; reconstruction presents its name, historical URL, retired
state, and supported direct-host/join alternative without issuing a network
request.

Live extraction from supplied VER 5.7 `empires2_x1_p1.dat` proves King record
434: 75 HP, speed 1.32, LOS 6, icon 48, standing graphic 1851 / SLP 1767,
walking graphic 1855 / SLP 1771, and dying graphic 1848 / SLP 1764. Decompiled
AI registration names `REGICIDE` and `regicide-game`. Supplied expansion
readme additionally documents Regicide map exceptions with no starting Castle
and maps with multiple Kings/Castles, proving King survival is mode state, not
ordinary Conquest elimination.

Death Match uses original 20,000 food, 20,000 wood, 10,000 gold, and 5,000
stone stockpiles, Imperial Age, and civilization-available post-Imperial
technologies. Normal selectable Conquest/Wonder/Relic victory remains active.

## Reconstruction contract

- Learn to Play launches a playable guided scenario with staged gather,
  building/army, and enemy-defeat objectives and messages.
- Regicide generates ten Villagers, one King, and one Castle per player,
  disables ordinary Conquest, and
  ends immediately when one King dies. Both Kings dying produces a draw.
- Death Match applies exact stockpiles and post-Imperial state before creating
  simulation.
- Zone opens a dedicated retired-service panel. It performs no request and
  directs players to supported Multiplayer host/join.

Mode state and King entity IDs persist in Scenario v67 and Save v115. Replay
uses unchanged deterministic commands against mode-bearing initial state.

## Verification

`frontend_game_modes_tests` covers mode setup, resources, technologies, King
identity and terminal outcome, tutorial triggers, Zone contract, Scenario and
Save round trips, and replay command application. `frontend_menu_tests` pins
all three formerly disabled routes. `frontend_menu_sdl_smoke` launches each
playable choice in dummy-video background mode and captures Zone presentation.

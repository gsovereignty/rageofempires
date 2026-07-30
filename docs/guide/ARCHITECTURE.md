# Architecture

- `GameMap`: terrain ownership and spatial validity.
- `Simulation`: deterministic rules, units, commands, economy, and ticks.
- `game_rules`: explicit balance assumptions with no hidden magic numbers.
- `ComputerPlayer`: isolated controller with no renderer or private-state access.
- `game_command`: typed commands plus deterministic recording and playback.
- `scenario`: validated text parser and simulation factory.

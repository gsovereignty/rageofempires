# Minimap modes contract

The local minimap mode is presentation state, never simulation or lockstep
state. Changing it cannot affect commands, visibility, hashes, replays, AI, or
multiplayer outcomes. `normal` is the migration default. Settings schema 4
persists the selected mode; schemas 1-3 migrate to `normal`.

All modes consume the same authoritative controller-visibility API. Observer
perspective sees the full map. Active players see owned objects, currently
visible enemy objects, allied objects supplied through Cartography, and
remembered explored buildings. Modes only filter that already-visible set.
Signals and camera viewport remain visible in every mode.

- Normal: terrain, resources, every visible unit, and every visible building.
- Combat: military/support units and non-economic buildings. Workers, trade,
  fishing, animals, Relics, Farms, camps, Markets, Docks, Fish Traps, and Town
  Centers are filtered.
- Economic: workers, trade/fishing units, animals, Relics, Town Centers,
  Farms, camps, Markets, Docks, Fish Traps, and highlighted wood, food, gold,
  stone, and fish terrain. Other units and buildings are filtered.

Statistics output follows mode: current score; unit kills/losses; or total
gathered resources. All eight player slots use stable player order and exact
recovered marker colors.

Evidence boundaries remain recorded in
[`MINIMAP_RUNTIME_EVIDENCE.md`](../evidence/MINIMAP_RUNTIME_EVIDENCE.md).
Mode names and Statistics behavior come from supplied manual; projection,
visibility masks, colors, signals, and viewport geometry come from read-only
decompiled executable evidence.

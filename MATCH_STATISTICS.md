# Match statistics contract

`Simulation::match_statistics()` returns a value snapshot suitable for score,
statistics, and timeline UI. It exposes one `PlayerStatistics` record for blue
and red, current live scores, and deterministic 100-tick timeline samples.

Counters update only when authoritative simulation transitions succeed:

- resource extraction credits gathered food, wood, gold, or stone;
- accepted tribute credits sender and receiver;
- entity creation, death, and destruction credit created/built, lost,
  killed, and razed totals;
- completed conversions, relic deposits, technologies, age advances, and
  Wonders credit their matching totals.

Scenario placements count as created or built because they enter simulation
through the same authoritative creation boundary. Neutral animals, relics,
and other non-player entities do not count as player units.

`MatchStatistics` contains no renderer state. Terrain/UI code may keep the
returned snapshot without holding simulation references. `current_score` is
calculated when the snapshot is requested; timeline scores are historical.

Current Save110 persists every cumulative counter, optional
Feudal/Castle/Imperial
completion tick, and timeline sample. Save104 and older files load with zero
statistics because those formats carried no reconstructable counter history.
Replay and multiplayer state hashes already serialize simulation saves, so
statistics participate in deterministic checkpoint comparison.

This reconstruction tracks two active players. Defeated player entities credit
the opposing active player, matching the current two-player simulation model.

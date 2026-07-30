# Match statistics and debrief

`F12` opens a read-only statistics snapshot during play. Completed matches
open the same screen automatically with Continue, Rematch, and Back to Menu
actions. Number keys select Economy, Military, Society, Technology, and
Timeline tabs.

All values come from `Simulation::match_statistics()`. Economy, military,
society, research, age timing, current score, and sampled score timeline are
shown for blue and red. Categories not tracked by the simulation, such as
largest army, map explored, villager high, and technology percentage, say
`UNAVAILABLE`; zero is reserved for tracked counters whose value is zero.
Wonder and relic countdown causes are named only when countdown state proves
them. Other outcomes explicitly say the exact cause is unavailable.

English tab, action, title, and unavailable labels belong to the validated
localization table. Panel and timeline graph are procedural: no matching
statistics-screen archive artwork has been proven.

Proof:

- `statistics_view_tests`: totals, tracked and unavailable rows, normalized
  graph points, and evidenced versus unavailable victory causes.
- `/tmp/aoe-statistics-economy.png`
- `/tmp/aoe-statistics-timeline.png`

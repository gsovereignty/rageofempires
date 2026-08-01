# Automated full-match playthrough

This guide gives an automation agent a repeatable way to start a supported
single-player game, develop the blue player, destroy the red computer player,
and prove victory. It targets gameplay testing, not an optimal competitive
build order.

## What “complete” means

Use a single-player Random Map game with **Conquest** victory. A successful run
ends only when the semantic state reports:

```json
{"outcome":"blue_victory"}
```

Destroying the enemy Town Center is not enough. Conquest counts red alive while
red owns any building or any non-animal, non-relic unit. Continue searching for
villagers, military units, houses, camps, farms, and other structures until the
outcome changes. Terminal outcomes freeze simulation updates and reject further
state-changing commands.

The native two-mission demonstration campaign exercises briefing, progression,
and debrief screens, but is not the preferred full-match test fixture: its maps
are presentation fixtures rather than balanced campaigns. Use Random Map for a
complete economy-to-victory playthrough. Use
`resources/foundations.scenario` only when fixed coordinates are more important
than random-map coverage.

## Automation boundary

Launch through `launch_game` with background mode enabled. It starts the game
without activating its window and with an isolated
`AOE_GAMEPLAY_TEST_API_DIR`. Reuse the returned
`automation_dir` in every semantic command.

Prefer these background-safe operations:

- `game_state`: tick, outcome, resources, population, capacity, idle count, and
  selected IDs;
- `list_units`: stable IDs, owners, kinds, positions, destinations, HP,
  gathering, and carried resources;
- `select_unit`: select one owned unit by ID;
- `issue_gather`: send one owned villager to one animal ID;
- `issue_move`: issue the normal context command to one owned unit and tile;
- `advance_simulation`: run a bounded number of deterministic ticks;
- `gameplay_macro`: batch several semantic commands;
- raw macro commands `observe`, `batch`, `quiet`, `list_buildings`, `train`,
  `construct`, `research`, `advance_age`, `attack_move_group`, `move_group`,
  market exchange, and `advance_until_idle` cover economy and combat without
  foreground input;
- `screenshot_window` with activation disabled: visual checkpoint without
  taking focus.

`gameplay_macro` also accepts `start_random_map`, `list_buildings`,
`select_building`, `select_building_at`, `select_building_kind`, `train`,
`construct`, `research`, `advance_age`, `market_buy`, `market_sell`, and
`attack_move`. These cover full match setup, economy, production, technology,
and combat without visible command-panel interaction. Use foreground UI tools
only when specifically testing real pointer or keyboard handling.
Never derive global pointer coordinates from Retina pixels; screenshots and
clicks must use logical, window-relative coordinates.

The file protocol behind the tools accepts:

```text
request-1	state
request-2	list_units
request-3	select 1
request-4	move 1 20 15
request-5	gather 1 7
request-6	advance 250
request-7	start_random_map 42
```

`advance` accepts 0 through 10,000 ticks. Responses echo the request ID. Treat
`{"ok":false,...}` or a missing response as a failed action, not progress.

## Stable test setup

1. Launch at main menu in background mode.
2. Send `start_random_map <seed>` through `gameplay_macro`. Default setup is:
   - map: **Arabia**;
   - size: **Maximum**;
   - civilization: **Britons**;
   - AI: **Easiest**;
   - victory: **Conquest**;
   - fixed seed recorded in test log.
3. Wait for semantic `state` response. Assert tick advances, outcome is
   `ongoing`, blue has at least one villager and one Town Center is visible in
   the window, and population does not exceed capacity.

Fixed seed makes failures reproducible. Arabia avoids water transport. Easiest
AI reduces strategy variance while still exercising red
gathering, construction, production, scouting, attacks, and rebuilding.

## Playthrough policy

Run in event-bounded decision cycles. Use `observe` once per cycle, `batch` for
independent actions, and `advance_until_idle` for known production or research.
Use 25-100 tick advances during combat. Use 250-1,000 tick jumps while queues or
construction are known safe. Large blind advances can let AI attack before
controller reacts.

### 1. Stabilize food and population

1. Call `list_units`. Record all blue villagers, scout, and nearby neutral or
   blue sheep IDs.
2. Send villagers to different sheep with `issue_gather`. If fewer animals than
   villagers exist, share targets.
3. Read Town Center ID from `observe`. Use `train ID villager` continuously
   while food and population room permit.
4. Before population reaches capacity, use
   `construct VILLAGER_ID house X Y` on clear ground near Town Center.
5. Keep at least half of villagers gathering food. Redirect idle villagers
   immediately; `idle_units > 0` is a decision trigger.

Pass checkpoint: population grew, no sustained idle villagers remain, food
recovers after each villager queue, and first house completed.

### 2. Establish wood and advance ages

1. Scout in expanding loops with `issue_move`; use several waypoints as separate
   commands only after prior destination is reached. Record enemy positions from
   sightings. `list_units` is a test oracle and may expose entities beyond human
   fog; do not confuse that with a fog-of-war UI assertion.
2. Move roughly one third of villagers onto forest tiles with `move`; normal
   context semantics begin tree gathering.
3. Build Lumber Camp near forest and Mill near berries with `construct`. Add
   houses before each capacity limit.
4. Keep creating villagers until economy has about 18-24 workers.
5. Use `advance_age TOWN_CENTER_ID` when normal resource and prerequisite rules
   allow it. Advance to Feudal, then Castle Age. Do not send every villager to
   construction; keep food and wood income running.

Pass checkpoint: Castle Age reached, at least 20 population capacity exists,
food/wood trend upward between purchases, and scout or another unit has found
red territory.

### 3. Build a low-risk army

Britons provide a simple ranged plan:

1. Build Barracks if required, then Archery Range, Blacksmith, and a second
   Archery Range when resources permit.
2. Queue archers continuously. Aim for 12 before attacking; 20 is safer.
3. Research ranged attack/range upgrades at Blacksmith when enabled. Add armor
   only after production remains funded.
4. Add two or three Spearmen from Barracks if red fields cavalry.
5. Keep producing villagers and houses. Send most new villagers to wood and
   gold; retain enough food for units and upgrades.
6. Set military rally points just behind the army, not inside enemy base.

Use command responses and `observe` as source of truth. Availability varies by
building, civilization, age, resources, and prerequisites. Rejected command
means refresh observation and satisfy missing game rule; repeated blind commands
are test failure noise.

Pass checkpoint: at least 12 combat units, positive food/wood/gold income,
population below cap, and army grouped outside enemy weapon range.

### 4. Attack and preserve production

1. Derive owned army IDs from `observe`.
2. Use `attack_move_group X Y ID...` toward a walkable tile at edge of red base.
   Attack-move is safer than plain movement because units engage visible threats
   en route.
3. Focus exposed military units first, then Town Center and production
   buildings. Keep ranged units behind melee screen and avoid fighting under
   Town Center with a very small army.
4. Every 50-100 ticks, poll state and unit HP. Retreat damaged survivors with
   semantic `issue_move` if army falls below roughly half its starting size.
5. Keep both ranges producing during combat. Send reinforcements to a safe rally
   tile, then merge and attack-move again.
6. If first attack fails, rebuild to a larger force. Do not feed single units
   into red base.

Pass checkpoint: red Town Center and main production buildings disappear,
blue retains working economy and at least one military production building.

### 5. Eliminate final enemy entities

After main base falls, poll `list_units` and visually sweep explored map.

1. Attack every surviving red unit reported by test oracle.
2. Search resource clusters and map edges for escaped villagers.
3. Search old red territory for houses, farms, camps, towers, and rebuilding.
   Semantic unit listing does not list buildings, so window/minimap sweep remains
   required.
4. Split army only after red can no longer defeat a small search group.
5. Advance 25 ticks and poll `game_state` after each final destruction.
6. Stop only on exact `blue_victory`. Any `red_victory`, `draw`, timeout, crash,
   or permanently `ongoing` result fails test.

## Controller loop

Use this decision order each cycle:

```text
poll state and units
if outcome != ongoing: finish and classify exact outcome
if population == capacity: build house
if idle villagers: assign food, wood, or gold
if queue affordable and production idle: queue villager or military unit
if under attack: group defenders and attack-move to threat
if army below threshold: keep gathering and producing
else: attack-move toward highest-priority known red position
advance 25..100 ticks
```

Record seed, tick, resources, population, unit IDs, issued action, response, and
outcome each cycle. Capture non-activating screenshots at game start, each age,
first attack, enemy Town Center destruction, and terminal outcome.

## Recovery and failure rules

- Command rejected: refresh `state` and `list_units`; entity may have died,
  ownership may differ, destination may be blocked, or match may be terminal.
- No semantic response: confirm process and current `automation_dir`; stale
  `ready` file does not prove a live responder. Relaunch instead of appending
  commands to a dead session.
- Villager stopped gathering: assign a new live target. Animal depletion and
  path blockage are normal game states.
- Population stuck: finish or add House; do not keep queuing units.
- Resources stuck: inspect idle/carried state, relocate blocked workers, and
  ensure drop-off buildings exist.
- Army ignores enemy: plain move was likely used. Select army and issue
  **Attack Move**, or context-click a visible enemy.
- Outcome stays ongoing after base destruction: remaining red entity exists.
  Continue unit-oracle and visual building sweep.
- UI geometry changed: reacquire a logical window screenshot and locate labels.
  Never retain absolute screen coordinates across launches or resolutions.

## Coverage assertions

A full automated run should prove all of these, not victory alone:

- menu and deterministic random-map setup;
- live semantic API correlation and bounded tick advancement;
- animal gathering, resource delivery, idle recovery, and population cap;
- villager training, construction, age advancement, military production, and
  at least one upgrade;
- scouting, pathfinding, attack-move, combat, building destruction, and AI
  opposition;
- exact terminal outcome plus frozen post-match tick/state behavior;
- background-safe operation except documented short UI action batches;
- screenshots and structured logs sufficient to reproduce failure from seed.

## Hybrid UI coverage

Full-match victory through semantic commands proves gameplay, not visible
controls. When run also targets UI, declare chosen UI checkpoints before play.
At each checkpoint, capture semantic before-state, use fresh logical window
screenshot, perform actual window-relative pointer or keyboard action, capture
visual after-state, then confirm authoritative result with `observe`.

Recommended real-UI checkpoints:

- menu navigation and deterministic match setup;
- first House placement;
- first villager or military-unit queue;
- one age advancement and one technology;
- army selection plus attack-move;
- minimap navigation or command;
- victory presentation and statistics transition.

Semantic commands may prepare each checkpoint and control waiting afterward.
They must not be cited as proof that corresponding visible control worked.
Tag logged assertions `UI`, `simulation`, or `UNTESTED UI` so later reviewers
cannot mistake fast semantic coverage for pointer, keyboard, or rendering
coverage. Detailed procedure lives in
[Gameplay automation](GAMEPLAY_AUTOMATION.md#testing-actual-ui).

Commercial decompiled evidence contains named Standard, Conquest, Time Limit,
Score, and Custom victory modes plus campaign/victory presentation paths. It
does not prove this reconstruction's exact thresholds or automation strategy.
Rules above therefore follow reconstruction runtime and tests; decompiled input
serves only as bounded reference evidence.

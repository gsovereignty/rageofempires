# Gameplay automation

Development builds expose an opt-in semantic test boundary when
`AOE_GAMEPLAY_TEST_API_DIR` names a writable directory. Normal launches do not
create or poll automation files.

The game creates these files inside the directory:

- `ready` after the live simulation can accept commands;
- `commands.jsonl` for request lines;
- `responses.jsonl` for correlated JSON responses.

## Agent quick start

Launch with `launch_game(background=true)`, keep returned `automation_dir`, and
create a match through visible menus. Once match starts, prefer
`gameplay_macro` for commands not exposed as typed desktop tools.

First observation:

```json
{
  "automation_dir": "/returned/by/launch_game",
  "commands": ["observe"]
}
```

One economy cycle:

```json
{
  "automation_dir": "/returned/by/launch_game",
  "commands": [
    "batch gather 1 70; gather 2 71; train 100 villager; advance 50",
    "observe"
  ]
}
```

`gameplay_macro` already reduces MCP round trips. Put related mutations inside
one protocol-level `batch` as well; batch suppresses intermediate state
serialization and returns one final state. Use standalone typed tools when they
already express desired action.

## Command reference

Arguments are whitespace-separated decimal IDs and tile coordinates. Kind and
technology names are lowercase with underscores.

| Command | Syntax | Result |
| --- | --- | --- |
| Observe everything | `observe` | State plus live units and all buildings |
| Compact success | `quiet COMMAND` | Only `{"ok":true}` on success |
| Batched transport | `batch COMMAND; COMMAND` | Ordered execution and one final state |
| Move group | `move_group X Y ID...` | Context-move each owned unit |
| Attack-move group | `attack_move_group X Y ID...` | Attack-move each owned unit |
| Wait for building | `advance_until_idle BUILDING MAX_TICKS` | Advance until queue and research idle |
| List buildings | `list_buildings` | IDs, ownership, health, queues, research |
| Train | `train BUILDING UNIT_KIND` | Queue one unit under normal rules |
| Construct | `construct VILLAGER BUILDING_KIND X Y` | Place normal foundation and assign builder |
| Research | `research BUILDING TECHNOLOGY` | Start normal technology research |
| Advance age | `advance_age TOWN_CENTER` | Start next-age research |
| Buy resource | `market_buy MARKET food|wood|stone` | Buy normal 100-unit lot |
| Sell resource | `market_sell MARKET food|wood|stone` | Sell normal 100-unit lot |

Existing commands remain `state`, `resources`, `idle_units`, `list_units`,
`select`, `move`, `attack_move`, `gather`, building-selection commands, and
`advance`.

Each command line contains a caller-chosen request ID, one tab, then a command:

```text
request-1	state
request-2	list_units
request-3	select 1
request-4	move 1 20 15
request-5	gather 1 7
request-6	advance 250
request-7	list_buildings
request-8	select_building 12
request-9	select_building_at 40 25
request-10	select_building_kind town_center
request-11	train 12 villager
request-12	advance_age 12
request-13	construct 1 house 20 15
request-14	research 14 fletching
request-15	observe
request-16	quiet move 1 22 18
request-17	batch train 12 villager; construct 1 house 20 15
request-18	move_group 30 20 1 2 3
request-19	attack_move_group 45 25 21 22 23
request-20	advance_until_idle 12 1000
```

Responses contain the same ID and a structured `result`. State reports tick,
outcome, resources, population, idle-unit count, and selected IDs. Unit reports
also expose stable entity IDs, owner, tile position, destination, health,
movement, gathering, and carried-resource state.

`observe` returns state, units, and buildings together. Prefix a mutating
command with `quiet` to return only success or failure, avoiding repeated state
serialization inside controller-side macros. `batch` accepts semicolon-separated
commands, executes them in order, stops on first failure, and returns one final
state with `completed_commands`.

Example observation response, abbreviated only for documentation:

```json
{
  "ok": true,
  "tick": 125,
  "outcome": "ongoing",
  "resources": {"wood": 210, "food": 340, "gold": 100, "stone": 200},
  "population": 8,
  "population_capacity": 10,
  "age": "Dark Age",
  "idle_units": 1,
  "units": [{"id": 1, "kind": "villager", "owner": "blue"}],
  "buildings": [{"id": 100, "kind": "town_center", "owner": "blue"}]
}
```

Successful batch response adds `"completed_commands":N`. Failed batch returns
first command error. Commands completed before that error stay applied; batch
is transport batching, not rollback transaction. Refresh with `observe` after
failure. Group commands likewise may have applied earlier IDs before a later ID
is rejected, so agents should build groups from fresh owned-unit observations.

Building reports expose stable IDs, kind, owner, origin tile, health,
completion, production-queue size, age-research progress, and technology-
research progress. Owned buildings
can be selected by ID, occupied tile, or kind. `train` and `advance_age` take an
owned building ID and call the same simulation production and research paths
as command-panel actions; normal cost, prerequisite, queue, age, and population
rules still apply.

`construct` takes an owned villager ID, building kind, and origin tile. It
executes the normal construction command, including civilization, age,
prerequisite, resource, builder-distance, terrain, footprint, and obstruction
checks. `research` takes an owned building ID and technology name and executes
normal technology availability, location, prerequisite, cost, and timing
checks. Building and technology names use lowercase underscore-separated names
returned by `list_buildings`, such as `archery_range` and `fletching`.

`advance` accepts 0 through 10,000 deterministic simulation ticks. Selection,
movement, gathering, construction, building selection, production, age
research, and technology research are restricted to active player's entities.
`move_group` and `attack_move_group` accept a destination followed by owned unit
IDs. `advance_until_idle` accepts an owned building ID and maximum tick count;
it stops when production, age research, and technology research are all idle,
the match ends, or the bound is reached.
Response includes `elapsed_ticks`; it does not include separate stop reason.
Infer stop cause from building queue/research fields and match outcome, then
continue decision loop. `MAX_TICKS` remains bounded to 0 through 10,000.

## Fast controller recipes

Observe once, decide once, mutate in batch, then advance:

```text
observe
batch gather 1 70; gather 2 71; train 100 villager; advance 50
observe
```

Wait for production or research without polling every few ticks:

```text
train 110 archer
advance_until_idle 110 1000
```

Move army with one semantic order:

```text
attack_move_group 45 25 201 202 203 204 205
advance 50
observe
```

Recommended loop:

```text
observe
if terminal outcome: stop
derive owned live IDs and building IDs from response
batch independent legal actions
advance_until_idle for safe production/research waits
advance 25..100 during combat
observe again
```

API is intended for local testing, agents, and CI. It is not a network service
and is inactive unless explicitly enabled.

Semantic commands do not focus the game window, move the pointer, or consume
keyboard input. They can therefore drive long gameplay checks while the Mac is
used normally. Use window-level screenshot automation for occasional visual
verification; reserve foreground mouse automation for UI flows that must prove
real pointer handling.

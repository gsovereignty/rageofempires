# Gameplay automation

Development builds expose an opt-in semantic test boundary when
`AOE_GAMEPLAY_TEST_API_DIR` names a writable directory. Normal launches do not
create or poll automation files.

The game creates these files inside the directory:

- `ready` after the live simulation can accept commands;
- `commands.jsonl` for request lines;
- `responses.jsonl` for correlated JSON responses.

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
```

Responses contain the same ID and a structured `result`. State reports tick,
outcome, resources, population, idle-unit count, and selected IDs. Unit reports
also expose stable entity IDs, owner, tile position, destination, health,
movement, gathering, and carried-resource state.

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
This
API is intended for local testing, agents, and CI. It is not a network service
and is inactive unless explicitly enabled.

Semantic commands do not focus the game window, move the pointer, or consume
keyboard input. They can therefore drive long gameplay checks while the Mac is
used normally. Use window-level screenshot automation for occasional visual
verification; reserve foreground mouse automation for UI flows that must prove
real pointer handling.

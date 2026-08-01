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
```

Responses contain the same ID and a structured `result`. State reports tick,
outcome, resources, population, idle-unit count, and selected IDs. Unit reports
also expose stable entity IDs, owner, tile position, destination, health,
movement, gathering, and carried-resource state.

`advance` accepts 0 through 10,000 deterministic simulation ticks. Selection,
movement, and gathering are restricted to the active player's entities. This
API is intended for local testing, agents, and CI. It is not a network service
and is inactive unless explicitly enabled.

Semantic commands do not focus the game window, move the pointer, or consume
keyboard input. They can therefore drive long gameplay checks while the Mac is
used normally. Use window-level screenshot automation for occasional visual
verification; reserve foreground mouse automation for UI flows that must prove
real pointer handling.

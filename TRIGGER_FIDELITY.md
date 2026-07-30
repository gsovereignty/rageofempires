# Native Trigger Fidelity

## Contract

Scenario64 replaces Scenario63's single quoted condition/effect pair with
bounded vectors: 1–256 conditions followed by 1–256 effects. Scenario63 and
older trigger records migrate their single pair into one-element vectors.
Conditions are evaluated with AND semantics against one tick snapshot.
Selected triggers then execute by priority; each trigger's effects execute in
stored order.

Effect application is transactional. All selected effects run on a simulation
copy. Invalid placement, missing object/objective/trigger, or unaffordable
tribute throws before any effect state reaches the authoritative simulation.
Activation/deactivation changes apply after trigger selection, so they affect
the next evaluation pass rather than changing the current snapshot.

## Represented grammar

Existing conditions/effects remain supported. Scenario64 adds:

- `object_hp ENTITY >= AMOUNT`
- `research PLAYER TECHNOLOGY`
- `tribute SOURCE TARGET RESOURCE AMOUNT`
- `remove_object ENTITY`
- `objective ID completed|incomplete|shown|hidden`
- `activate_trigger ID`
- `deactivate_trigger ID`

Strict scenarios reject empty vectors, more than 256 entries, unknown syntax,
neutral stateful condition/effect players, same-player tribute, negative
timers/durations/amounts, missing references, and invalid resources. Message
text remains quoted and bounded by scenario text validation.

One parsed semantic validator is shared by Scenario64 load/save,
`create_simulation`, Save110 load, runtime-state replacement, and transactional
trigger preflight. Scenario validation requires initial entity references to
exist. Save/runtime validation permits already-destroyed entities but still
requires positive entity IDs and existing objective/trigger targets. Scenario
save validates before opening its destination, so rejected strict syntax
cannot truncate or emit a file its loader rejects.

## Persistence

Save110 writes full ordered runtime condition/effect vectors, including
activation tick and fire counters. Save107 introduced vector records; Save106
and older single-pair records
migrate into one-element vectors. Replay64 remains unchanged because trigger
evaluation is deterministic simulation state, not a new replay command.
Save110 load rejects activation/last-fire ticks beyond the current tick and
the impossible state `fired_count == 0 && last_fired_tick != 0`.

Tests cover Scenario64 round-trip, Scenario63 migration through existing
fixtures, AND gating, ordered research/tribute/objective/message/activation
effects, Save110 round-trip, Replay64-equivalent deterministic updates, and
transactional rollback after an invalid later effect. Corruption tests mutate
Scenario64 neutral-player syntax, Save110 negative amounts, and Save110 fire
counters and require atomic rejection.

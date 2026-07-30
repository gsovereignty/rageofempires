# Scenario Trigger Fidelity Contract

## Scope

This document separates facts present in original scenario files from runtime
behavior chosen for this reconstruction. It covers trigger identity and order,
enabled/looping state, conditions, effects, objectives, and messages. It does
not claim byte-compatible scenario import.

## Evidence hierarchy

Use sources in this order:

1. **Supplied original scenario/campaign binaries.** These would be the most
   direct evidence for content and field combinations. The supplied-content
   audit found **0 scenario files and 0 campaign files**, so no `.scn`, `.scx`,
   or campaign trigger payload was available to decode. `DAT` and `DRS`
   archives are not scenario-trigger evidence.
2. **Original Microsoft documentation.** The original *Age of Empires II: The
   Age of Kings* manual documents that the editor controls victory conditions
   and hints, that scenarios can be play-tested, and that campaigns are ordered
   groups of scenarios (manual pp. 26–27). It does not specify trigger
   evaluation order, polling cadence, looping reset, or same-tick activation.
   [Original Microsoft manual](https://manuals.plus/m/bede02f48ca7b2aae252168379555520636b871dcd6c6fdba8ba2766ea0186e2.pdf)
3. **Classic SCX decoder source.** `genie-js/genie-scx` commit
   `4ad152ab2d3afb61dcf77de200ecacf3e8db1974` decodes the classic compressed
   trigger block. This is primary implementation evidence for persisted fields,
   but not for the closed-source game loop.
   [Classic trigger structures](https://github.com/genie-js/genie-scx/blob/4ad152ab2d3afb61dcf77de200ecacf3e8db1974/src/struct.js)
4. **Definitive Edition parser source.** `AoE2ScenarioParser` commit
   `b763e2e37006bedab50c7d349b3ce24b9c2497f6` corroborates the distinction
   between trigger identity and display order. Its manager explicitly says
   trigger display order is “NOT execution order.” DE-only fields are not
   evidence of AoC behavior.
   [Trigger model](https://github.com/KSneijders/AoE2ScenarioParser/blob/b763e2e37006bedab50c7d349b3ce24b9c2497f6/AoE2ScenarioParser/objects/data_objects/trigger.py)
   and
   [trigger manager](https://github.com/KSneijders/AoE2ScenarioParser/blob/b763e2e37006bedab50c7d349b3ce24b9c2497f6/AoE2ScenarioParser/objects/managers/trigger_manager.py).
5. **Current reconstruction model.** This is evidence only for our present
   behavior, not the original game.

Secondary guides and model-generated claims are excluded from the factual
baseline.

## What the formats prove

The classic decoder proves that a scenario can persist:

- a trigger array and a separate `triggerOrder` array;
- per-trigger `enabled`, `looping`, `startTime`, name, and description;
- objective membership and objective order;
- multiple conditions plus a `conditionOrder` array;
- multiple effects plus an `effectOrder` array;
- typed operands for timers, players, resources, technologies, units, areas,
  target triggers, text, sound, display time, and instruction-panel state.

These facts transfer cleanly: triggers need stable identity; enabled and
looping are independent state; conditions and effects are lists rather than
opaque single operations; objective text and transient effect messages are
different concepts.

The format does **not** prove:

- that `triggerOrder`, `conditionOrder`, or `effectOrder` controls execution
  rather than editor display;
- the polling frequency or trigger ordering used by the original engine;
- whether conditions short-circuit, or even the runtime interpretation of an
  empty condition list;
- whether effects from one trigger can make another fire in the same update;
- activation/deactivation timing, loop timer reset rules, or maximum fires per
  update;
- failure/atomicity behavior for invalid targets;
- multiplayer synchronization rules;
- automatic objective completion, localization fallback, message lifetime, or
  sound playback semantics.

Those remain `UNKNOWN`, not inferred original behavior.

## Current reconstruction model

`ScenarioTrigger` currently stores:

```text
id, priority, enabled, looping, condition:string, effect:string
```

The text format accepts one quoted condition string and one quoted effect
string. Saving sorts triggers by descending `priority`, then ascending `id`.
Scenario v62 compiles the represented grammar to typed runtime state; Scenario
v61 preserves unknown expressions as inert compatibility metadata.

Mapping:

| Current field | Evidence-backed interpretation | Required change |
|---|---|---|
| `id` | Stable reconstruction identity | Keep |
| `priority` | Reconstruction policy only; no classic priority field is proved | Keep, document ordering |
| `enabled` | Initial enabled state | Mutable runtime copy implemented |
| `looping` | Persisted mode flag | Relative loop timer implemented |
| `condition` | Serialized source text | One typed runtime condition implemented |
| `effect` | Serialized source text | One typed runtime effect implemented |
| `ScenarioObjective` | Reconstruction objective definition | Completion and hidden presentation implemented |

Do not reinterpret `priority` as classic `triggerOrder`.

## Implemented typed grammar

Keep the two quoted fields for text-format compatibility, but parse and
validate them during Scenario v62 load:

```ebnf
condition = "elapsed_ticks >= ", uint
          | ("unit_exists " | "unit_destroyed "), uint
          | ("building_exists " | "building_destroyed "), uint
          | "resource ", player, " ", resource, " >= ", uint
          | "area_presence ", player, " ", sint, " ", sint, " ",
            sint, " ", sint, " >= ", uint ;

effect    = "message player=", player, " ticks=", positive_uint,
             " text=", quoted
          | "complete_objective ", uint
          | "add_resource ", player, " ", resource, " ", sint
          | "create_unit ", unit, " ", player, " ", sint, " ", sint
          | "create_building ", building, " ", player, " ", sint, " ", sint
          | "diplomacy ", diplomacy
          | ("victory " | "defeat "), player ;
```

`player`, `resource`, units, buildings, and diplomacy use repository enums.
Entity and objective IDs resolve at simulation creation; areas and creation
positions are map-validated. Unknown keywords, unresolved references, malformed
quotes, invalid enums/ranges, and trailing tokens are v62 errors. The original
source strings remain available for scenario round trips.

Example:

```text
trigger 7 100 1 0 "elapsed_ticks >= 250" \
  "message player=blue ticks=150 text=\"Reinforcements arrived\""
```

## Deterministic reconstruction semantics

These are explicit project choices because original runtime semantics are
unknown:

1. At each simulation tick, snapshot trigger runtime state and all
   condition-visible game state.
2. Consider enabled triggers in descending `priority`, then ascending `id`.
3. Evaluate each trigger's represented condition against the snapshot.
4. Execute selected single effects in that same stable order.
5. Snapshot selection prevents same-tick condition cascades.
6. A non-looping trigger disables after one successful fire. A looping trigger
   remains enabled but fires at most once per tick. Its timer measures ticks
   since activation or its preceding fire.
7. Validate references and ranges at load so supported runtime effects are
   total. A programmer-invariant violation stops deterministically; it is not
   silently skipped.
8. Objective changes update objective state. `message` separately appends a
   transient, player-scoped UI message with an expiry tick.

Runtime state minimally contains `enabled`, `fired_count`,
`activation_tick`, and `last_fired_tick`. Save games must persist it. Replays
need not record internal firings when simulation inputs, scenario AST, tick
order, and save state are deterministic.

## Verification status

Tests cover:

- parse/save round trips preserving source strings and stable order;
- rejection of unknown syntax, bad enum names, overflow, and missing IDs;
- effects not changing the current tick's selected set;
- non-looping one-shot behavior and looping once-per-tick behavior;
- timer origin/reset after activation and loop firing;
- objective state versus transient message lifetime;
- save/load continuation producing the same next firing;
- v100/v101 migration into Save v103;
- replay equivalence for outcomes, objectives, messages, and trigger runtime.

Multiple conditions/effects, trigger activation effects, and the complete
commercial condition/effect set remain future reconstruction work.

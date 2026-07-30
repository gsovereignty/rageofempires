# Campaign Fidelity Contract

## Scope

This document defines a small reconstruction-native campaign manifest that
references the repository's existing `.scenario` files. It is not a parser,
importer, or byte-compatible replacement for proprietary `.cpn`, `.cpx`, or
`.cpx2` files.

## Evidence hierarchy

1. **Supplied campaign binaries.** These would be the strongest content
   evidence. `generated/live_content_assets_inventory.json` reports campaign
   `count: 0`, `bytes: 0`; therefore no supplied campaign container can be
   inspected.
2. **Original Microsoft manual.** The original *Age of Empires II: The Age of
   Kings* manual says a campaign is a series of scenarios, that each game must
   be won before progressing to the next, and that custom-campaign scenarios
   are played in the order selected in the Campaign Editor (manual pp. 16 and
   27). It also documents saving/restoring an active single-player game
   (p. 15), but does not describe persistence of campaign unlocks.
   [Original Microsoft manual](https://manuals.plus/m/bede02f48ca7b2aae252168379555520636b871dcd6c6fdba8ba2766ea0186e2.pdf)
3. **Commit-pinned open-source decoder.**
   `bangbang93/aoe2-campaign-parser` commit
   `232a568fd644cd7e32eab5cfba5f7587925f1451` parses classic campaign
   containers. Its `CpxFile` reads a four-byte signature, optional
   version-two integers, a fixed campaign name, scenario count, then one
   ordered record per embedded scenario containing byte length, byte offset,
   scenario name, scenario filename, and the scenario payload.
   [Campaign decoder](https://github.com/bangbang93/aoe2-campaign-parser/blob/232a568fd644cd7e32eab5cfba5f7587925f1451/src/cpx-file.ts)
4. **Current reconstruction.** `.scenario` parsing and `MatchOutcome` behavior
   describe this project only, not the original engine.

The decoder README says its parsing logic was reverse engineered. Treat its
code as direct evidence of what that implementation reads and writes, not as
an official format specification and not as evidence of closed-source runtime
behavior.

## Transferable facts

Evidence supports:

- a campaign has a name;
- it contains an ordered sequence of scenarios;
- a classic campaign container embeds complete scenario byte payloads;
- each entry carries a display-style scenario name and a filename;
- the container uses explicit byte lengths and offsets;
- normal campaign progression is linear;
- winning the current scenario permits progression to the next.

Evidence does **not** support:

- a campaign-description field in the classic container;
- arbitrary branching, prerequisites, optional scenarios, or multiple endings;
- a persisted unlock table inside the campaign container;
- exact behavior after defeat, resignation, replay, or loading an old save;
- whether completion is stored globally, per profile, or only in save games;
- how edited/reordered campaign content invalidates prior progress;
- the interpretation of the version-two unknown integers;
- original filename normalization or duplicate-name behavior;
- original automatic-advance timing and presentation.

Unsupported claims remain `UNKNOWN`. They are not filled from secondary guides
or model inference.

## Reconstruction-native manifest

Use a text manifest separate from scenario files:

```text
aoe-campaign 1
id "reconstruction-learning"
name "Reconstruction Learning Campaign"
description "Optional reconstruction-authored text."
human-player blue
scenario 1 "resources/intro.scenario" "A New Beginning"
scenario 2 "resources/counterattack.scenario" "Counterattack"
```

Grammar:

```ebnf
manifest       = header, newline,
                 id, newline,
                 name, newline,
                 [ description, newline ],
                 human_player, newline,
                 scenario, newline, { scenario, newline } ;
header         = "aoe-campaign 1" ;
id             = "id ", quoted_nonempty ;
name           = "name ", quoted_nonempty ;
description    = "description ", quoted ;
human_player   = "human-player ", player ;
scenario       = "scenario ", positive_id, " ",
                 quoted_path, " ", quoted_nonempty ;
player         = "blue" | "red" ;
```

Rules:

- Campaign IDs are stable ASCII identifiers and unique in the loaded catalog.
- Scenario IDs are positive, unique, and must increase in file order. File
  order is play order.
- Paths are UTF-8, relative to the campaign manifest directory, and must end
  in `.scenario`.
- Reject absolute paths, empty path segments, `.`/`..` segments, and paths
  whose normalized resolution leaves the manifest directory.
- Resolve and validate every referenced scenario before campaign play starts.
- Reject duplicate resolved paths. A campaign may deliberately reuse content
  later only after the manifest version adds an explicit reuse mechanism.
- `description` is reconstruction metadata. Do not present it as a decoded
  classic campaign field.
- Do not embed scenario bytes. Existing repository `.scenario` files remain
  independently editable and testable.
- Do not accept proprietary campaign extensions through this loader.

The bounded loader result should be:

```cpp
struct CampaignScenarioEntry {
    int id;
    std::filesystem::path path;
    std::string name;
};

struct Campaign {
    std::string id;
    std::string name;
    std::string description;
    Player human_player;
    std::vector<CampaignScenarioEntry> scenarios;
    std::string manifest_digest;
};
```

`manifest_digest` is a deterministic digest of the normalized manifest plus
the normalized bytes of every referenced `.scenario` file. Exact digest
algorithm is a repository choice and must be versioned.

## Minimum progression contract

This is reconstruction behavior, selected because it is small and matches the
manual's proved linear rule:

1. A new campaign unlocks only its first scenario.
2. Starting an unlocked entry creates a normal simulation from its referenced
   `.scenario`.
3. Only a terminal victory for `human-player` marks the entry completed.
   Defeat, resignation, abort, or nonterminal save leaves completion unchanged.
4. Completing entry `N` unlocks entry `N + 1`. The final victory marks the
   campaign completed.
5. Already completed entries remain replayable. Replaying them cannot revoke
   progress.
6. Completing entries out of order is impossible through the campaign API.
7. Progress changes only after the terminal match result is committed. UI
   messages or objective text never advance a campaign.

This contract is implemented on top of stable terminal match results. The SDL
flow commits victory progress once and presents the newly unlocked mission;
defeat leaves progress unchanged.

## Persistence

Keep campaign progress separate from an in-scenario save:

```text
aoe-campaign-progress 1
campaign-id "reconstruction-learning"
manifest-digest "<versioned digest>"
completed 1
unlocked 2
```

For the minimum linear model, persist:

- format version;
- campaign ID;
- manifest digest;
- sorted completed scenario IDs;
- highest unlocked scenario ID.

Write progress atomically after victory. On load, validate that completed IDs
form a contiguous prefix and `highest unlocked` is either the first entry or
exactly one entry after that prefix. Reject inconsistent state.

Current Save v109 persists complete simulation/trigger state, but does not embed
campaign ID, scenario ID, or manifest digest. Binding an arbitrary active save
back to campaign progress therefore remains outside campaign format v1.

If the manifest digest differs, do not silently remap progress by position.
Report stale progress and allow an explicit fresh start. Migration policy is
outside version 1.

The original manual proves that games can be saved and restored. It does not
prove this campaign-progress representation; this is a reconstruction
reliability contract.

## Verification status

Tests cover:

- parse/save canonical manifest round trip;
- stable ordered scenario loading;
- duplicate IDs, paths, missing files, wrong extensions, and malformed quotes;
- absolute paths, traversal segments, and resolution outside the manifest
  directory;
- invalid `.scenario` propagation before play;
- first-entry-only initial unlock;
- victory-only advancement and no advancement on defeat/abort;
- replay of completed entries without progress regression;
- final campaign completion;
- atomic progress replacement;
- rejection of noncontiguous or digest-mismatched progress;
- content digest changes and stale-progress reporting;
- duplicate singleton progress fields and invalid programmatic manifests.

Active-save campaign binding and original briefing/cinematic behavior remain
future work.

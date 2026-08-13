---
name: crush-browser-multiplayer-bugs
description: Orchestrate evidence-driven bug discovery across this reconstruction's packaged two-player browser Nostr gameplay. Use audit-browser-multiplayer-gameplay to play both sides and surface gameplay, rendering, motion, animation, asset, relay, and terminal candidates; invoke diagnose-gameplay-sync only for reproduced synchronization-class failures; apply crush-bugs confirmation and root-cause standards; document and log only reproducible bugs with precise proved root causes. Use when the user wants a broad multiplayer bug hunt with strict false-positive control, root-cause documentation, and confirmed-only bug logging.
---

# Crush Browser Multiplayer Bugs

Audit both sides of real packaged Nostr multiplayer, localize failures, and
produce a confirmed-only bug ledger. Default to diagnosis and documentation;
do not fix product code unless the user explicitly requests fixes.

## Load component contracts

Read these installed skills completely before acting:

1. `audit-browser-multiplayer-gameplay`
2. `diagnose-gameplay-sync`
3. `crush-bugs`, including `references/bug-discovery.md` and
   `references/evidence-ledger.md`

Apply this skill as orchestrator. Component instructions remain binding unless
this skill narrows scope. In particular, use multiplayer audit for discovery,
sync diagnosis only after its trigger, and crush-bugs for epistemic standards.

## Declare durable destinations

Before launching any audit, declare and create:

- raw evidence:
  `artifacts/browser-multiplayer-audits/<UTC-run-id>/`;
- consolidated report:
  `docs/audits/<YYYY-MM-DD>-BROWSER-MULTIPLAYER-BUG-HUNT.md`;
- confirmed bug log destination under `../todo/`, using repository convention.

Record candidate, rejected, blocked, and confirmed ledgers in durable audit
artifacts. Never use console-only or temporary evidence as final proof. Do not
write a candidate to `../todo/` until its precise root cause is proved.

## Phase 1: Discover through real gameplay

Run `audit-browser-multiplayer-gameplay` against the packaged production build:

- two independent browser profiles and ephemeral identities;
- ordinary public relays and visible production controls;
- both players active through stable gameplay;
- economy, construction, research, production, scouting, combat, destruction,
  relay recovery, and natural victory;
- synchronized state, frame sequences, motion telemetry, sprite provenance,
  whole-screen captures, and terminal evidence.

Preserve first-bad and last-good state immediately. Continue independent audit
phases when safe so one candidate does not erase other coverage. Never convert
infrastructure, harness, relay outage, missing instrumentation, black capture,
or ambiguous visual evidence into a product bug.

For every candidate record:

- location or affected subsystem;
- exact trigger and production reachability;
- expected and actual behavior;
- user-visible impact;
- first failing tick/frame/milestone;
- host and join state/hash/sequence evidence;
- reproduction seed, actions, relays, viewport, and artifact paths;
- current status: `CANDIDATE`, `NEEDS_PROOF`, `REJECTED`, or `BLOCKED`.

## Phase 2: Route synchronization candidates

Invoke `diagnose-gameplay-sync` only when discovery reproduces one of:

- divergent state hashes or canonical state;
- missing, delayed, duplicated, or unauthorized commands;
- unequal ticks, active-player ownership, or terminal state;
- relay publication, backfill, quorum, recovery, or WebSocket replication
  failure;
- differing live views whose durable-state relationship must be classified.

Give diagnosis raw host/join artifacts and shortest known action prefix. Trace
first failed boundary in production order: input and ownership, command
construction, signing/publication, relay acceptance, subscription/deduplication,
lockstep receive, simulation application, serialization/hash, presentation,
and recovery.

Keep `HARNESS_FAILURE`, expected `TRANSPORT_STALL`, transient convergent drift,
and external relay failure out of product bug log. Return diagnosis evidence to
same candidate record; do not create duplicate findings.

## Phase 3: Confirm with crush-bugs rules

For each candidate, apply crush-bugs confirmation independently and
sequentially, respecting repository one-live-subagent limit. A bug is
`CONFIRMED` only when all exist:

1. deterministic or repeated production-path reproduction;
2. violated repository, gameplay, protocol, visual, or original-fidelity
   contract;
3. concrete product harm;
4. exact reachable source/data path;
5. precise root cause established by direct code and data evidence;
6. independent review verdict `CONFIRMED`;
7. nonduplicate allocation to one root-cause finding.

If root cause remains undetermined, status stays `NEEDS_PROOF` or `BLOCKED`.
Never log it as confirmed. Reject intended behavior, stale coordinates,
automation mistakes, unsupported expectations, relay incidents, and duplicate
manifestations while preserving useful evidence.

Consult `../decompiled/` and original assets read-only for gameplay or visual
fidelity claims. When references conflict, mark expectation unresolved rather
than choosing silently.

Apply repository five-field cybersecurity proof before any security label:
`BOUNDARY`, `SOURCE`, `PATH`, `HARM`, and `CONTRACT`. Missing any field means
`MISCLASSIFIED`; retain any real product bug under its proved category and
continue.

## Phase 4: Minimize and log confirmed bugs

Minimize each confirmed reproduction to shortest valid action/fault prefix.
Deduplicate by root cause. Only then write bug log entry containing:

- title, classification, severity, and affected production workflow;
- precise root cause with functions, files, and line numbers;
- exact reproduction and first failing tick/frame;
- expected versus actual behavior and impact;
- host/join hashes, sequences, entity IDs, and relay evidence as applicable;
- screenshots/frame sequences and provenance/original evidence for visuals;
- artifact paths and duplicate-group ID;
- independent confirmation verdict.

Do not write hypotheses, remediation guesses, or unproved security language.
Do not call bugs fixed: this workflow diagnoses and logs unless user separately
authorized implementation and full production-fix gates pass.

## Report verdict

Write consolidated report with coverage matrices, all candidate dispositions,
confirmed root-cause ledger, rejected/blocked summaries, sync-diagnosis handoffs,
cleanup, and exact artifact paths.

Use:

- `PROBLEMS FOUND`: at least one confirmed product bug;
- `PASS`: required audit completed and no confirmed bugs remain;
- `BLOCKED`: required coverage could not be judged.

Report separate counts for candidates, confirmed bugs, rejected candidates,
blocked cases, infrastructure incidents, and harness failures. Never count
blocked or untested cases as passes.

Close both browsers, drivers, and owned server; release ports and verify file
descriptors return to baseline. If tracked audit files changed, follow repository
commit rules. Do not run game builds/tests for skill-only or documentation-only
changes.

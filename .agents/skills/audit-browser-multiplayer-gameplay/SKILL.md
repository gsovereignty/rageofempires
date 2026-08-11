---
name: audit-browser-multiplayer-gameplay
description: Audit this reconstruction's packaged two-player browser multiplayer for deterministic gameplay defects and production visual defects, including desync, missing or duplicated commands, relay recovery failures, teleporting or jittering units, broken interpolation, wrong animation state or facing, procedural or placeholder fallbacks, missing or incorrect legacy sprites, layering, terrain overlap, HUD, minimap, and terminal presentation. Use for browser multiplayer gameplay fuzzing, long two-player browser sessions, frame-sequence motion review, sprite-provenance verification, synchronized cross-client captures, or combined gameplay-and-visual Nostr audits.
---

# Audit browser multiplayer gameplay

Audit one real packaged match through two independent browser instances and
three independent oracles: lockstep state, temporal motion, and production
render provenance. Never infer visual correctness from matching state hashes or
gameplay correctness from matching screenshots.

Read repository `AGENTS.md` files first. Preserve unrelated changes. Consult
`../decompiled/` and original assets read-only when establishing fidelity.

## Respect scope

- Treat an audit request as read-only diagnosis plus audit artifacts and report.
- Do not fix product code or add production instrumentation unless the current
  user explicitly requests implementation or fixes.
- Existing read-only diagnostics, capture hooks, and audit tooling are valid.
  Never mutate simulation, transport, render state, identity, or relay state
  through hidden test hooks while claiming production-path coverage.
- Drive claimed gameplay actions through ordinary visible canvas, keyboard,
  lobby, HUD, and relay controls.
- Use semantic or direct state mutation only for separately labeled fixture
  setup. Never cite it as UI or production multiplayer proof.
- Treat every suspected defect as unverified until deterministically reproduced
  with direct state, frame, asset, or original-reference evidence.

## Own the runtime

Use one audit agent and at most two live game browser instances: one host and
one joiner. Do not run another game, visual audit, or gameplay subagent in
parallel. Use independent temporary Chrome profiles and distinct ephemeral
Nostr identities. Record both public keys; never inspect or persist private key
material.

Before launch:

1. Confirm no stale audit-owned browser, driver, server, or game process.
2. Record browser, driver, packaged build, viewport, device scale, and relay
   versions or URLs.
3. Record soft file-descriptor limit and current use. Do not start at or above
   half the limit.
4. Create one unique artifact directory under
   `artifacts/browser-multiplayer-audits/<UTC-run-id>/`.
5. Keep the same two browser processes for the run. Close screenshots, videos,
   logs, and command sessions promptly.
6. On exit, quit both drivers, stop the owned server, reap children, verify
   ports are released, and confirm descriptor use returns to baseline.

Use three ordinary public relays for production acceptance. Use a controlled
relay or deterministic transport only for high-volume fault discovery, and
label that coverage non-public. Never substitute a local-relay pass for public
relay acceptance.

## Build the run ledger

Record before first action:

- packaged artifact and source commit;
- host and join public keys and assigned slots;
- match reference, deterministic map seed, scenario/rules digest, and lobby
  revision;
- relay list and quorum;
- action-generation seed and transport-fault seed when randomized;
- logical canvas size, CSS size, backing-store size, device scale, camera, and
  capture cadence;
- enabled production asset-loader flags and provenance diagnostics;
- exact assertions that each test phase must satisfy.

Reject a run with shared identity, mismatched canonical lobby, mock transport,
disabled production assets, frontend mistaken for gameplay, black warm-up
frames, or ambiguous browser/window ownership.

## Execute audit phases

Read requested targets before launch. Always run the stable-network baseline
needed to validate the two-client oracle, then run every phase relevant to the
targets. Run all phases for broad, exhaustive, or unspecified audits. Do not
silently expand a focused motion, sprite, or relay request into seeded long
matches; record omitted phases as out of scope. Run selected phases in the
order below. Preserve the first failing state before continuing.

### 1. Stable-network baseline

Host and join through visible launch UI, ready both players, start visibly, and
wait for equal active ticks. Make both players issue distinct non-empty world
commands through real input. Cover at minimum:

- selection and movement from each client;
- simultaneous opposing commands in one lockstep interval;
- gathering or construction;
- unit production or research;
- attack-move, direct combat, damage, death, and destruction;
- chat and signal from both relevant audiences;
- host speed and pause/resume proposals;
- terminal result and frozen post-terminal tick.

Do not accept a run where only one player issues world commands.

### 2. Temporal motion cases

Capture correlated frame bursts before, during, and after each meaningful
transition. Use the smallest practical interval that exposes frame progression;
prefer every displayed frame for short movement/combat bursts. Record frame
number, wall time, simulation tick, entity ID, simulation position,
destination, render position, moving flag, action, facing, animation ID/frame,
camera, and sprite provenance when available.

Exercise:

- every reachable movement direction and direction change;
- single-unit and formation movement;
- stop, resume, collision, obstruction, and arrival;
- gather, carry, deposit, construct, repair, and work cycles;
- melee release, projectile launch/flight/impact, recoil, damage, death, decay,
  and rubble;
- elevation, terrain boundary, viewport edge, occlusion, and camera movement;
- relay suspension and restoration while units are moving or fighting.

Flag candidates for:

- displacement beyond the unit's permitted speed;
- unexplained reversal, oscillation, overshoot, orbit, or teleport;
- render stall while authoritative position advances;
- render jump while authoritative motion remains continuous;
- facing inconsistent with motion or rapid unexplained facing flips;
- animation reset, skipped/reversed frame order, frozen loop, foot sliding, or
  gliding with an idle sprite;
- animation continuing while the synchronized session is suspended;
- client-specific motion when authoritative states agree.

Account for configured tick cadence, interpolation, path steps, speed changes,
camera motion, and legal command changes before confirming a defect. A single
large pixel delta is not proof.

### 3. Sprite and fallback cases

For every visible entity and effect, correlate rendered output with expected
production asset source. Prefer renderer-provided provenance containing entity,
legacy asset ID, animation, frame, facing, flip, palette, scale, anchor,
shadow, layers, and fallback reason.

Confirm as bugs when reproduced in production:

- procedural, placeholder, synthetic, debug, missing-asset, or fallback draw
  where a real legacy asset is expected;
- wrong unit/building/resource, civilization, age, ownership palette, frame,
  facing, scale, anchor, transparency, shadow, or multipart layer;
- terrain painted over opaque sprite pixels;
- invalid occlusion, clipping, selection outline, projectile/effect ordering,
  construction/damage stage, death, decay, or rubble;
- one client choosing different visual assets for identical authoritative
  state.

If provenance is unavailable, combine source-path inspection, loaded-asset
diagnostics, transparent sprite isolation, and correlated frame evidence. Mark
the case `blocked`, not passed, when real-versus-fallback origin cannot be
proved.

Use `tools/capture_visual_overlap.py` and the sprite review tools only when
their scenario/capture contract matches the case. Do not silently replace the
live two-browser frame with a different scenario. Produce one batch review
page when isolated sprite candidates require human decisions; accept only
`bug`, `intentional`, or `uncertain`, and never count `uncertain` as pass.

### 4. Deterministic relay chaos

During active movement and combat:

1. Disconnect one relay through the production `Manage relay connections`
   disclosure and prove equal continued progress.
2. Disconnect another and prove both stop at the same tick before further
   turns execute.
3. Attempt ordinary player input while suspended and record accepted/rejected
   presentation without assuming intended semantics.
4. Restore a relay visibly; require connection, ready state, EOSE, exact-event
   backfill, contiguous sender sequences, and equal resumed state.
5. Restore all relays and continue combat through terminal result.

In controlled-transport runs also exercise deterministic duplication,
reordering, delay, stale control stages, and page freeze/resume. Record exact
fault schedule. Do not inject uncontrolled random faults that cannot replay.

### 5. Seeded long matches

Run bounded state-driven browser players across recorded seeds. Derive actions
from fresh observations but execute them through real UI. Reacquire targets
after camera, viewport, selection, or panel changes; never retain stale screen
coordinates.

Bias action generation toward concurrency and transitions rather than idle
time. Include economy, construction, age/technology, army production,
formations, scouting, combat, resource exhaustion, population cap, building
destruction, and terminal flow. Stop on first invariant failure and preserve
the exact action prefix.

## Apply three independent oracles

### Lockstep gameplay oracle

At every committed turn require both clients to agree on:

- current tick and session phase;
- deterministic state hash;
- sender sequences and missing ranges;
- entity IDs, ownership, position, destination, health, action, and targets;
- resources, population, queues, research, controller states, and outcome.

Flag duplicate execution, skipped command, unauthorized command, progress
below quorum, divergent outcome, or post-terminal advancement immediately.

### Temporal render oracle

Analyze ordered frame sequences, not isolated screenshots. Normalize for
camera and viewport before comparing clients. Distinguish simulation motion
from render interpolation:

| Evidence | Classification |
|---|---|
| Hash or authoritative state diverges | gameplay/lockstep candidate |
| Authoritative motion jumps identically | movement/path candidate |
| Authoritative motion smooth, render jumps | interpolation/render candidate |
| Only one client renders incorrectly | client/render-state candidate |
| Motion correct, frames/facing wrong | animation/asset candidate |

### Production visual oracle

Predict expected presentation before judgment using authoritative state,
original assets/metadata, decompiled evidence, and durable fidelity contracts.
Compare synchronized whole frames for HUD, minimap, terrain, effects, and
terminal screens; compare transparent isolated RGBA for sprite identity,
palette, anchor, alpha, and overlap. Mask only proved local-only regions such
as cursor or local identity. Never use broad tolerance to hide entity defects.

## Confirm and minimize findings

For each candidate:

1. Preserve first-bad and last-good ticks/frames immediately.
2. Reproduce with the same build, fresh-distinct-ephemeral identity strategy,
   map/action/fault seeds, viewport, and relay mode. Do not reuse or preserve
   private keys merely to reproduce the same public keys.
3. Follow direct source and data evidence to precise root cause. If evidence is
   insufficient, record `undetermined`; never speculate.
4. Remove preceding actions and faults while reproduction remains, producing
   the shortest valid action prefix.
5. Deduplicate manifestations sharing one root cause.
6. Apply repository cybersecurity classification rules; visual corruption,
   crashes, hangs, or malformed public events alone do not prove security
   relevance.

A confirmed finding must contain:

- classification and first failing milestone;
- exact build, seed, pubkeys, relays, viewport, and reproduction actions;
- expected versus actual gameplay or presentation;
- first divergent tick/frame and affected entity IDs;
- both clients' state/hash/sequence evidence;
- correlated screenshots or frame sequence;
- asset/provenance and original-reference evidence when visual;
- precise root cause or explicit `undetermined`;
- duplicate-group ID and affected coverage cells.

Infrastructure failures, relay outages, black frames, stale WebDriver nodes,
ambiguous captures, unsupported provenance, hypotheses, and uncertain human
decisions are not product bugs.

When durable reconstruction contracts, original assets/metadata, and
decompiled evidence conflict, record each source and mark the expectation
`unresolved`. Never choose a preferred source silently or confirm a fidelity
bug from a disputed expectation.

## Preserve evidence

Store within the run directory:

```text
run.json
actions.jsonl
transport.jsonl
states/host.jsonl
states/join.jsonl
frames/host/
frames/join/
motion.json
sprite-provenance.jsonl
screenshots/
console-host.json
console-join.json
first-failure.json
```

Keep artifacts untracked unless explicitly requested. Never place private key
material in evidence. Ensure every frame filename maps to browser, tick, frame,
camera, and action ledger entry.

## Report completion

Write one dated report under `docs/audits/` containing:

- scope, build, identities, relay modes, and run configuration;
- stable, motion, sprite, chaos, and long-match coverage matrices;
- exact passes, confirmed bugs, blocked cases, and untested cases;
- deduplicated findings and reproduction/evidence paths;
- per-client tick/hash/sequence and terminal ledger;
- asset-source totals and procedural/fallback totals;
- motion case/frame totals and thresholds used;
- public-relay versus controlled-transport separation;
- remaining instrumentation and fidelity gaps;
- cleanup confirmation.

Use verdict `PROBLEMS FOUND` when any confirmed product defect exists,
`BLOCKED` when required coverage cannot be judged, otherwise `PASS`. Do not
erase historical findings from the verdict merely because a later build is
corrected.

If tracked files changed, follow repository commit rules. Run `make` before a
completion commit only when game code changed; do not compile or test when only
skill or documentation files changed. Stage only audit-related files and
create a focused commit before reporting completion.

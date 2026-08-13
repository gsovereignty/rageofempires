# Browser multiplayer bug hunt — 2026-08-13

## Verdict

**BLOCKED.** Two fresh packaged two-browser public-relay runs reached stable
gameplay and movement, but route setup failed before required economy,
construction, research, production, combat, destruction, relay recovery,
natural victory, and terminal coverage. One audit-tool bug is confirmed. No
product-code bug is confirmed.

## Run configuration

- Durable evidence root:
  `artifacts/browser-multiplayer-audits/20260813T132158Z-crush-browser-multiplayer-bugs/`.
- Source commit: `603cab43e4e2b2caffdbf17ba291deb967ead3f3`.
- Packaged entry: `build-web/dist/aoe_web.html`, SHA-256
  `b439a25572fdcde3d4ff21f445a77c60670ef5f2f94e1f4a4f2ab5e19206a2fb`.
- Chrome `151.0.7922.109`; ChromeDriver `151.0.7922.138`; Selenium `4.47.0`.
- Viewport 1280x900; DPR 1; zoom 1; owned server port 8897.
- Public relays: `wss://nostr-pub.wellorder.net`,
  `wss://nostr.oxtr.dev`, `wss://relay.nostr.wirednet.jp`.
- Seed: `11055785183250`.
- Both attempts used independent temporary profiles and distinct ephemeral
  public keys. Private keys were not retained; exact public keys and match IDs
  are in each attempt's final state stream.

## Counts

- Candidates: 4.
- Confirmed bugs: 1 audit-tool bug; 0 product bugs.
- Rejected candidates: 2.
- Blocked/needs-proof cases: 1 candidate plus required downstream coverage.
- Infrastructure incidents: 1 relay rate-limited publications while two other
  relays accepted them; quorum remained healthy.
- Harness failures: 2 root causes/statuses: one confirmed oracle defect and one
  undetermined selection failure.

## Coverage

| Phase | Result | Evidence |
|---|---|---|
| Distinct identities, lobby, ready, start | PASS | Attempt state streams and run ledgers |
| Host, join, simultaneous movement | PASS, bounded | Retained movement evidence and correlated frames |
| Lockstep continuity before stop | PASS, bounded | Both peers contiguous through sender sequence 46 with empty missing ranges |
| Temporal/visual oracle | FAIL, tooling | CBMB-20260813-01 false failure |
| Direction route setup | BLOCKED | Join selection timeout or absent command |
| Economy/build/research/production | UNTESTED | Route setup stopped run |
| Combat/damage/destruction | UNTESTED | Route setup stopped run |
| Relay chaos/recovery | UNTESTED | Route setup stopped run |
| Natural victory/terminal freeze | UNTESTED | Route setup stopped run |

## Confirmed root-cause ledger

### CBMB-20260813-01 — movement oracle applied to static building layers

`analyze_render_samples()` at
`tests/web/nostr_multiplayer_smoke_test.py:2054-2163` sends every animated layer
through a branch that unconditionally requires entity movement positions.
House 45 has valid animated ambient layers but, as a static building, no unit
movement positions. Three ordinary production-frame phases therefore report
`frame oracle lacks positions`. `analyze_render_samples_for_audit()` at lines
2541-2562 retains each as `FAIL`. Independent review classified this
`CONFIRMED`, with three manifestations deduplicated to one root cause.

Bug log:
`../todo/BROWSER_MULTIPLAYER_BUG_HUNT_2026-08-13.md`.

No fix was requested or attempted. No product defect is inferred from the
oracle's false failure.

## Rejected and blocked

- Command/transport loss: `REJECTED`. Second run stopped before command
  construction; first run lacks proof of the failed boundary. Relay receipts,
  sender continuity, and missing-range state do not prove replication loss.
- Peer desync: `REJECTED`. Final snapshots compared ticks 138 and 139, not the
  same durable state.
- Join selection timeout: `NEEDS_PROOF` / harness failure. Production click was
  recorded, but precise root cause remains undetermined.
- Required later gameplay phases: `BLOCKED`, never counted as passes.

No security claim is made. No evidenced `BOUNDARY`, `SOURCE`, `PATH`, `HARM`,
or `CONTRACT` exists for these tooling observations.

## Cleanup

Both attempts closed owned browsers, drivers, and server. Port 8897 was
released. Descriptor use returned from 10 to 9, below baseline. Unrelated
working-tree edits and processes were preserved. No game code changed, so no
build or test command was run beyond the requested production audit journey.

## Follow-up fixes

Both harness problems were subsequently corrected:

- Movement-direction oracle now runs only for movable `unit-*` and
  `projectile-*` categories. Static animated buildings retain provenance,
  asset, draw, animation-progress, and peer-consistency checks.
- Route selection now reacquires synchronized unit position after camera
  movement and clicks only a stable current production tile. Retained telemetry
  records selection attempt and selected unit.

Focused audit-tool suite passed 74 tests. Fresh packaged two-browser validation
with seed `11055785183250` crossed prior join-selection failure, completed route
approach, captured direction frames, and retained zero visual-oracle failures.
It stopped only at configured action limit 40. Evidence:
`artifacts/browser-multiplayer-audits/20260813T132158Z-crush-browser-multiplayer-bugs/fix-validation/20260813T135904Z-6994351ae5dc/`.

These fixes remove identified harness blockers. Original broad gameplay audit
verdict remains `BLOCKED` because full economy, combat, relay-recovery, and
natural-victory coverage was not rerun.

## Current run — `20260813T140647Z`

**BLOCKED.** Fresh packaged run used two independent Chrome profiles and
ephemeral identities over three healthy public relays. Both peers joined one
match and completed sustained route movement through tick 1360+, but audit
stopped before economy, construction, research, production, combat,
destruction, relay recovery, natural victory, or terminal freeze.

- Raw root: `artifacts/browser-multiplayer-audits/20260813T140647Z/`.
- Attempt: `20260813T140921Z-e3f1529d3d8c/`.
- Package SHA-256:
  `b439a25572fdc3d4ff21f445a77c60670ef5f2f94e1f4a4f2ab5e19206a2fb`.
- Chrome `151.0.7922.109`; Selenium `4.47.0`; viewport 1280x900; DPR 1.
- Host key: `aaf1e59dd00319e8ebc60e629c3ebec4bfd0fcbf4046e7bb5e610f1ff5cd3b97`.
- Join key: `a3d5146cc16455e9950ace09121c37a6151420fc58ab7d7033fcd49e2bb5b22a`.
- Evidence totals: 136 actions, 39 correlated-frame records, 92 retained frame
  images, 78 provenance records covering 3,409 entity observations, and 424
  visual-oracle records (231 pass, 193 skipped).

Current-run counts: 3 candidates; 1 newly confirmed tooling bug; 2 rejected
candidate classes; 2 blocked/needs-proof candidate or coverage groups; 0 proved
infrastructure incidents; 1 harness failure. One tooling bug log entry was
written.

Sync diagnosis rejected product command loss/desync: both peers reached the
commanded tile, both sender streams were contiguous through 458, and both had
empty missing ranges. `matching_games()` requires exact instantaneous tick and
hash before testing acceptance; final peers differed by three ticks, so the
harness discarded matching arrival state and raised `BLOCKED_COMMAND_ABSENT`.
Independent review confirmed this tooling bug as CBMB-20260813-R2-01. It is
logged in `../todo/BROWSER_MULTIPLAYER_BUG_HUNT_2026-08-13.md`; no fix was
authorized or attempted.

Two visual candidates remain unconfirmed: 98 house provenance records compare
composite live layers against an expected set containing only SLP 2235, and
five opaque-overlap cases report 17/13/4/4/4 pixels. Exact product root cause is
undetermined; neither is logged as a product bug. Six pixel-oracle failures are
deliberate mutation checks and were rejected as product findings.

Cleanup completed: both browsers, driver, and owned server exited; port 8898
is free; descriptor count returned from 10 to 9. No product code changed and no
build or general test command ran.

## Harness restart — `20260813T145000Z-harness-restart-2`

**BLOCKED by public-relay infrastructure.** A fresh run verified the exact
destination-allocation harness correction through the production audit path,
then completed 82 ordinary gameplay actions before publication quorum failed.
The run retained 152 peer states, 75 correlated frames, 150 provenance rows,
148 visual-oracle rows, and 434 screenshots.

`receipt-312` was accepted by `wss://relay.nostr.wirednet.jp`; `wss://nostr.oxtr.dev`
rejected it as rate limited and `wss://nostr-pub.wellorder.net` timed out. Unit
9 consequently did not receive a quorum-committed command to `(28,12)`. This
is classified `public-relay-infrastructure`, not a product or harness bug.
Evidence:
`artifacts/browser-multiplayer-audits/20260813T145000Z-harness-restart-2/`.

The preceding launch attempt proved CBMB-20260813-R3-01: a report root nested
inside an exact artifact destination caused `allocate_audit_destination()` to
create the destination as a parent before trying to reserve it. Artifact
reservation now occurs first; a regression covers nested report roots. The
restart crossed allocation, relay probing, browser launch, lobby setup, and
gameplay capture, verifying the correction in the real audit path.

Cleanup completed: owned browsers, driver, and server exited; port 8899 is
free; descriptor count returned to its baseline of 5. Broad required coverage
remains incomplete, so relay rotation must restart the audit.

## Relay-rotation restart — `20260813T161000Z-harness-restart-3`

**FAIL with no confirmed product bug.** Three public-relay attempts and four
production-path minimization replays retained durable evidence. Attempt 1
stopped when unit 10's command to `(34,13)` was absent from both peers; the
first failed boundary remains undetermined. Attempt 2 proved an infrastructure
incident: `wss://nostr.mom` demanded 28-bit proof of work for host `turn-756`
and join `turn-744`, leaving only one accepting relay. Attempt 3 retained equal
peer hashes but failed to select join unit 9 before the
`quantization-boundary-vectors` route. Prefix minimization did not reproduce
that original identity. No game bug is confirmed or logged.

Direct harness inspection established CBMB-20260813-R4-01. Transition routes
centered the camera on each destination, then issued one click at a position
sampled before those camera operations and immediately waited for selection.
This bypassed `select_route_unit_at_current_position()`, which re-synchronizes
after camera movement, reacquires the unit's current tile, and retries visible
selection. Transition routes now use that helper. Production rerun remains
required before this correction is classified fixed.

Evidence:
`artifacts/browser-multiplayer-audits/20260813T161000Z-harness-restart-3/`.
Cleanup completed: port 8900 is free, all owned runtime exited, and descriptor
count returned to baseline 5.

## Bounded command-retry restart — `20260813T180000Z-harness-restart-4`

**BLOCKED with no confirmed product bug.** Three rotating public-relay attempts
all crossed route-unit selection, then each stopped on one transition command
whose destination was never observed on either peer: `(32,14)`, `(30,14)`, and
`(28,12)`. Attempt 2 separately retained a public-relay reliability failure;
the other two remained healthy. Focused sync diagnosis localized the unknown
boundary between application receipt of the dispatched right-click and
semantic command construction. No retained action intent or publication could
be correlated to any failed target, so product classification remains
`NEEDS_PROOF`.

Attempt 1 proves CBMB-20260813-R4-01 crossed its prior failure: join unit 10 was
selected through the synchronized helper and retained
`quantization-boundary-vectors` step-1 frames at ticks 2394–2412. The original
unit 9 selection timeout did not recur.

Direct harness evidence established CBMB-20260813-R5-01. One ambiguous absent
command ended the entire attempt even though visible input can be safely
reissued after synchronized reselection. Transition steps now retain each miss,
reacquire selection, and retry the same ordinary production command up to
three times. Three misses still block and remain available for product
diagnosis; one ambiguous dispatch can no longer discard later coverage.

Evidence:
`artifacts/browser-multiplayer-audits/20260813T180000Z-harness-restart-4/`.
Cleanup completed: port 8901 is free, owned runtime exited, and descriptors
returned to baseline 5.

## Transition-route restart — `20260813T192000Z-harness-restart-5`

**BLOCKED with no confirmed product bug.** Attempt 1 completed 23 transition
route records with empty `commandMisses` arrays and reached join
`quantization-boundary-vectors` step 7. This verifies synchronized selection
and first-attempt transition commands in the packaged production path. The
retry-after-miss branches added for CBMB-20260813-R5-01 were not exercised, so
that correction remains only partially production-verified.

Three later stops exposed CBMB-20260813-R6-01. Attempt 1's queued-waypoint
route stopped after unit 1 reached `(22,12)` but did not reach `(20,12)`.
Attempts 2 and 3 stopped while resetting unit 10 from `(30,12)` to `(28,12)`.
Tracebacks prove both paths bypassed the bounded retry logic used by ordinary
transition steps and propagated their first `BLOCKED_COMMAND_ABSENT`. Exact
product command-construction or publication boundaries remain undetermined;
none is classified as a product bug.

Queued recovery now synchronizes and reselects the unit, resumes only from a
reached canonical waypoint, and retains each miss. Route resets now use the
same synchronized selection and three-attempt bound. Persistent third misses
still block for diagnosis. Fresh production verification is required.

Evidence:
`artifacts/browser-multiplayer-audits/20260813T192000Z-harness-restart-5/`.
Cleanup completed: port 8902 is free, owned runtime exited, and descriptors
returned to baseline 5.

## All-direction selection restart — `20260813T210000Z-harness-restart-6`

**FAIL with no confirmed product bug.** Attempt 1 reached join all-direction
movement, then timed out after one left click for unit 10 at `(28,16)`.
Host and join retained equal state hashes and identical stopped unit state.
Failure capture later rendered unit 10's `selectionOverlay` at the exact
clicked tile, so the visible selection eventually took effect. Minimization
did not reproduce this identity.

Direct source inspection proves CBMB-20260813-R7-01: the all-direction inner
loop still used its route's assumed prior tile followed by one click and one
wait, bypassing `select_route_unit_at_current_position()`. That helper exists
to synchronize, reacquire the authoritative current tile after camera motion,
and retry delayed visible selection. The inner loop now uses it and takes its
returned current tile as the movement baseline. Fresh production verification
is required.

Reset and queued-command retry branches from CBMB-20260813-R6-01 were not
reached and remain production-unverified. Evidence:
`artifacts/browser-multiplayer-audits/20260813T210000Z-harness-restart-6/`.
Cleanup completed: port 8903 is free and all owned runtime exited.

## Stable-selection restart — `20260813T220000Z-harness-restart-7`

**FAIL with no confirmed product bug.** Attempt 1 stopped on an absent initial
movement command. Attempt 2 reproduced the selection failure through the full
31-action prefix and exhausted `select_route_unit_at_current_position()`.
Only attempt 1 inside that helper emitted a click; the remaining three loop
iterations consumed their retry budget while unit 10 continued moving toward
`(34,8)`. Final peer state was equal and the unit arrived normally.

This proves CBMB-20260813-R8-01. Despite its stopped-unit contract, the helper
waited only for matching peer states and did not reject `moving=true`. Camera
movement let authoritative position change, causing immediate loop retries
without input. It now waits for the named unit to be present, stopped, and at
the same position on both peers before centering or spending a selection
attempt. Fresh production verification is required.

Evidence:
`artifacts/browser-multiplayer-audits/20260813T220000Z-harness-restart-7/`.
Cleanup completed: port 8904 is free and descriptors returned to baseline 5.

## Muted production resume — `20260814T000000Z-muted-resume`

**BLOCKED with no confirmed product bug.** All browser parents carried
`--mute-audio`; preflight and per-attempt failure records preserve command-line
proof. Attempt 3 completed both three-lap all-direction sweeps, crossed join
quantization step 8, and entered host transition routes. This production run
verifies CBMB-20260813-R7-01 and CBMB-20260813-R8-01 across both peers.

Attempt 1 stopped on queued unit 1 to `(20,12)`. Attempts 2 and 3 reproduced
unit 10 to `(34,13)` absence in the obstacle-detour approach. Direct traceback
and source inspection prove CBMB-20260813-R9-01: obstacle approach and detour
each issued one command outside all bounded recovery logic. Both phases now
reselect synchronized stopped state and retry up to three times while retaining
miss records. Exact lower-layer causes of the absent commands remain
undetermined; no product bug is classified.

Evidence:
`artifacts/browser-multiplayer-audits/20260814T000000Z-muted-resume/`.
Cleanup completed: port 8906 is free, owned runtime exited, and descriptors
returned to baseline 5.

## Obstacle-retry restart — `20260814T013000Z-obstacle-retry`

**BLOCKED with no confirmed product bug.** Attempts 1 and 2 exercised three
transition-reset attempts before unit 10 to `(28,12)` remained absent. Attempt
3 crossed that boundary and exercised three obstacle-detour attempts before
unit 10 to `(40,13)` remained absent. This production-verifies bounded reset
and obstacle retry invocation; first-miss termination no longer caused either
stop.

Focused sync diagnosis found equal peer ticks and hashes, empty missing ranges,
healthy reliability, and nine precisely logged pointer retries. First unproved
boundary is semantic command construction. Failed route-local `commandMisses`
were lost when exceptions unwound, `transport.jsonl` contained no recovery
phase, and retained publication/subscription windows lacked action correlation.
CBMB-20260813-R10-01 therefore blocked exact classification.

Harness now appends `command-boundaries.jsonl` before and after every reset,
queued, ordinary transition, and obstacle retry. Each durable record correlates
pointer action, phase, attempt, destination, peer tick/hash/sequence/missing
ranges, reliability, recent publications, subscription messages, outcome, and
error. Records flush before propagation, so persistent failures remain
diagnosable. Fresh production verification is required.

Evidence:
`artifacts/browser-multiplayer-audits/20260814T013000Z-obstacle-retry/`.
Cleanup completed: port 8907 is free and descriptors returned to baseline 5.

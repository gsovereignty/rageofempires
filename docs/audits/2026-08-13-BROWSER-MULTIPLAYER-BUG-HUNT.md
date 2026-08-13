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

Current-run counts: 3 candidates; 0 newly confirmed bugs; 2 rejected candidate
classes; 3 blocked/needs-proof candidate or coverage groups; 0 proved
infrastructure incidents; 1 harness failure. No new bug log entry was written.

Sync diagnosis rejected product command loss/desync: both peers reached the
commanded tile, both sender streams were contiguous through 458, and both had
empty missing ranges. `matching_games()` requires exact instantaneous tick and
hash before testing acceptance; final peers differed by three ticks, so the
harness discarded matching arrival state and raised `BLOCKED_COMMAND_ABSENT`.
Primary independent confirmation remains required before this becomes a
confirmed tooling bug.

Two visual candidates remain unconfirmed: 98 house provenance records compare
composite live layers against an expected set containing only SLP 2235, and
five opaque-overlap cases report 17/13/4/4/4 pixels. Exact product root cause is
undetermined; neither is logged as a product bug. Six pixel-oracle failures are
deliberate mutation checks and were rejected as product findings.

Cleanup completed: both browsers, driver, and owned server exited; port 8898
is free; descriptor count returned from 10 to 9. No product code changed and no
build or general test command ran.

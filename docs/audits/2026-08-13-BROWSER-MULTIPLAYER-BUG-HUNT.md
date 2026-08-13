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

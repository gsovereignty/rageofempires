# Browser multiplayer broad production runtime audit — 2026-08-11 17:33 UTC

## Verdict

**BLOCKED.** One fresh packaged two-browser run brought both peers to tick 8 or
later and reached the relay-chaos phase over three ordinary public relays. An
unrelated third game browser appeared after preflight, violating the
run-isolation contract. The
owned run then exposed one confirmed audit-harness defect and stopped before
movement, combat, temporal-render, sprite-provenance, recovery, terminal, or
long-match coverage. No product defect is confirmed from this run.

## Scope and build ledger

- Source commit: `ce5bbc9d02b560800651a547fe3b45ce7671d384`.
- Packaged artifact: `build-web/dist/aoe_web.html` plus packaged JS, WASM,
  data, and Nostr bridge files.
- Package SHA-256 values: recorded in the run ledger at
  `artifacts/browser-multiplayer-audits/20260811T173355Z-broad-production-audit/run.json`.
- Browser: Chrome `151.0.7922.76`; ChromeDriver `151.0.7922.77`; Selenium
  `4.47.0` from the repository virtual environment.
- Relays: `wss://relay.damus.io`, `wss://nos.lol`, and
  `wss://relay.primal.net`; quorum 2.
- Owned local server: `127.0.0.1:8897`.
- Host public key / Blue slot:
  `5fdf959168f502681f670c89c3b4c28f22492e6bdc64335c54a83c91d747196a`.
- Join public key / Red slot:
  `9708bf8eeb668578e799f88d16195e3b9527e315ed75b0df664ca3c12892c354`.
- Match reference, lobby revision 2, scenario digest
  `save131+ids-fnv1a64:ede762fbf1afbbb2`, 200 ms tick cadence, display and
  package digests: `run.json`.
- Viewport: 1280x900 outer, 1280x757 inner; logical, CSS, and backing canvas
  all 1280x720; device scale 1.
- Random action and transport-fault seeds: none. This was a fixed production
  journey using visible canvas, keyboard, lobby, and relay controls.
- Private key material was neither inspected nor retained.

## Runtime ownership and isolation

Operator-observed, unretained preflight at `2026-08-11T17:33:55Z` found no stale multiplayer audit process,
ChromeDriver, or Selenium Chrome. Soft file-descriptor limit was 256 and the
audit shell used 10 descriptors, below the 128 half-limit. Port 8897 was free.
An existing unrelated server on port 8888 was left untouched.

The owned run used exactly two independent temporary Chrome profiles. Operator
observation, not retained in the artifact bundle, recorded that at
approximately 17:36 UTC, unrelated ChromeDriver PID 15898 and Chrome PID 15899
appeared with a `worms-host-*` profile. They were absent at preflight and were
not touched. Their appearance invalidated isolation for the remainder of the
run. No retry was made while that browser remained live.

## Coverage matrix

| Phase or oracle | Result | Direct evidence or limit |
|---|---|---|
| Distinct identities and canonical lobby | PASS | Distinct public keys, shared match reference, lobby revision 2. |
| Packaged production assets enabled | PASS, startup only | Both browser logs record DAT, unit animation, resource sprite, building composite, interface sprite, and asset-closure loads; failure screenshots are non-black. |
| Active lockstep start | PASS, limited | Harness control flow reached relay chaos only after both peers were ready and ticks reached at least 8. Intermediate baseline snapshot was not retained. |
| One-relay loss with continued progress | PASS, limited | Harness advanced beyond its one-relay assertion. Intermediate state was not retained because `evidence["recovery"]` is assigned only after all recovery stages. |
| Quorum-loss suspension | BLOCKED | Final diagnostics show all three relays disabled and both peers suspended with reliability status 2/reason 5, but snapshots differ at ticks 10/11 and hashes differ. Harness predicate was impossible because of BG-20260811-01, and run was contaminated. |
| Relay restore/backfill | UNTESTED | Harness stopped before restore. |
| Host and join world commands | UNTESTED | Journey stopped before movement phase. |
| Simultaneous commands | UNTESTED | Journey stopped before movement phase. |
| Gather/build/production/research | UNTESTED | Current harness has no broad coverage for these actions. |
| Combat/damage/death/destruction | UNTESTED | Current harness has no broad coverage for these actions. |
| Chat and signal | UNTESTED | Journey stopped before side-channel phase. |
| Speed and pause/resume | UNTESTED | Journey stopped before control phase. |
| Terminal result/frozen tick | UNTESTED | Journey stopped before resignation. |
| Temporal motion oracle | BLOCKED | No correlated frame samples were reached; `sprite-provenance.jsonl` is empty and `motion.json` contains null phases. |
| Production visual oracle | BLOCKED | Startup asset logs and failure screenshots do not prove per-entity identity, frame, palette, anchor, alpha, layers, or fallback absence. |
| Seeded long match | UNTESTED | Fixed bounded journey stopped in relay chaos. |
| Controlled transport chaos | UNTESTED | No mock or controlled transport was used. |

## Confirmed finding

### BG-20260811-01: quorum-loss harness disables all relays but waits for two disabled

- Severity: medium for audit reliability.
- Category: tooling correctness.
- Location: `tests/web/nostr_multiplayer_smoke_test.py`, relay-chaos block in
  `run`, around lines 601-633 at the tested commit.
- Trigger: run with three configured relays.
- Expected: after proving one-relay tolerance, remove one additional relay,
  leave one enabled, and assert a two-disabled quorum-loss state.
- Actual: relay 0 is disabled first. The next loop iterates `range(1,
  len(relays.split(",")))`, disabling relays 1 and 2 as well. The following
  predicate requires `disabled=len(relays.split(","))-1`, or 2. Final
  diagnostics prove both peers had 3 disabled relays, so the predicate could
  never return a match and timed out after 180 seconds.
- Impact: every three-relay harness execution that reaches this block with
  functional visible relay controls stops before restore, movement,
  render/provenance, controls, and terminal phases even when both clients
  enter the expected suspended reliability state.
- Proof: source loop and predicate above; `first-failure.json` error; both
  final diagnostics list all three disabled relay URLs and reliability
  status 2/reason 5; both failure screenshots show three `Restore` buttons.
- Root cause: exact off-by-one mismatch between the relay-disabling loop and
  the expected disabled-relay count.
- Status: `CONFIRMED` audit-tool bug. No product-code bug inferred.
- Duplicate group: `browser-multiplayer-relay-chaos-disabled-count`.

No security classification is claimed. No evidenced security boundary,
less-trusted source, security-relevant path, confidentiality/integrity/
authorization/isolation/privilege harm, or security contract exists for this
tooling failure. Security framing would be `MISCLASSIFIED`.

## Crush Bugs evidence ledger

| Finding | Category | Confirmation | Status | Fix | Regression | Prevention | Result |
|---:|---|---|---|---|---|---|---|
| BG-20260811-01 | tooling correctness | Direct source inspection plus final production-run diagnostics prove 3 disabled while predicate requires 2. | CONFIRMED | Not changed by audit agent. | None added. | None; currently absent. | FAIL: current harness cannot complete three-relay chaos journey. |

Proposed remediation: disable only one additional relay, or change contract and
predicate consistently; retain intermediate recovery evidence before later
stages.

## Rejected and uncertain candidates

- Product quorum-loss desynchronization: `NEEDS_PROOF`, not confirmed. Final
  host/join snapshots differ by one tick and state hash, but capture was not
  correlated, harness predicate was impossible, and unrelated browser activity
  invalidated isolation. No precise production root cause is established.
- Visual defects visible in failure screenshots: `REJECTED` as findings from
  this run. Screenshots prove non-black production rendering only; different
  local cameras and absent correlated provenance prevent peer visual comparison.
- Console entries labeled `SEVERE`: `REJECTED` as bug proof. Messages are
  informational asset-loader/render logs and contain no runtime failure.

Original decompiled source and original assets were not used to establish the
tooling defect: it has no original-game fidelity expectation. No product
fidelity judgment was made from disputed evidence.

## Evidence inventory

Evidence root:
`artifacts/browser-multiplayer-audits/20260811T173355Z-broad-production-audit/`.

Retained files include `run.json`, `actions.jsonl`, `first-failure.json`,
per-client final state streams, `motion.json`, empty sprite-provenance and
transport streams, console logs, and host/join failure screenshots. Empty or
placeholder action records describe planned later phases, not executed
actions; this report does not count them as coverage.

Motion totals: 0 correlated samples and 0 retained motion frames. Sprite
provenance totals: 0 records; procedural/fallback totals therefore unknown,
not zero. Public-relay coverage is separated from controlled transport:
public-relay lobby/start and partial relay-chaos were reached; controlled
transport was not run.

## Cleanup

Operator-observed cleanup, not retained in the artifact bundle, found owned
Python, both ChromeDrivers, both Chrome processes, and server exited. Port 8897
was released. Audit-shell descriptor use returned to 10/256. The
unrelated port-8888 server, unrelated `worms-host-*` browser, and unrelated
`.codebase-memory` working-tree changes were preserved. No product code was
modified. No build, compiler, or test-suite gate was run because audit output
is documentation/artifacts only.

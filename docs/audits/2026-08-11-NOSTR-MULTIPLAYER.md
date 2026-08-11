# Public Nostr multiplayer audit — 2026-08-11

## Scope and build

- Commit tested: `53c6ba5` plus the focused movement-evidence changes documented below.
- Branch: `main`.
- UTC test window: 2026-08-11.
- Browser / Selenium / Emscripten: Chrome 151.0.7922.76 / Selenium 4.47.0 / Emscripten 4.0.10.
- Relays: `wss://relay.damus.io`, `wss://nos.lol`, `wss://relay.primal.net`.
- Local serving ports: 8890 and 8891, released after each run.
- Terminal/recovery/movement evidence: `artifacts/nostr-multiplayer/20260811T-movement-proof/final-production-smoke.json`.
- Checkpoint evidence: `artifacts/nostr-multiplayer/20260811T-movement-proof/final-production-checkpoint.json`.
- Packaged artifact: `build-web/dist/aoe_web.html` and its generated JS, WASM, data, and Nostr bridge files.

## Identity ledger

| Role | Public key | Slot | Distinct | Private material absent |
|---|---|---:|---|---|
| Host | `a194d9a4ab5bf366db7c3b7aa9a620ca357cd9b281ee9660eee59e905795323f` | Blue | Yes | Yes |
| Joiner | `2e1b313901de8ea59f035bbf09cb5e5ae32a5664f0f09bde785079a73d448bf0` | Red | Yes | Yes |

Both keys were created by separate normal browser launches using independent
Chrome profiles. No private key or signer serialization was inspected, copied,
persisted, or found in captured diagnostics and logs.

## Journey ledger

| Milestone | Host | Join | Status | Evidence |
|---|---|---|---|---|
| Public relay EOSE/quorum | 3 relays, quorum 2 | 3 relays, quorum 2 | PASS | `final-production-smoke.json` |
| Canonical lobby and roster | Revision 2, Blue | Revision 2, Red | PASS | same evidence; distinct public keys above |
| Exact-lobby acknowledgement and ready | Both observed at quorum | Both observed at quorum | PASS | final diagnostics |
| Visible start and lockstep | Started; tick 45 | Joined; tick 45 | PASS | final diagnostics and screenshots |
| Non-empty unit command and empty turns | Selected unit 3 moved `(11,23)` to `(12,23)` | Same ID and positions | PASS | direct production simulation diagnostics after normal canvas selection/right-click; both streams contiguous through 15 with no missing range |
| Public chat | One entry | One entry | PASS | `chatCount: 1` on both |
| Allied signal | One entry | One entry | PASS | `signalCount: 1` on both; `signal-armed.png` |
| Speed and pause/resume | Fast, resumed | Fast, resumed | PASS | `gameSpeed: 2`, `paused: false`; production run reached terminal |
| Save/checkpoint barrier | Matched at tick 42 | Matched at tick 42 | PASS | `final-production-checkpoint.json`; `stateHashStatus: 2`, equal sequences through 14, no missing ranges |
| Resignation and terminal result | Blue resigned, Red active; outcome 2, tick 45 | Same roster result | PASS | both observe two agreeing signed result records and hash `save130+ids-fnv1a64:1f59400ed6cf8180`; stable tick |

The checkpoint and terminal checks use separate fresh production journeys.
Matched checkpoint is intentionally a resumable-session stop, so continuing
ordinary play after it is not part of that runtime contract.

## Relay recovery ledger

| Check | Status | Evidence |
|---|---|---|
| Continue after one relay loss | PASS | Visible control disconnected Damus on both; tick advanced equally from 8 to 10 over Primal and nos.lol quorum. |
| Stop safely after quorum loss | PASS | Visible control also disconnected nos.lol; both suspended at tick 10 with `relay_quorum_lost`; tick stayed unchanged for two seconds. |
| Restore, EOSE, backfill, and resume | PASS | Visible Restore for nos.lol reached EOSE; both resumed equally at tick 11, then all three relays restored by tick 15. |
| Duplicate delivery executes once | PASS | Both final streams contiguous through 15 with empty missing ranges; chat and signal counts remain one. |
| Post-recovery hash agreement | PASS | Both reached stable terminal tick 45 and identical final state hash. |

All relay changes used visible production buttons. No direct JavaScript state
mutation, mock, proxy, local relay, or synthetic event was used.

## Problems encountered

### Startup configuration erased required peer identities

- Classification: product defect.
- First failed milestone: host initialization.
- Observed behavior: public lobby startup aborted before canonical join.
- Expected behavior: compatibility hashing must not mutate the runtime configuration.
- Reproduction: original packaged two-browser launch.
- Proven root cause: `compatibility_digest` cleared required peer IDs in the configuration used by the live runtime.
- Affected production path: browser launch → Nostr multiplayer runtime initialization.
- Evidence: pre-fix failure artifacts; corrected production run; commit `47dc31d`.

### Relay filters were rejected for excessive tag constraints

- Classification: product defect with relay-specific manifestation.
- First failed milestone: relay subscription establishment.
- Observed behavior: ordinary public relays returned `too many tags in filter`.
- Expected behavior: supported public-relay subscriptions reach EOSE/quorum.
- Reproduction: original packaged public-relay launch.
- Proven root cause: the subscription combined match, application, and protocol tag constraints; affected relays rejected that filter shape.
- Affected production path: Applesauce subscription setup for match events.
- Evidence: pre-fix browser diagnostics; successful three-relay EOSE after commit `47dc31d`.

### Slow lobby immediately entered peer-silent suspension

- Classification: product defect.
- First failed milestone: first lockstep turns after start.
- Observed behavior: a lobby taking over 30 seconds entered peer-silent state immediately after start, before first turn exchange.
- Expected behavior: peer silence timing begins at match start.
- Reproduction: public-relay lobby with delayed readiness/start.
- Proven root cause: `last_peer_traffic_` retained browser-launch time when the start commit was applied.
- Affected production path: `NostrMultiplayerRuntime::Impl::maybe_apply_start` → reliability update.
- Evidence: pre-fix `last-failure.json`; post-fix production journeys advancing beyond eight equal ticks.

### Delayed completed control stages aborted the next control

- Classification: product defect.
- First failed milestone: pause/resume after speed control.
- Observed behavior: host aborted with `control event does not match proposal`.
- Expected behavior: authenticated duplicates of an already completed proposal are idempotent.
- Reproduction: speed change followed by pause/resume over three public relays.
- Proven root cause: `handle_control` compared delayed stages for a completed ID against the newer pending proposal and had no completed-ID watermark.
- Affected production path: public Nostr control event → `handle_control`.
- Evidence: `last-failure-host.png` before correction; successful speed and pause/resume in `production-smoke-terminal-stop-fix.json`.

### Superseded lobby handshake events were not retired

- Classification: product defect.
- First failed milestone: canonical lobby revision 2.
- Observed behavior: joiner showed `startup failed: stale lobby acknowledgement` after relays delivered revisions out of order.
- Expected behavior: authenticated handshake records for a superseded lobby revision are ignored.
- Reproduction: fresh public-relay run; host reached revision 2 while joiner aborted.
- Proven root cause: mismatched handshake events were repeatedly deferred, and non-current/non-quorum acknowledgements aborted instead of remaining non-authoritative.
- Affected production path: public relay event → deferred handshake replay → acknowledgement handler.
- Evidence: `last-failure-join.png` and associated diagnostics; later production journey passed revision 2.

### Multiplayer resignation keyboard path bypassed lockstep

- Classification: product defect.
- First failed milestone: source-path audit of terminal input.
- Observed behavior: `Ctrl+Shift+R` directly executed a local `ResignCommand` while other multiplayer commands used `queue_command`.
- Expected behavior: resignation is committed through the same lockstep route on both clients.
- Reproduction: direct production-path inspection of the keyboard handler.
- Proven root cause: multiplayer routing branch was absent in `SdlApp::loop`.
- Affected production path: SDL keyboard input → resignation command.
- Evidence: corrected handler is included in passing native/web builds. The
  visible-button route was production-verified with equal outcome and hash;
  the exact keyboard chord remains separately unverified.

### Terminal matches continued scheduling turns

- Classification: product defect.
- First failed milestone: stable terminal state.
- Observed behavior: both clients agreed on resignation outcome and hash, but lockstep ticks continued advancing afterward.
- Expected behavior: no new gameplay turns after terminal outcome.
- Reproduction: visible `RESIGN` button in two-browser production run.
- Proven root cause: `NostrMultiplayerRuntime::Impl::pump` published a result but had no terminal guard before later calls to `schedule_turns` and `advance`.
- Affected production path: simulation outcome → Nostr runtime pump.
- Evidence: pre-fix equal terminal diagnostics at later tick 40; post-fix stable tick 30 in `production-smoke-terminal-stop-fix.json`.

### Initial audit shell lacked Selenium

- Classification: tooling.
- First failed milestone: harness import.
- Observed behavior: system Python raised `ModuleNotFoundError: selenium`.
- Expected behavior: use repository isolated Selenium environment.
- Reproduction: `python3 tests/web/nostr_multiplayer_smoke_test.py`.
- Proven root cause: command used system Python instead of `build-web/selenium-venv/bin/python`.
- Affected production path: none.
- Evidence: console traceback; all subsequent runs used the isolated environment.

### Checkpoint harness attempted post-checkpoint gameplay

- Classification: automation.
- First failed milestone: terminal result after matched checkpoint.
- Observed behavior: both peers remained at the matched checkpoint until peer-silent timeout; resignation was queued after the checkpoint stop.
- Expected behavior: checkpoint acceptance and terminal acceptance use contract-compatible journeys.
- Reproduction: harness requested F6, waited for MATCHED, then clicked RESIGN.
- Proven root cause: test ordering contradicted `LockstepSaveBarrier::should_pause`, which intentionally pauses at any non-idle barrier status at or beyond its target tick.
- Affected production path: none; harness expectation was wrong.
- Evidence: checkpoint run `last-failure.json`; terminal rerun without post-checkpoint input passed.

### Relay-control harness retained a replaced DOM node

- Classification: automation.
- First failed milestone: one-relay degradation.
- Observed behavior: Selenium raised `StaleElementReferenceException` while
  clicking a visible relay button.
- Expected behavior: live status refresh does not invalidate the test action.
- Reproduction: open relay management while diagnostics refresh its buttons,
  then click a previously located button.
- Proven root cause: the browser shell re-renders relay buttons on diagnostics
  updates while the harness retained one DOM element between state read and
  click.
- Affected production path: none; visible control remained available.
- Evidence: failed production-run traceback; harness now re-locates the button
  after replacement, and the final recovery journey passes.

### In-match relay recovery control is absent

- Classification: missing capability.
- First failed milestone: relay loss and recovery.
- Observed behavior: production UI can choose relays before launch but cannot remove or restore one during a match.
- Expected behavior: an auditable supported control for relay loss/restoration if recovery is to be acceptance-tested through production behavior.
- Proven root cause: no in-match relay management control exists in the browser launch shell or game UI.
- Affected production path: relay recovery acceptance only.
- Evidence: browser shell and runtime UI inspection. Closed by visible
  per-relay Disconnect/Restore controls; both final production journeys use
  those controls successfully.

### Quorum-failed turn was unavailable to restored relay

- Classification: product defect.
- First failed milestone: relay EOSE backfill and lockstep recovery.
- Observed behavior: after nos.lol restoration reached EOSE, both peers stayed
  at tick 10 and eventually entered peer-silent suspension.
- Expected behavior: exact signed turn unavailable on restored relay is
  republished, then both peers resume from same sender sequence.
- Reproduction: disconnect Damus on both peers, continue with quorum, disconnect
  nos.lol to lose quorum, then restore nos.lol through visible controls.
- Proven root cause: a turn that reached only Primal returned a quorum-failed
  publication result. `handle_publish_result` discarded its event-ID/sequence
  mapping, while receipt recovery could only republish IDs in that mapping.
  Restored nos.lol therefore completed EOSE without exact signed turn.
- Affected production path: turn publication result → cached signed-event
  recovery → restored-relay subscription.
- Evidence: pre-fix `last-failure.json` showed tick 10, contiguous sequence 4,
  nos.lol EOSE, and peer-silent suspension; `final-production-smoke.json`
  proves exact republication, tick resumption, empty missing ranges, and final
  hash agreement through same production path.

## Coverage gaps

- The UI does not display npub encoding; distinct 64-hex public keys provide
  available identity proof.
- Exact `Ctrl+Shift+R` browser modifier synthesis remains a harness limitation.
  Visible RESIGN production control covers terminal gameplay path.

## Verdict

**PROBLEMS FOUND.** Corrected packaged build passes complete base gameplay,
terminal result, one-relay degradation, quorum-loss stop, restored-relay EOSE
and backfill, resumed lockstep, duplicate suppression, post-recovery hash
agreement, direct equal-world movement, and separate matching checkpoint
journey. Eight product defects were confirmed; seven corrections were verified
through affected production journeys. Keyboard-only resignation routing
remains built but not separately reproduced; visible terminal route passes.

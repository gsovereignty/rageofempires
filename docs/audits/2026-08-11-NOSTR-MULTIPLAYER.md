# Public Nostr multiplayer audit — 2026-08-11

## Scope and build

- Commit tested: `47dc31ddee6ca67c4b1b45946fac6cee0ba1c4f3` plus the focused fixes documented below.
- Branch: `main`.
- UTC test window: 2026-08-11.
- Browser / Selenium / Emscripten: Chrome 151.0.7922.76 / Selenium 4.47.0 / Emscripten 4.0.10.
- Relays: `wss://relay.damus.io`, `wss://nos.lol`, `wss://relay.primal.net`.
- Local serving port: 8888, released after each run.
- Evidence directory: `artifacts/nostr-multiplayer/20260811T112539Z/`.
- Packaged artifact: `build-web/dist/aoe_web.html` and its generated JS, WASM, data, and Nostr bridge files.

## Identity ledger

| Role | Public key | Slot | Distinct | Private material absent |
|---|---|---:|---|---|
| Host | `67a48bb81458ae8a3dc107e1e77903d5367df15a999e4f1b55a22419e40a21cc` | Blue | Yes | Yes |
| Joiner | `edadfc80b8898afd1eb7ba54eea115d5e21cabb137931bc68985a9163647e177` | Red | Yes | Yes |

Both keys were created by separate normal browser launches using independent
Chrome profiles. No private key or signer serialization was inspected, copied,
persisted, or found in captured diagnostics and logs.

## Journey ledger

| Milestone | Host | Join | Status | Evidence |
|---|---|---|---|---|
| Public relay EOSE/quorum | 3 relays, quorum 2 | 3 relays, quorum 2 | PASS | `production-smoke-terminal-stop-fix.json` |
| Canonical lobby and roster | Revision 2, Blue | Revision 2, Red | PASS | same evidence; distinct public keys above |
| Exact-lobby acknowledgement and ready | Both observed at quorum | Both observed at quorum | PASS | final diagnostics |
| Visible start and lockstep | Started; tick 30 | Joined; tick 30 | PASS | final diagnostics and screenshots |
| Non-empty unit command and empty turns | Blue sequences contiguous through 10 | Same received stream, no missing range | PASS | equal terminal state hash after normal canvas selection/right-click; final diagnostics |
| Public chat | One entry | One entry | PASS | `chatCount: 1` on both |
| Allied signal | One entry | One entry | PASS | `signalCount: 1` on both; `signal-armed.png` |
| Speed and pause/resume | Fast, resumed | Fast, resumed | PASS | `gameSpeed: 2`, `paused: false`; production run reached terminal |
| Save/checkpoint barrier | Matched at tick 29 | Matched at tick 29 | PASS | `last-failure.json` from the checkpoint run shows `stateHashStatus: 2` on both; checkpoint publication succeeded on quorum |
| Resignation and terminal result | Outcome 2, tick 30 | Outcome 2, tick 30 | PASS | equal terminal hash `save130+ids-fnv1a64:51a157e22c85e5ed`; stable tick; `production-smoke-terminal-stop-fix.json` |

The checkpoint and terminal checks use separate fresh production journeys.
Matched checkpoint is intentionally a resumable-session stop, so continuing
ordinary play after it is not part of that runtime contract.

## Relay recovery ledger

| Check | Status | Evidence |
|---|---|---|
| Continue after one relay loss | blocked: missing production control | Browser UI exposes relay selection only before launch. |
| Stop safely after quorum loss | blocked: missing production control | No supported in-match relay disconnect control. |
| Restore, EOSE, backfill, and resume | blocked: missing production control | No supported in-match relay restore control. |
| Duplicate delivery executes once | PASS in ordinary operation | Multi-relay duplicate delivery present; contiguous sender sequences and single chat/signal entries on both. |
| Post-recovery hash agreement | blocked: missing production control | Recovery cannot be initiated through production UI. |

No relay was disconnected through JavaScript mutation, mocks, a proxy, or a
local relay. The unsupported checks therefore remain unclaimed.

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

### In-match relay recovery control is absent

- Classification: missing capability.
- First failed milestone: relay loss and recovery.
- Observed behavior: production UI can choose relays before launch but cannot remove or restore one during a match.
- Expected behavior: an auditable supported control for relay loss/restoration if recovery is to be acceptance-tested through production behavior.
- Proven root cause: no in-match relay management control exists in the browser launch shell or game UI.
- Affected production path: relay recovery acceptance only.
- Evidence: browser shell and runtime UI inspection.

## Coverage gaps

- Relay loss, quorum loss, stored-event EOSE/backfill, and recovery remain
  untested because production exposes no supported in-match relay control.
- The UI does not display npub encoding; distinct 64-hex public keys provide
  the available identity proof.
- Final roster result is represented by agreed simulation outcome and final
  state hash; diagnostics do not expose a separate roster-result field or
  explicit received-result-event ledger.

## Verdict

**PROBLEMS FOUND.** The complete base gameplay path and terminal result pass in
the corrected packaged build, and a separate packaged journey proves matching
checkpoint hashes. Seven product defects were confirmed; six corrections were
verified through their affected production journey, while the keyboard-only
resignation route is implemented and built but not separately reproduced.
Full PASS is unavailable because production relay loss/recovery is blocked by
missing controls.

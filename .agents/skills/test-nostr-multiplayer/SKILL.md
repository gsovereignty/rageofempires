---
name: test-nostr-multiplayer
description: Test this project's packaged web multiplayer end to end through two independent browser instances, two distinct ephemeral Nostr identities, and ordinary public relays; verify lobby, readiness, deterministic lockstep gameplay, public side channels, controls, hashes, checkpoint flow, and terminal behavior, while capturing evidence and documenting every confirmed problem or blocker. Use for Nostr multiplayer gameplay tests, two-browser multiplayer audits, public-relay acceptance, ephemeral npub validation, relay quorum/reconnect checks, or investigation of browser multiplayer diagnostics.
---

# Test Nostr multiplayer

Read repository `AGENTS.md` files first. Treat every run as production-path
acceptance, not proof by mocks or direct state injection.

Use exactly two independent Chrome profiles: one host and one joiner. Let each
page create its own ephemeral Applesauce signer through normal launch flow.
Never supply, copy, inspect, persist, or report private keys.

## Preserve audit boundaries

- Do not modify product code while auditing unless user separately requests a
  fix. Diagnose and document confirmed problems.
- Do not run a local relay, proxy, coordinator, signaling server, or application
  server. Static HTTP serving of packaged files is allowed.
- Use ordinary configured public `wss://` relays. Never answer NIP-42 AUTH.
- Never classify a problem as cybersecurity-relevant without proving all five
  repository-required fields: `BOUNDARY`, `SOURCE`, `PATH`, `HARM`, and
  `CONTRACT`. Otherwise record actual product impact only.
- Keep screenshots, browser logs, downloaded diagnostics, and raw run evidence
  under `artifacts/nostr-multiplayer/<run-id>/`.
- Write durable findings to
  `docs/audits/<YYYY-MM-DD>-NOSTR-MULTIPLAYER.md`.

## Preflight

1. Record Git commit, current branch, UTC time, browser version, Emscripten
   version, Selenium version, selected relays, and whether port 8888 is free.
2. Preserve unrelated working-tree changes. Never clean build output or kill a
   process not owned by this audit.
3. Run repository `make` because multiplayer uses shared game code.
4. Build packaged web target without starting a long-lived Makefile server:

```sh
. build-web/emsdk/emsdk_env.sh
cmake --build build-web --target aoe_web --parallel "${JOBS:-4}"
```

5. Use existing isolated Selenium environment. If absent, create it under
   ignored build output:

```sh
python3 -m venv build-web/selenium-venv
build-web/selenium-venv/bin/python -m pip install selenium
```

6. Verify `build-web/dist/aoe_web.html`, `.js`, `.wasm`, `.data`, and
   `aoe_nostr.js` exist. Reject stale source-only evidence.

## Run baseline production journey

Use existing harness first:

```sh
build-web/selenium-venv/bin/python \
  tests/web/nostr_multiplayer_smoke_test.py \
  --port 8888 \
  --evidence artifacts/nostr-multiplayer/<run-id>/production-smoke.json
```

If port 8888 is occupied by an audit-owned process, reuse or stop that process
cleanly. If another process owns it, choose a free port with `--port` and record
deviation. Port choice does not change relay/game protocol.

Harness must launch two browser instances with separate Chrome profiles. Do
not reuse one page, one profile, one signer, or one browser storage directory
for both roles.

## Verify identity and relay establishment

Capture `Module.browserNostrDiagnostics()` independently from host and joiner.
Require:

- both `publicKey` values are 64 lowercase hex characters;
- public keys differ;
- corresponding npubs, when displayed or encoded by supported project tooling,
  differ;
- neither diagnostics nor logs contain private-key or signer serialization;
- both participants agree on match ID, epoch, host pubkey, ordered relay set,
  protocol version, and lobby event ID/revision;
- every participant reaches required EOSE/quorum through ordinary public
  relays;
- joiner's accepted roster slot maps to joiner's pubkey;
- host and joiner observe exact-lobby acknowledgement and ready events.

Failure of any item blocks gameplay acceptance. Preserve both diagnostics and
record exact differing fields.

## Exercise complete gameplay

Use visible production UI and normal game inputs. Do not inject frames, mutate
simulation state, force ticks, use test-only runtime branches, or call internal
queue functions.

Require this ordered journey:

1. Host creates public lobby and copies match reference.
2. Joiner enters that reference and joins canonical lobby.
3. Both acknowledge and ready against same lobby revision.
4. Host starts through normal UI.
5. Both advance through at least eight equal lockstep ticks.
6. Host selects a unit and issues one non-empty movement or gather command;
   verify matching world result in both pages.
7. Allow several explicit empty turns; verify continued equal tick advancement.
8. Send public chat from joiner; verify both logs show it once.
9. Send allied map signal from host; verify both clients show it once.
10. Commit one speed change and pause/resume barrier; verify equal barrier tick
    and final control state.
11. Request save/checkpoint barrier; verify matching state hashes and checkpoint
    digest without public save bytes.
12. Exercise resignation or ordinary match objective through terminal result;
    verify both clients agree on outcome, final tick, roster result, and final
    state hash.

Do not call run passed if terminal result is absent. Mark it `partial` even when
all earlier stages pass.

At every milestone capture:

- both browser diagnostics;
- browser console errors/warnings;
- current tick and state-hash status;
- lobby revision, ready state, and slot/pubkey mapping;
- relay EOSE, publication, and quorum state;
- sender contiguous sequences and missing ranges;
- host and join screenshots where visible state matters.

## Test relay loss and recovery

After base journey succeeds, repeat in fresh browsers and test:

1. Remove or disconnect one configured relay through supported production
   behavior; verify remaining configured quorum continues.
2. Lose quorum; verify both clients wait or suspend and ticks stop without
   synthesized empty input.
3. Restore relay connectivity; verify stored-event subscription reaches EOSE,
   missing sequences backfill, and same match resumes.
4. Verify no logical input executes twice after multi-relay duplicate delivery.
5. Verify post-recovery state hashes and ticks match.

If production UI exposes no supported way to perform a step, record `blocked:
missing production control`; do not simulate success through mocks or direct
JavaScript object mutation.

## Diagnose failures from evidence

Identify first failed milestone, then trace only direct evidence through:

```text
browser console / diagnostics
Applesauce callback and EventStore state
WASM bridge queue
NostrMultiplayerRuntime event handling
LockstepSession::receive
simulation advancement and presentation
```

Use codebase-memory graph tools before text search for code discovery. Compare
host and join fields rather than relying on one browser. Separate:

- product defect;
- relay-specific rejection or outage;
- browser/tooling failure;
- automation failure;
- missing production capability;
- undetermined cause.

Do not speculate. Record root cause only when source, reachable production path,
and observed failure establish it. Otherwise state `root cause undetermined`
and list missing evidence.

Always collect failure artifacts before browser shutdown when possible. If the
existing harness loses evidence on exceptions, rerun headed or add an
audit-only wrapper under `artifacts/`; do not weaken production inputs.

## Write audit report

Create or update dated report with:

```markdown
# Public Nostr multiplayer audit — YYYY-MM-DD

## Scope and build
- Commit:
- Browser / Selenium / Emscripten:
- Relays:
- Local serving port:
- Evidence directory:

## Identity ledger
| Role | Public key / npub | Slot | Distinct | Private material absent |

## Journey ledger
| Milestone | Host | Join | Status | Evidence |

## Relay recovery ledger
| Check | Status | Evidence |

## Problems encountered
### <short factual title>
- Classification: product defect / relay / tooling / automation / missing capability
- First failed milestone:
- Observed behavior:
- Expected behavior:
- Reproduction:
- Proven root cause or `undetermined`:
- Affected production path:
- Evidence:

## Coverage gaps

## Verdict
PASS / PROBLEMS FOUND / PARTIAL / BLOCKED
```

Document every encountered problem, including recovered infrastructure and
automation problems. Deduplicate repeated manifestations by root cause while
listing both affected browsers and all evidence.

Use verdicts strictly:

- `PASS`: complete base journey, terminal result, and relay recovery pass.
- `PROBLEMS FOUND`: one or more confirmed product defects.
- `PARTIAL`: meaningful production stages pass, but required stages remain
  untested or unsupported.
- `BLOCKED`: environment or relay state prevents meaningful gameplay proof.

Never describe multiplayer as working end to end when verdict is not `PASS`.

## Finish

Close both browsers and reap Selenium/Chrome/server processes owned by audit.
Confirm port released. Keep raw evidence in ignored `artifacts/`.

If only audit docs or skill files changed, do not run another compiler/test gate
for commit. Stage only skill/report files, create focused commit, and report:

- verdict and last completed milestone;
- distinct ephemeral identity proof;
- problem count by classification;
- evidence and report paths;
- commands and gate results;
- cleanup confirmation;
- commit hash.

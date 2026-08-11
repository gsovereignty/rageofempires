# Browser multiplayer gameplay audit — 2026-08-11

## Verdict

**BLOCKED.** A fresh two-browser public-relay smoke run passed its implemented
lockstep, relay-recovery, movement, chat/signal, control, and terminal checks.
It did not provide enough gameplay breadth or render telemetry to execute the
required broad gameplay, temporal-motion, and sprite-provenance oracles. No
current product defect was confirmed.

## Scope and configuration

- Source revision: `08b2dbf695e5820ebf8c3ef88754355098c26a76`.
- Packaged artifact exercised: existing `build-web/dist/aoe_web.html` package.
- Browser / driver: Chrome 151.0.7922.76 / Selenium 4.47.0.
- Public relays: `wss://relay.damus.io`, `wss://nos.lol`, and
  `wss://relay.primal.net`.
- Local serving port: 8892; released after the run.
- Host public key:
  `28e493af3db30d6da21c6e2f41dabbdc81b4f9555e8034b38573d1f3a0071c44`.
- Join public key:
  `ba0ec38470490cf30b61acea38b2b77c5e7aadb42aaa41184befe3045df9efc0`.
- Evidence: `artifacts/browser-multiplayer-audits/20260811T145744Z-browser-audit/`.
- Match reference, map seed, scenario/rules digest, lobby revision, viewport,
  device scale, canvas dimensions, and capture cadence were not separately
  materialized in a run ledger. Claims depending on those fields remain
  blocked.

The identities are distinct. Captured evidence contains public keys only; no
private key material was inspected or recorded. No controlled transport or
mock relay was used.

## Coverage matrix

| Phase | Result | Direct evidence or limit |
|---|---|---|
| Public-relay lobby and readiness | PASS | Three configured relays, distinct identities, canonical lobby revision 2. |
| Lockstep stream agreement | PASS | Both final streams contiguous through blue/red sequence 15; missing ranges empty. |
| Host world movement | PASS | Visible host selection/right-click moved entity 3 from `(11,23)` to `(12,23)` identically on both peers. |
| Join world command | BLOCKED | Harness issues no distinct non-empty joiner world command. |
| Gather/build/production/research | UNTESTED | No matching actions in current smoke journey. |
| Combat/damage/death/destruction | UNTESTED | No matching actions in current smoke journey. |
| Chat and allied signal | PASS | Both clients ended with one chat and one signal record. |
| Speed and pause/resume | PASS | Both clients ended resumed at speed 2. |
| One-relay loss | PASS | Equal progress from tick 8 to tick 11. |
| Quorum loss | PASS | Both stopped at tick 12 with reliability status 2 and reason 5. |
| Relay recovery | PASS | Both resumed at tick 13 and reached tick 18 after all relays were restored. |
| Terminal agreement | PASS | Both reached outcome 2 at tick 45 with two agreeing results and hash `save130+ids-fnv1a64:8ac8ba80cd8c9b08`. |
| Frozen terminal tick | PASS, limited evidence | Harness performed the stability assertion; retained JSON contains one final snapshot rather than both timed samples. |
| Temporal motion oracle | BLOCKED | No per-frame authoritative/render positions, movement state, facing, or animation data. |
| Sprite/provenance oracle | BLOCKED | No per-entity asset ID, animation frame, palette, layer, or fallback reason. |
| Seeded long match | UNTESTED | Current journey is a bounded smoke, not a seeded state-driven match. |

## Validated coverage gaps

### BG-01: only host issues a non-empty world command

- Classification: tooling / coverage gap.
- Status: `BLOCKED`, not a product bug.
- Evidence: `tests/web/nostr_multiplayer_smoke_test.py:330-372` selects and
  right-clicks a blue villager through `host_journey`; there is no equivalent
  joiner world action. The journey then covers chat, signal, controls, and
  resignation without gathering, construction, production, research, combat,
  damage, death, or destruction.
- Impact: required stable-network baseline cannot establish that both player
  command streams execute distinct gameplay commands.
- Product root cause: none. This is missing audit coverage.

### BG-02: temporal and production-visual oracles lack evidence

- Classification: tooling / instrumentation gap.
- Status: `BLOCKED`, not a product bug.
- Evidence: `NostrMultiplayerRuntime::Impl::diagnostics_json` exposes living
  blue-villager IDs and authoritative integer tile positions at
  `src/nostr_multiplayer_runtime.cpp:766-831`. Browser telemetry exposes
  aggregate counts, camera, fallback count, and target coordinates at
  `include/aoe/browser_telemetry.hpp:8-43` and
  `src/browser_telemetry_web.cpp:39-63`.
- Missing evidence: entity destination, moving/action state, render position,
  facing, animation ID/frame, asset provenance, palette, layers, and fallback
  reason.
- Impact: ordered-frame motion defects and real-asset-versus-fallback defects
  cannot be confirmed or rejected from this run.
- Product root cause: none. Current evidence interface does not support the
  requested audit oracle.

## Crush Bugs evidence ledger

| Finding | Category | Confirmation | Status | Fix | Regression | Prevention | Result |
|---:|---|---|---|---|---|---|---|
| BG-01 | tooling | Direct harness inspection proves host-only world input and omitted action classes. | BLOCKED | Not requested; no product fix applies. | None added. | Add a production-path audit journey where both clients issue distinct commands and cover required gameplay classes. | PASS review: valid coverage gap; not a product defect. |
| BG-02 | tooling | Runtime and telemetry inspection proves required temporal and provenance fields absent from retained evidence. | BLOCKED | Not requested; no product fix applies. | None added. | Capture correlated per-frame simulation/render/animation/provenance ledgers. | PASS review: valid instrumentation gap; not a product defect. |

No security finding is claimed. For both gaps: no evidenced `BOUNDARY`, no
less-trusted `SOURCE`, no production defect `PATH`, no confidentiality,
integrity, authorization, isolation, or privilege `HARM`, and no
security-relevant `CONTRACT`. A security classification would therefore be
`MISCLASSIFIED`.

## Evidence limitations

Retained run output contains `baseline.json`, host/join terminal screenshots,
and one signal screenshot. It does not contain the skill-required `run.json`,
`actions.jsonl`, per-client state streams, ordered frame sequences,
`motion.json`, sprite-provenance stream, console JSON files, or a
`first-failure.json`. `baseline.json` also does not embed packaged build
identity. These omissions prevent a broad `PASS` and prevent confirmation of
any motion or visual candidate.

No fidelity expectation was selected from disputed evidence. Decompiled
multiplayer strings were consulted read-only but did not establish the missing
gameplay or render claims and were not used to classify a defect.

## Cleanup and repository state

Both audit WebDrivers exited and port 8892 was released. The pre-existing
server on port 8888 was not touched. Runtime artifacts remain untracked under
`artifacts/`. Existing unrelated `.codebase-memory` working-tree changes were
preserved.

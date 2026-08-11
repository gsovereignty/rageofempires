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

## Tooling follow-up

Later on 2026-08-11, tooling changes implemented the missing evidence path:

- browser targets and aggregate economy/research telemetry are now relative to
  the local multiplayer slot, allowing the Red joiner to use ordinary canvas
  controls;
- Nostr diagnostics now include both players' units, buildings, economies,
  populations, and current deterministic state hash;
- the production renderer now publishes per-frame visible-entity simulation
  position, destination, movement/action/facing/animation state, interpolated
  render position, and exact successful legacy draw layers with resource ID,
  frame, palette, flip, and destination rectangle;
- the multiplayer harness now issues distinct Blue and Red movement commands,
  includes a simultaneous-command phase, captures correlated render frames,
  runs motion/provenance assertions, and writes the complete audit evidence
  bundle.

The oracle accepts a frame only when both render snapshots match an unchanged,
equal authoritative tick and hash. Screenshot evidence is retained only when
post-capture telemetry still identifies that same frame and tick. It rejects
unmapped assets, unexpected sprite resources, peer layer/frame or animation
state divergence, animation reversal/skips, frozen moving animation,
teleport-sized displacement, duplicate transient identities, and missing
entities when cameras match. Live units, buildings, resources, unit deaths,
building rubble, native projectiles, and native impacts now have direct
expected-asset mappings. Death, rubble, projectile, and impact scopes use
stable simulation-owned effect IDs; projectile identity survives its
transition into an impact. IDs and the monotonic sequence persist in save
version 131 and participate in deterministic multiplayer hashes. Version 130
saves receive deterministic IDs on load, while duplicate version 131 IDs are
rejected.

Intentional procedural fallback geometry and commercial projectile/impact
asset contracts remain explicit blocking cells. Those paths emit unsupported
or unproved provenance and cannot silently pass the production visual oracle.

Native `make`, packaged WebAssembly build, native persistence regression, and
17 focused render-oracle tests pass.
A packaged single-browser production probe observed six visible entities, all
with direct legacy sprite provenance. Two fresh public-relay multiplayer runs
could not verify the new two-client journey: nos.lol returned HTTP 502, Damus
returned HTTP 503, then the second match suspended both peers equally at tick
4 for peer silence with equal state hashes. The historical verdict therefore
remains **BLOCKED** until the same packaged two-client path completes over a
working public-relay quorum.

## Full-match harness extension and rerun

The harness now requires each peer to gather gold, construct and complete a
house through the visible command panel, select its barracks, train militia,
research Man-at-Arms, attack the opposing town center, and continue until a
natural conquest outcome. Resignation is no longer an audit shortcut. Relay
loss and recovery run only after core gameplay evidence, and the retained
combat frames feed the same motion and sprite-provenance oracles.

Two directly observed harness defects were corrected while exercising this
path:

- `H` on the villager root command page invokes Garrison. The harness now
  clicks root grid slot 0 (Economic) and economic grid slot 0 (House), matching
  `hud_layout::command_button` and `build_selection_panel`.
- The initially visible multiplayer diagnostics panel covered world targets.
  The harness now uses the production F4 toggle before gameplay and requires a
  nonzero `selectedBuilding` before issuing barracks commands.

The production browser telemetry now also publishes the opposing town-center
center and hit points relative to the local multiplayer slot. This lets each
peer target the actual victory-critical building and lets the harness prove
damage before accepting a terminal result.

Validation evidence:

- `build-web/selenium-venv/bin/python tests/web/test_nostr_multiplayer_audit_tools.py`:
  20 tests passed.
- Packaged WebAssembly target rebuilt successfully after telemetry changes.
- Run `20260811T184000Z-full-gameplay` passed gathering and house construction,
  then exposed the diagnostics-overlay barracks-selection defect.
- Runs `20260811T185000Z-full-gameplay` and
  `20260811T190000Z-full-gameplay` each timed out before gameplay at
  `deterministic lockstep tick exchange` against the three ordinary public
  relays. Neither run supplies product-bug evidence.

The verdict remains **BLOCKED**. The harness now expresses the complete
two-sided natural-victory audit, but no clean public-relay run has yet completed
that entire path. Runtime evidence is under
`artifacts/browser-multiplayer-audits/`; it remains intentionally untracked.

## Facing and gathering-presence oracles

The temporal oracle now independently recomputes canonical logical facing from
each unit's previous and current authoritative tile using the direction
quantization proved by original `FUN_0058da80`. Production render diagnostics
publish each resolved animation's direction count, so both 8-direction land
units and 16-direction units are checked without a hardcoded asset assumption.
A shared wrong facing now fails even when both clients agree.

Gathering render diagnostics now retain target tile, target kind and existence,
map membership, remaining amount, visibility, stable render entity ID,
building/unit target IDs, return state, and carried resource. While a unit is
actively gathering a map node, hunt/herd unit, or resource building, the oracle
requires an existing in-map target with positive remaining amount and, when
visible to that client, a matching rendered entity with production provenance.
Host and join must also agree on deterministic gathering fields. Each player's
full-gameplay preparation now retains and analyzes a correlated gather-frame
burst in `motion.json`, state ledgers, and `sprite-provenance.jsonl`.

Focused audit-tool validation passes 27 tests, covering all eight canonical
land directions, a 16-direction case, wrong facing, valid visible gathering,
depleted gathering, and missing visible resource presentation. Packaged
WebAssembly build succeeded. Public-relay run
`20260811T203000Z-facing-gather-oracle` timed out before gameplay at
`deterministic lockstep tick exchange`; therefore live public-relay acceptance
remains **BLOCKED**, not failed.

## Complete-audit rerun at `90b85b4`

Two fresh runs used packaged artifact SHA-256
`5ac6a546da35510de18956ca83550e5de54851bf29249d77b144bd643d54629c`
with Chrome 151.0.7922.76 and distinct ephemeral identities:

- `20260811T193346Z-complete-audit`, using Damus, nos.lol, and Primal,
  reached the canonical lobby but timed out at `deterministic lockstep tick
  exchange`. Its complete failure bundle is retained under the matching
  artifact directory.
- `20260811T194000Z-complete-audit-retry`, using nostr.band, Snort, and
  nostr.mom, timed out earlier at `host relay quorum and EOSE`. While preserving
  the failure, the harness attempted `diagnostics(join)` after that browser no
  longer exposed `Module`; the resulting JavaScript exception prevented bundle
  finalization for this retry.

No gameplay phase ran, so neither run exercised facing, gathering, production,
combat, destruction, relay-chaos recovery, or natural victory. These are
infrastructure-blocked cells, not passes. No product gameplay defect was
confirmed. A separate audit-harness cleanup defect is confirmed: failure
evidence collection assumes `Module` exists and can mask the primary failure
when a browser leaves the packaged game context. Root cause is the unguarded
identifier access in `diagnostics()` combined with an unconditional diagnostics
call in the failure handler. Overall verdict remains **BLOCKED**.

## Public relay pool replacement

Removed the six endpoints implicated by blocked runs: Damus, nos.lol, Primal,
nostr.band, Snort, and nostr.mom. Also removed the unresolved `relay.nostr.bg`
hostname and paid `relay.orangepill.dev` endpoint from defaults. Production
runtime, browser launch UI, and audit harness now share this pool:

- `wss://nostr-pub.wellorder.net`
- `wss://nostr.oxtr.dev`
- `wss://nostr.bond`
- `wss://relay.nostr.net`
- `wss://yabu.me`
- `wss://relay.nostr.wirednet.jp`
- `wss://relay.nostr.info`
- `wss://nostr.sathoarder.com`
- `wss://relay.wavlake.com`
- `wss://relay.noswhere.com`

All ten returned live NIP-11 documents and independently completed a WebSocket
`REQ`/`EOSE` probe on 2026-08-11. Packaged run
`20260811T200000Z-new-relay-pool` then passed relay establishment, deterministic
lockstep, movement, and early full-gameplay gathering/construction. It stopped
later at the existing host barracks-selection automation timeout. This proves
the replacement pool reaches meaningful gameplay; it does not yet prove full
match completion or relay-loss recovery. Overall audit verdict remains
**BLOCKED** on later harness coverage, not relay establishment.

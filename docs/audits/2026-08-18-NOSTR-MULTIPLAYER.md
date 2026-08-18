# Public Nostr multiplayer lobby diagnostic — 2026-08-18

## Scope and build

- Build base: `1886f09` plus the lobby fix recorded by this report
- Browser: Chrome 151.0.7922.76
- Relays: exact ordered 20-relay production pool from
  `resources/nostr-relays.json`
- Relay-pool SHA-256:
  `c0251c7773ce5b7dd81c442d29e5fa4d36f65f48abee6b1362475b830e55ca32`
- Local serving port: 8891
- Evidence: `artifacts/nostr-multiplayer/2026-08-18-lobby-visibility/`
- Scope stopped after two independent browsers converged on the occupied lobby.
  Gameplay and terminal synchronization were not tested.

## Identity ledger

| Role | Public key | Slot | Distinct | Private material absent |
| --- | --- | --- | --- | --- |
| Host | `9ecac2f513184e9013dab834877593a4bbc610e73ecff092ee650ed5d5bd027b` | Blue | Yes | Yes |
| Join | `527ef67bd5265e4c4f7d6dfae433465afc1066ef63a3649d8617d8284b258b3f` | Red | Yes | Yes |

## Journey ledger

| Milestone | Host | Join | Status | Evidence |
| --- | --- | --- | --- | --- |
| Exact production relay identity | 20 relays, canonical digest | 20 relays, canonical digest | PASS | `joined-lobby.json` |
| Relay quorum and EOSE | 14 usable / 14 EOSE | 13 usable / 11 EOSE | PASS | `joined-lobby.json` |
| Initial lobby publication | Two relay accepts | Observed revision 1 | PASS | `joined-lobby.json` |
| Join publication | Observed join | Two relay accepts | PASS | `joined-lobby.json` |
| Occupied canonical lobby | Revision 2, active | Revision 2, active | PASS | `joined-lobby.json`, `host-joined-lobby.png`, `join-joined-lobby.png` |
| Peer roster | Join key assigned to Red | Own key assigned to Red | PASS | `joined-lobby.json`, screenshots |
| Waiting-session discovery | Published open lobby | Listed host without invite input | PASS | `discovered-lobby.json`, `join-session-list.png` |
| Direct selected join | Accepted selected peer | Selected visible session button | PASS | `discovered-lobby.json`, `host-discovered-join.png`, `join-discovered-join.png` |

## Problems encountered

### Relay-status flood prevents lobby publication

- Classification: product defect
- First failed milestone: initial lobby publication
- Observed behavior before correction: both browsers reached 19 EOSE relays in
  JavaScript, while C++ stayed at `relay_quorum_lost`, lobby revision 0, and no
  publication intent for 180 seconds.
- Expected behavior: C++ receives current relay readiness and publishes once
  quorum and backfill are ready.
- Proven root cause: `AoeNostrClient.observeRelayStatus` iterated and emitted
  every entry in Applesauce's full relay-status map on every status update.
  These duplicates exceeded `nostr_bridge_max_queued_messages` (256).
  `enqueue_bounded` silently rejected later messages, including ready states,
  leaving `NostrMultiplayerRuntime::Impl::usable_relay_count()` below quorum.
- Production correction: emit bridge relay status only when its four
  bridge-relevant fields change. C++ now defers initial publication until relay
  quorum and EOSE quorum exist. Publication attempts still target every ready
  relay but complete when production quorum accepts, so one silent relay cannot
  hold the lobby open path indefinitely.
- After correction: same packaged build and public pool reached revision 2 in
  6 seconds with both clients active.
- Evidence: `joined-lobby.json` and retained before/after run history in the
  evidence directory.

### Occupied lobby row omitted peer identity

- Classification: product defect
- Proven root cause: `render_multiplayer_presentation` did not read
  `LockstepPlayerConfig::peer_id` when formatting non-local rows.
- Correction: open rows render `[OPEN SLOT]`; occupied rows render a peer-key
  prefix. Both production screenshots show two occupied team rows.
- Evidence: `host-joined-lobby.png`, `join-joined-lobby.png`.

### One production relay refused connection

- Classification: relay/environment
- Relay: `wss://offchain.pub/`
- Effect: none on lobby quorum. Exact production pool remained configured on
  both clients; working relays carried publications and observations.
- Evidence: `joined-lobby.json` browser console and relay status.

## Waiting-session discovery

- Join mode accepts an empty invite reference and opens a global kind-30078
  subscription on the exact production relay pool.
- Discovery filters by application tag, then validates protocol version,
  event/address identity, host authorship, expiry, two-hour production lifetime,
  compatibility digest, ordered relay identity, revision, and open status.
- Events deduplicate by host and match; relay copies merge into one selectable
  session and newer revisions replace older revisions. Full or expired sessions
  are removed.
- Selecting a visible session switches the same client from discovery to exact
  host/match subscriptions and performs the existing join handshake.
- Invite reference remains available as optional direct-entry/deep-link input.
- Production acceptance entered no invite reference, selected host from visible
  list, and reached revision 2 with both clients active in 9 seconds.
- Evidence: `discovered-lobby.json`, `join-session-list.png`,
  `host-discovered-join.png`, and `join-discovered-join.png`.

## Regression gates

- `npm test`: 11/11 pass
- `npm run typecheck`: pass
- `make`: pass
- packaged `aoe_web` build: pass
- two-browser public-relay lobby acceptance: pass

## Coverage gaps

- Ready/start, gameplay commands, lockstep hashes, recovery, and terminal result
  remain outside this lobby-only run.

## Verdict

PARTIAL for the full multiplayer protocol skill. Lobby acceptance itself PASS:
host publication, global discovery, selectable waiting-session list, direct
join without invite input, canonical revision 2, occupied roster, and active
relay reliability all passed through the packaged production path.

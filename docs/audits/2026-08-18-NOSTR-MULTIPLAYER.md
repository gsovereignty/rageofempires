# Public Nostr multiplayer lobby diagnostic — 2026-08-18

## Scope and build

- Commit tested: `68e00b0`
- Browser: Chrome 151.0.7922.76
- Relays: packaged 20-relay pool from `resources/nostr-relays.json`
- Local serving port: 8891
- Evidence: `artifacts/nostr-multiplayer/2026-08-18-lobby-visibility/`
- Scope stopped at lobby discovery; gameplay was not tested.
- One earlier three-relay attempt was configuration-invalid and is excluded.
  Acceptance runs must use the exact packaged 20-relay pool.

## Journey ledger

| Milestone | Status | Evidence |
| --- | --- | --- |
| Host ephemeral identity | PASS | `joined-lobby.json` contains a 64-character public key and no private material |
| Host relay EOSE/quorum | BLOCKED | `joined-lobby.json` records no EOSE relays after 180 seconds |
| Joiner launch | NOT REACHED | Host produced no match reference |
| Joined roster rendering | NOT REACHED | Relay prerequisite failed first |

## Problems encountered

### Occupied lobby row omits peer identity

- Classification: product defect
- First affected milestone: joined roster presentation
- Observed behavior: lobby renderer labels every non-local row only `[PEER]`.
- Expected behavior: open slots and occupied peer identities are distinguishable.
- Proven root cause: `render_multiplayer_presentation` receives the live
  `LockstepSessionConfig`, but its `player_line` formatter does not read
  `LockstepPlayerConfig::peer_id`. `handle_join` stores the accepted joiner's
  public key in `config_.red.peer_id`, and the main loop copies that config to
  the presentation each frame.
- Change under verification: open peer rows render `[OPEN SLOT]`; occupied rows
  render `[PEER <12-character public-key prefix>]`.
- Regression check: `aoe_ui_perspective_tests` passes.
- Production verification: incomplete because public relay EOSE/quorum failed
  before a joiner could enter the lobby.

### Public relay quorum unavailable during verification

- Classification: relay/environment
- First failed milestone: host relay EOSE/quorum
- Observed behavior: two attempts timed out after 180 seconds; canonical-pool
  diagnostics reported `eoseRelays: []`, `lobbyRevision: 0`, and no match
  reference.
- Root cause: undetermined. Browser diagnostics did not expose per-relay state
  or a fatal error for this run.
- Evidence: `joined-lobby.json`

### Relay configuration identity gate

- Production pool: exact ordered 20-relay list in
  `resources/nostr-relays.json`.
- SHA-256: `c0251c7773ce5b7dd81c442d29e5fa4d36f65f48abee6b1362475b830e55ca32`.
- Host and join browser diagnostics both reported all 20 relays and this exact
  digest before lobby work.
- Acceptance CLI rejects `--relays`; visual audit and display-matrix
  orchestrators no longer pass relay subsets.
- Packaged browser bridge rejects any runtime list or digest differing from
  packaged production configuration.
- Verification reached 19 relay EOSE observations on each independent browser.
  Joined lobby revision 2 was not reached, so broader lobby acceptance remains
  blocked and is not claimed here.

## Verdict

BLOCKED. Source defect corrected and packaged build succeeds, but joined-lobby
production verification remains incomplete until public relay quorum succeeds.

# Localhost multiplayer lobby

The SDL frontend presents a bounded pregame lobby for the typed
`LockstepSessionConfig` handshake.

Displayed immutable metadata includes both player slots, canonical player
colors, civilizations, teams, scenario and rules digests, build/schema/save
versions, tick cadence, input delay, deterministic seed, transport state, and
ready state.

Controls:

- `R` or the left button marks the local player ready. Ready is locked once
  sent because the current protocol has no unready frame.
- Host presses `Enter` or the Start button after both exact configurations
  have been accepted and both players are ready.
- Join waits for the host Start frame.

The panel uses reconstruction-drawn beveled framing over the existing
archive-backed game presentation. No archive asset has been identified as an
exact multiplayer-lobby frame, and no original UI layout or wire-protocol
compatibility is claimed.

Automated SDL smoke uses `AOE_MULTIPLAYER_AUTO_READY=1` on both processes and
`AOE_MULTIPLAYER_AUTO_START=1` on the host. Interactive runs do not enable
either behavior by default.

## Browser public-relay controls

Browser Nostr matches keep configured relay list immutable, but Public Nostr
match panel exposes a `Manage relay connections` disclosure during play. Each
configured relay has a visible `Disconnect` or `Restore` button.

- Disconnect stops that relay's match subscription and socket, excludes it
  from publication, and clears its EOSE state.
- With two of three relays still active, quorum remains two and play continues.
- Below configured quorum, runtime suspends before scheduling more turns.
- Restore opens a fresh stored-event subscription. Play remains blocked until
  required relay quorum is connected, ready, and has reached EOSE.
- A signed turn that failed publication quorum remains cached by event ID.
  After restored-relay EOSE, runtime republishes exact signed bytes and remains
  in `backfill_incomplete` until republication reaches quorum. It never creates
  replacement input.
- EventStore and sender-sequence checks make multi-relay duplicates
  idempotent. Conflicting logical input still suspends session.

Controls are ordinary production UI. They are useful for changing relay
connectivity and diagnosing a degraded match; they are not a test-only hook.

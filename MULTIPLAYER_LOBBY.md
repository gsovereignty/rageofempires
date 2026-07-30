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

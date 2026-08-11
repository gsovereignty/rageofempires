# Allied map signals

The original Microsoft manual says map signals can be sent to allies (manual,
pp. 54–55; evidence inventory in `../fidelity/MULTIPLAYER_FIDELITY.md`). The reconstruction
uses `Alt+F` or the visible `ALLY SIGNAL` button, then a left click on an
explored world tile. These controls are reconstruction choices; the available
evidence proves the feature and audience, not an exact original binding.

Signals are bounded ephemeral side-channel records containing host-assigned
sequence, sender, audience, and tile. They do not enter simulation state,
deterministic hashes, typed gameplay commands, or replay files. The host routes
allied signals only when negotiated teams match. Both peers display them in
host sequence order.

The sender may emit four signals per rolling two seconds. Negative coordinates,
out-of-map clicks, and unexplored clicks are rejected. Received history is
bounded to 64 records. A signal pulses in the sender's player color on the
world and minimap for six seconds. Its latest signal-log row says `CLICK TO
VIEW`; clicking that row centers the camera.

Single-player uses the same marker locally without network traffic. No signal
sound plays: archive/decompiled strings include flare-like names, but no exact
verified sound-resource mapping has been established, so the UI does not guess.

`multiplayer_signal_tests` covers codec bounds, allied host routing, ordering,
rate/input rejection, and hash/replay exclusion. The
`multiplayer_signal_sdl_smoke` test launches two SDL processes and verifies
three signals reach both logs in the same host-assigned order without changing
their shared simulation hash.

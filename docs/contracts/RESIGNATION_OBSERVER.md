# Resignation and observer contract

UI controls: [resignation and observer UI](../ui/RESIGNATION_OBSERVER_UI.md).

`PlayerControllerState` separates authoritative participation from local
viewing:

- `active`: commands and multiplayer chat may be submitted;
- `resigned`: authoritative lockstep state after `ResignCommand`;
- `observer`: local runtime/view state for a resigned controller or a
  non-playing perspective.

Resignation remains a deterministic `GameCommand`. The first accepted resign
changes the authoritative controller state and applies the existing terminal
outcome policy. A surviving allied controller receives victory in the current
two-player team model. Repeated resignation and every later state-changing
command from that player are rejected.

`is_visible` and `is_explored` retain ordinary simulation fog semantics after
resignation. UI code must use `is_visible_to_controller` and
`is_explored_to_controller`: they match ordinary fog while active, then expose
the full bounded map to a resigned/observer perspective. This separation
prevents observer visibility from affecting targeting, AI, replay state, or
pre-resignation ownership/fog decisions.

`LocalhostMultiplayerRuntime::local_controller_state()` reports the local
runtime transition to `observer`. Pending commands are cleared; new commands
and chat return `false`. Transport pumping, committed lockstep turns, hashes,
and viewing continue.

Current Save110 persists blue/red authoritative controller states; Save106
introduced them. Save105 and older
files migrate both players to `active`, matching their former implicit state.
Controller records accept only `active`, `resigned`, or `observer`; malformed
values reject the save. Replay v62 needs no framing change because resignation
already has a deterministic command record.

Focused tests cover fog before resignation, full-map controller perspective
afterward, post-resign command rejection, replay divergence on an invalid
post-resign command stream, Save106 round-trip, allied outcome behavior, and
multiplayer command/chat refusal.

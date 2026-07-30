# Movement presentation timing

## Implemented contract

Gameplay remains a deterministic fixed-step simulation. The SDL frontend uses
one cadence value for both simulation scheduling and presentation
interpolation:

- slow single player: 400 ms;
- normal single player: 200 ms;
- fast single player: 100 ms;
- live multiplayer: the lockstep runtime's effective cadence.

Elapsed frame time is retained in an accumulator. Every complete fixed step is
processed, including catch-up after a late frame, and the remaining fraction
positions moving units between their previous and current authoritative tiles.
Paused, modal, editor, and frontend states discard accumulated simulation time
so resuming does not replay wall-clock time spent outside active gameplay.

Walking sprite frames use active-gameplay milliseconds with a stable per-unit
phase. They no longer inherit the coarse simulation tick rate. This clock is
presentation-only and is absent from saves, replays, deterministic hashes, and
multiplayer commands.

Movement animation follows physical displacement, not pending order state.
Fixed-point cavalry, ships, formation members, and other units can retain a
move order during an accumulator, cooldown, regroup, or blockage tick without
changing position. Those waits render a standing frame instead of cycling the
moving graphic on one tile. A unit selects moving art for the presentation
interval immediately after an authoritative position change.

Blocked fixed-point movement restores the accumulator value from before the
blocked tick. Cavalry, ships, unique units, and paced formation members cannot
bank movement credit against an occupied or newly invalid path and later
release it as a burst when the obstruction clears.

## Original evidence

The supplied installer executable is a dummy, so substantial behavior evidence
comes from the supplied 2013 patched game binary decompile. Its application
idle path calls the game service repeatedly (`FUN_004f1fe0`), uses
`timeGetTime()` throughout that path, and stores object coordinates as
floating-point values. This supports decoupling visual cadence from coarse
logical turns, but does not prove exact original frame durations or movement
integration mathematics.

## Remaining parity gaps

- Authoritative reconstruction positions remain integer tiles. Smooth motion
  is presentation interpolation, not original-style sub-tile simulation.
- Fractional-speed units still alternate displaced and non-displaced logical
  ticks. Standing art on the latter removes on-tile oscillation, but true
  continuous sub-tile integration remains open.
- Walking art currently uses a bounded 100 ms frame period because exact
  per-graphic timing metadata has not been proved and integrated.
- A very late frame runs every due deterministic step and can therefore cause
  one long catch-up frame. No time is discarded; a future presentation-only
  policy may cap work without changing simulation results.
- Camera scrolling remains frame-based and is outside this movement timing
  contract.

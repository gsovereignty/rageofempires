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
- Walking art currently uses a bounded 100 ms frame period because exact
  per-graphic timing metadata has not been proved and integrated.
- A very late frame runs every due deterministic step and can therefore cause
  one long catch-up frame. No time is discarded; a future presentation-only
  policy may cap work without changing simulation results.
- Camera scrolling remains frame-based and is outside this movement timing
  contract.

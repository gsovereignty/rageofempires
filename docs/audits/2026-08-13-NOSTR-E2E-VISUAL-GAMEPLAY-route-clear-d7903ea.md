# Nostr multiplayer gameplay audit

- Run: `route-clear-d7903ea`
- Started UTC: `2026-08-13T11:28:25.535639+00:00`
- Verdict: **BLOCKED**

## Result

Source commit `d7903ea2e466467b86386419209761b11e471aa2` moved secondary
formation villagers away from canonical route destinations. Relevant route
tests (6 cases), multiplayer audit-tool tests (67 cases), repository `make`,
and production web rebuild passed before launch.

Public-relay after-verification did not reach the corrected collision:

- attempt `20260813T112830Z-9043d08ffe75` timed out waiting for matching
  replacement world movement on both peers before direction capture;
- attempt `20260813T113204Z-d016a73b7f57` left route subject id 10 idle at
  `(34,8)` after the ordinary UI reset command to unoccupied `(28,12)` never
  appeared in either peer's synchronized state.

Both attempts and their full browser, relay, state, request, console, package,
source, and partial-coverage evidence remain under
`artifacts/nostr-e2e-visual/route-clear-d7903ea/attempts/`. Infrastructure
versus input-delivery root cause for the missing commands is undetermined.
Therefore the scenario correction is implemented and packaged, but production
before/after verification and full TODO completion remain open.

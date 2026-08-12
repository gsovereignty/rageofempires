# Nostr multiplayer gameplay audit

- Run: `20260812T174030Z-8b9c25e0b5de`
- Started UTC: `2026-08-12T17:40:30.939527+00:00`
- Verdict: **BLOCKED**

## Result

Production package launched through two Chrome clients and public relays. Run
retained `first-failure.json`: unit 3 never accepted route destination
`(20, 24)`. Post-processing was interrupted after screenshot evidence reached
381 MB. No coverage cell is classified as passed.

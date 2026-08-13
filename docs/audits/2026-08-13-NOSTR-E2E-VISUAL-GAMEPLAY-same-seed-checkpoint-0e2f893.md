# Nostr multiplayer gameplay audit

- Run: `same-seed-checkpoint-0e2f893`
- Started UTC: `2026-08-13T06:52:11.901538+00:00`
- Verdict: **FAIL**

## Result

The pinned run completed all 24 Join direction captures and reached Host
direction 0. One Host direction-0 crop was ambiguous and correctly `BLOCKED`.
During automatic recapture, the unit advanced from `(31,7)` to its destination
`(32,8)` before the next drawable-step wait, so the wait could never succeed.
The retained first failure is `timed out waiting for entity 1 drawable
direction 0`.

Prefix minimization was interrupted after the retained synchronized positions
proved this audit-journey race. The implementation now preserves the blocked
crop and lets later laps fill the cell when arrival wins that race.

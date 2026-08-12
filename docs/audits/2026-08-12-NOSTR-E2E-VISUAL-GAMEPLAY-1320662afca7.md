# Nostr multiplayer gameplay audit

- Run: `20260812T175343Z-1320662afca7`
- Started UTC: `2026-08-12T17:56:48.060807+00:00`
- Verdict: **BLOCKED**

## Result

Run stopped before acceptance completed. Infrastructure versus product classification remains unproved pending evidence review.

Primary harness failure: `Failure: timed out waiting for host Nostr initialization; last=None`

Browser diagnostics from the same packaged startup recorded
`could not open scenario: /resources/browser-nostr-visual.scenario`. The generated
asset directory contained the scenario, but `aoe_web.data` was older because the
web link dependencies did not include the generated asset manifest. This run is
retained as the pre-correction production-path reproduction; it contains no pass
verdict.

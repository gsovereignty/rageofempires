# Nostr multiplayer gameplay audit

- Run: `20260813T024556Z-baf62ed0ff94`
- Started UTC: `2026-08-13T02:45:56.861044+00:00`
- Verdict: **INTERRUPTED**

## Result

Durable destinations allocated before browser launch. Bounded runner rotated
two attempts after correctly classifying reliability stalls as `BLOCKED`.
Third attempt was manually interrupted during erroneous minimization after
retained publication acknowledgements showed two relays rejected required
lobby event kinds and only one relay accepted each event. Evidence:
`artifacts/nostr-e2e-visual/20260813T024556Z-baf62ed0ff94/attempts/`.
No complete gameplay verdict was reached.

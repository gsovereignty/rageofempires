# Nostr multiplayer gameplay audit

- Run: `production-fixture-d6d0c89`
- Started UTC: `2026-08-13T10:08:34.823921+00:00`
- Verdict: **PROBLEMS FOUND**

## Result

Rebuilt production package pinned `aoe_web.data` SHA-256
`69795670c799dbd0e2fa53821f1009fd2583c792a852e6b19646833c7831d690`
at source commit `d6d0c89461fcfc6ddb9b397bac989c705331b910`. This proves the
passive-militia fixture change from `8cbf43f` reached the package.

Attempt `20260813T101231Z-63fc0b3388e3` completed all 24 Join and all 24
Host direction captures, then failed at tick 3650 in Join's
`quantization-boundary-vectors` route: subject villager id 9 was commanded to
`(30,16)`, which friendly villager id 11 already occupied. Both peers agreed
that id 9 stopped at `(28,12)` while id 11 remained at `(30,16)`. Root cause
is therefore a scenario fixture collision, not relay divergence. Evidence is
retained under
`artifacts/nostr-e2e-visual/production-fixture-d6d0c89/attempts/20260813T101231Z-63fc0b3388e3/`.

Automatic prefix minimization was stopped after retained candidates proved it
reruns the full direction journey for every binary-search point. Original
244-action causal prefix remains in `causal-replay-prefix.json`.

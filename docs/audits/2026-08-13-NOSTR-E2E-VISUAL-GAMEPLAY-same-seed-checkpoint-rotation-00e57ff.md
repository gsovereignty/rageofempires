# Nostr multiplayer gameplay audit

- Run: `same-seed-checkpoint-rotation-00e57ff`
- Started UTC: `2026-08-13T07:33:44.084055+00:00`
- Verdict: **FAIL**

## Result

The run completed all Join directions, retained packaged wrong-direction and
wrong-position mutation failures for both peers, then lost Blue villager id 1
during Host lap 0. Both peers remained synchronized at tick 3363 while Blue
population fell from 17 to 16. Red militia id 12 had autonomously crossed from
its aggressive fixture `(35,6)` to `(25,16)`, beside the villager's last route
segment. Failure evidence persists at
`artifacts/nostr-e2e-visual/same-seed-checkpoint-rotation-00e57ff/attempts/20260813T073345Z-8d2ed8afb96b/first-failure.json`.

Minimization was interrupted after synchronized production state proved the
scenario-fixture collision. The patrol/combat militia fixtures are now passive
until their later explicit visible commands.

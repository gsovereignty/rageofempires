# Nostr multiplayer gameplay audit

- Run: `same-seed-checkpoint-rotation-e50296f`
- Started UTC: `2026-08-13T06:22:32.583446+00:00`
- Verdict: **FAIL**

## Result

Two attempts retained. First was public-relay `BLOCKED`. Second reached 27
semantic direction captures, then failed because both direction-0 baselines
selected the expected direction but exceeded the audit's absolute pixel score
ceiling. See `attempts.json` and the second attempt's `first-failure.json`.

This exploratory run launched from `e50296f`, while tracked audit sources
changed before finalization. It is diagnostic evidence, not completion proof.
Automatic prefix minimization was stopped and classified `BLOCKED` because a
candidate from changed provenance cannot prove the original failure.

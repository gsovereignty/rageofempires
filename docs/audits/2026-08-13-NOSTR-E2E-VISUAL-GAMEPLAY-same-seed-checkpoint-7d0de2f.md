# Nostr multiplayer gameplay audit

- Run: `same-seed-checkpoint-7d0de2f`
- Started UTC: `2026-08-13T08:02:38.055035+00:00`
- Verdict: **BLOCKED**

## Result

User requested stop after current fixture change was exercised. The journey
completed all 24 Join direction captures, all eight Host lap-0 directions, and
two Host lap-1 directions. Host villager id 1 remained present through tick
3258, past prior failure segment where aggressive Red militia id 12 had killed
it. However, retained `build-web/dist/aoe_web.data` hash
`464e4b98bc5bd609112891c22af21e026ad5120abcbb84a887c864b470b0af95`
came from package built before commit `8cbf43f`; this run therefore does not
prove fixture change reached production package. Full checkpoint journey was
intentionally not completed. Production fix and full TODO completion remain
unverified. Evidence persists under
`artifacts/nostr-e2e-visual/same-seed-checkpoint-7d0de2f/attempts/20260813T080240Z-b4117f998b06/pixel-oracle/`.

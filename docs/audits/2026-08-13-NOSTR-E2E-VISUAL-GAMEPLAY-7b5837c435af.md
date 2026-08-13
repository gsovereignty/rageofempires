# Nostr multiplayer gameplay audit

- Run: `20260813T115948Z-7b5837c435af`
- Started UTC: `2026-08-13T12:03:33.233247+00:00`
- Verdict: **BLOCKED**
- Source commit: `ef44a0c0d17d3baae7ef0a91626d315fe7991965`
- Packaged build: `build-web/dist/aoe_web.html`
- Browser: Chrome `151.0.7922.109`
- ChromeDriver: `151.0.7922.138`
- Viewport / DPR / zoom: `1280x900` / `1.0` / `1.0`
- Action seed: `11055785183250`
- Public relay quorum: `wss://nostr-pub.wellorder.net`,
  `wss://nostr.oxtr.dev`, `wss://nostr.bond`
- Evidence: `artifacts/browser-multiplayer-audits/20260813T115948Z-7b5837c435af/`

## Result

Run stopped before acceptance completed. Infrastructure versus product classification remains unproved.

Primary failure: `Failure: timed out waiting for matching replacement world movement on both peers; last=None`

## Completed evidence

- Two independent Chrome profiles and WebGL 2 renderers launched.
- Both packaged game instances reached tick `0`; one state record exists for
  each peer.
- The browser player attempted `19` ordinary UI actions before the timeout.
- Initial whole-game and per-entity overlap captures exist for both peers.
- Native `make` and packaged `make web-build` completed successfully before
  launch.

## Coverage ledger

| Phase | Status | Evidence |
|---|---|---|
| Stable two-sided gameplay | BLOCKED | No matching replacement world movement on both peers |
| Temporal motion | UNTESTED | `correlated-frames.jsonl` contains no records |
| Sprite provenance / fallback | UNTESTED | `sprite-provenance.jsonl` contains no records |
| Economy, construction, production, research | UNTESTED | Stable movement prerequisite failed |
| Combat, damage, death, destruction | UNTESTED | Stable movement prerequisite failed |
| Relay loss and recovery | UNTESTED | Stable gameplay prerequisite failed |
| Natural victory and terminal hash | UNTESTED | Match did not advance beyond tick `0` |

## Finding classification

No product bug is confirmed by this run. The first failed milestone is stable
two-peer movement. Available evidence does not distinguish product behavior,
public-relay behavior, or automation behavior, so root cause is
`undetermined`. No rendering, motion, animation, or gameplay pass/fail count is
claimed beyond the coverage ledger above.

## Cleanup

Both ChromeDriver processes and the audit-owned static server exited. Port
`8892` was released. Private signer material was neither inspected nor retained.

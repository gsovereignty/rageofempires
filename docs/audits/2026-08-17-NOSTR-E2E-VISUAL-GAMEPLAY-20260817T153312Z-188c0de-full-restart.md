# Nostr multiplayer gameplay audit

- Run: `20260817T153312Z-188c0de-full-restart`
- Attempt: `20260817T171751Z-e7e3daaa9e57`
- Source: `188c0de7b0fc38160ebcc793e17e3f17999b54ed`, based on requested `e841584` harness restart
- Attempt UTC: `2026-08-17T17:17:51.274938+00:00` to `2026-08-17T18:41:20.122067+00:00`
- Verdict: **BLOCKED**

## Configuration

- Public relays: `wss://relay.nostr.net`, `wss://relay.nostr.wirednet.jp`, `wss://nostr.sathoarder.com`
- Host public key: `6696a9d7212cb7b656ff5b33e30e5781d4ee2b74c4c24a1cfda9215acff5e424`
- Join public key: `821e61aaf1d4e0ea1a680cfb0716f9f843cc44ec0dbe9f9ba6bdf548f4f058a1`
- Browser/driver: Chrome `151.0.7922.138` / ChromeDriver `151.0.7922.138`
- Browser isolation: two distinct temporary Chrome profiles; `--mute-audio`
- Viewport: `1280x900`; DPR `1.0`; zoom `1.0`
- Seed: `11055785183250`
- Wrapper limits: retry budget `2`; attempt timeout `14400s`; progress timeout `600s`; no action limit
- Relay mode: ordinary public relays only; no controlled transport

## Retained coverage

- 51 ordinary UI actions
- 4 correlated frame records and 8 retained gameplay frames
- 79 visual-oracle records
- 8 sprite-provenance records
- 8 screenshots and 24 overlap cases audited
- Stable gameplay began on both clients, but required full-match, motion, sprite, relay-chaos, and terminal matrices did not complete.
- Natural victory, equal terminal outcome/hash, and frozen post-terminal tick remain untested.

## Blocker

At turns 473-476, both peers lost required two-of-three publication quorum:

- `relay.nostr.net` rejected turn publications with `rate-limited: too many events from this key (60/60s)` and later timed out.
- `relay.nostr.wirednet.jp` timed out turn, receipt, and republish operations.
- `nostr.sathoarder.com` accepted one turn from each peer, then timed out receipt and republish operations.

Production reliability diagnostics recorded both peers at status `1`, reason `7`. Harness then timed out waiting for entity 9 drawable direction 3. This is retained as public-relay infrastructure failure, not a product bug.

Wrapper marked all three relays incompatible from production acknowledgements. Every remaining rotating quorum contained at least one proved-incompatible relay, so no second attempt was eligible within configured relay pool.

## Findings

- Confirmed product bugs: 0
- Product passes: none claimed beyond retained partial observations
- Blocked cases: all required completion matrices and terminal assertions
- Screenshot overlap candidates: 5 initial candidates across 24 cases. They were not independently reproduced with exact root-cause proof, so none is logged or counted as a product bug.
- Procedural/fallback totals: blocked; run ended before complete aggregate provenance verdict.
- Motion thresholds/totals: blocked; only partial frame evidence retained.

## Evidence and cleanup

- Aggregate artifacts: `artifacts/browser-multiplayer-audits/20260817T153312Z-188c0de-full-restart/`
- Attempt evidence: `artifacts/browser-multiplayer-audits/20260817T153312Z-188c0de-full-restart/attempts/20260817T171751Z-e7e3daaa9e57/`
- Relay acknowledgements and reliability state: attempt `first-failure.json`
- Attempt selection and skipped quorums: aggregate `attempts.json`
- Actions, frames, visual oracles, provenance, screenshots, and overlap reports remain under attempt directory.
- Cleanup confirmed: wrapper, child harness, both ChromeDrivers, both temporary Chrome profiles, server, and port `8926` released.

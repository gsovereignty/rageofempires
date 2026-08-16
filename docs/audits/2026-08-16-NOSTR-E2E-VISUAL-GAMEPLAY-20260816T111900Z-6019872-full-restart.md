# Nostr multiplayer gameplay audit

- Run: `20260816T111900Z-6019872-full-restart`
- Started UTC: `2026-08-16T10:20:33.245457+00:00`
- Verdict: **BLOCKED**

## Result

Production-path verification crossed the earlier empty host exact-capture
boundary. A later exact capture reproduced one empty host manifest and then
recovered on attempt 2 with fresh position, camera, frame, direction, visible
layers, and a unique request ID. The run then exposed a separate stale
direction/readback harness race. Evidence and cleanup records remain under the
declared artifact root. No product bug was confirmed.

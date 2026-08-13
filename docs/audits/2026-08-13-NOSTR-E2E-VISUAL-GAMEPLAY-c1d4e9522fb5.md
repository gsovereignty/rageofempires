# Nostr multiplayer gameplay audit

- Run: `20260813T122251Z-c1d4e9522fb5`
- Started UTC: `2026-08-13T12:24:16.366464+00:00`
- Verdict: **BLOCKED**

## Result

Run stopped before acceptance completed. Infrastructure versus product classification remains unproved.

Primary failure: `InfrastructureBlocked: BLOCKED_COMMAND_ABSENT: unit 10 command to (28, 12) was not accepted by both peers`

## Crush-bugs closure ledger

| Finding | Category | Confirmation | Status | Fix | Regression | Prevention | Result |
|---:|---|---|---|---|---|---|---|
| Relay probe selected relays unable to carry gameplay kind 78 | tooling | Prior run selected `nostr.bond`; it rejected every kind-78 event, then one rate limit left publication below quorum and suspended lockstep at tick 19 | PASS | Preflight now requires positive publish acknowledgement and exact-ID replay for a signed ephemeral kind-78 event | Rejecting-relay test proves exclusion and socket cleanup | Live probe excluded `nostr.bond` and selected three kind-78-capable relays | Original host, join, simultaneous, and all-direction movement phases passed; both peers reached tick 44 with active reliability |

No security boundary or security claim was in scope.

## Later independent blocker

After original movement milestone passed, a later command for unit `10` to
`(28, 12)` was absent on both peers. Available evidence does not yet prove
whether this was automation input/selection failure or product command loss;
classification and root cause remain `undetermined`. This later blocker does
not contradict closure of the relay-selection finding, but prevents a full
gameplay-audit PASS.

Evidence directory:
`artifacts/browser-multiplayer-audits/20260813T122251Z-c1d4e9522fb5/`.

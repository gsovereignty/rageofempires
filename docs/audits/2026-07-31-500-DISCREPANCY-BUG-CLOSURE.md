# 500-discrepancy bug closure

Date: 2026-07-31

## Scope

This closure audit reviews
`2026-07-30-500-DISCREPANCY-AUDIT.md`, the current production paths behind
its movement and DAT rows, and adjacent build/runtime wiring.

A discrepancy was treated as a bug only when current code proved a reachable
contract violation with concrete product impact. No trust or privilege
boundary was established in this scope, so no finding is classified as a
security issue.

## Disposition

- D001 and D002 remain confirmed historical correctness bugs. Their existing
  fixes and direct regressions remain wired.
- D003 remains a documented integer-authority/fixed-point-presentation
  transform. Replacing the authoritative simulation with floating point would
  be an architecture change, not a proved bug fix.
- D004 remains an open fidelity question for exact per-graphic scheduling.
  DAT `seconds_per_frame` values do not prove the original runtime scheduler.
  The adjacent bounded-cadence implementation bug C001 was fixed without
  claiming exact original cadence.
- D005-D500 remain 312 declared representation transforms and 185 declared
  reconstruction policies. Runtime review found no direct contract violation
  in these rows.
- D045, D487, and D493 expose raw ram capacity values, but accepted occupant
  classes and occupancy semantics remain unproved. They are fidelity
  `NEEDS_PROOF`, not implementation bugs.

## Evidence ledger

| Finding | Category | Confirmation | Status | Fix | Regression | Prevention | Result |
|---:|---|---|---|---|---|---|---|
| D001 | correctness | Pending orders selected moving art without physical presentation displacement. | PASS | `99d7782` gates movement art on render-subtile displacement. | `render_asset_coverage_tests` stationary/moving action coverage. | Renderer action derives from presentation displacement. | PASS |
| D002 | correctness | Blocked fixed-point movers retained newly accumulated credit and could burst later. | PASS | `89e9f86` restores pre-tick remainder after blocked movement. | `blocked_cavalry_does_not_bank_movement_credit`. | Failed primary movement cannot bank that tick's credit. | PASS |
| C001 | correctness | Direct legacy unit animations consumed a 50 ms source tick directly while bounded walking cadence requires 100 ms; composite paths divided by two. | PASS | `7c4ca80` adds a direct 100 ms unit-frame clock while preserving composite and non-unit clocks. | `frame_timing_tests` checks 50/99/100 ms boundaries and stable unit phase. | Direct and composite unit clocks now expose separate typed helpers. | PASS |
| C002 | correctness | Formation remainder used denominator 32000, then survived formation exit and entered individual 320/100 arithmetic. | PASS | `543bd37` clears formation-domain credit at natural completion, redirect, and stop. | `formation_movement_credit_stays_in_its_denominator_domain`. | Every formation flag-clear site resets only formation-domain credit; ordinary solo retasks preserve solo credit. | PASS |
| C003 | correctness | Fractional render endpoint used stale authoritative `previous_position` elevation after a tile crossing. | PASS | `cc269f6` stores presentation-only previous/current elevation endpoints and wires renderer/lifecycle cleanup. | Hill-boundary endpoint projection plus add/replace/delete lifecycle coverage. | Presentation elevation state snapshots with subtile state; authoritative position semantics remain unchanged. | PASS |
| C004 | tooling | Documented complete CTest gate registered 4 of 30 Python test files, omitting 112 test cases. | PASS | `beea12a` registers every `tools/**/test_*.py` file with stable unique names. | CTest inventory proves 30/30 files; Python label suite passes 30/30. | Configure-dependent recursive inventory registers new Python tests after reconfigure. | PASS |

## Wiring audit

- All 49 production `src/*.cpp` files belong to a CMake target.
- All 47 C++ test sources and all 18 shell smoke tests are registered.
- All 30 Python `test_*.py` files are now registered individually in CTest.
- Movement presentation changes reach the SDL executable, focused render
  coverage, simulation tests, and frame-timing tests.
- Unit presentation elevation state initializes on add and state replacement,
  snapshots during update, and is pruned after relic collection, direct or
  transport deletion, and death cleanup.
- Transport capacity is already enforced through its dedicated runtime path.
  Population, researched vision, integer contact range, tick timing, relative
  speed, and packed-trebuchet policy rows are consumed according to their
  declared reconstruction contracts.

## Gate record

- Every fix lane received an independent review. Final verdicts: PASS.
- Fresh Release configure and full build: PASS.
- Full CTest inventory: 102/102 PASS, including 30/30 Python files and all SDL,
  self-containment, resource-manifest, performance, and visual smoke gates.
- `git diff --check`: PASS.
- Final tracked worktree check occurs after this ledger is committed.

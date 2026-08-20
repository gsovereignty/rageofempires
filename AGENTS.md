# Agent instructions

## Parent instructions

Read and follow `../AGENTS.md` before working in this repository. It defines
workspace modification boundaries, Git scope, external-input handling, and
mandatory build/runtime isolation. Rules here supplement those parent rules.

Preserve unrelated working-tree changes. Do not edit generated or build output
unless the task specifically requires regenerating it.

## Subagent lifecycle and concurrency

Use at most one live subagent at any time. Before every spawn, query agent
status and prove that no child agent is `running`, `waiting`, or otherwise
live. Completed historical entries do not authorize another spawn until the
current child has returned a final result and its status is confirmed closed.

For sequential per-problem work:

1. Spawn one child for exactly one problem.
2. Do not spawn reviewers, helpers, retries, or replacement children while it
   is live.
3. If replacing or retrying that child, interrupt it first and confirm it is
   no longer live before spawning the replacement.
4. Verify the result and repository state, then confirm the child is closed.
5. Only then spawn a fresh child for the next problem.

Never accumulate multiple live children, even when nominal concurrency slots
are available. If agent status is ambiguous or cannot be queried, do not spawn.

After solving any user-requested problem that changes tracked files, create a
focused Git commit before reporting completion. Stage only current-task files;
preserve and exclude unrelated user or agent changes. If no tracked files
changed, or a commit is genuinely impossible, report that explicitly instead
of creating an empty or mixed commit.

Before committing, inspect the actual changes and write one focused message as
a problem statement: `problem: <affected subject> <undesirable condition or
missing capability>.` Describe the actual problem solved, not the solution,
implementation, command, or files changed. Keep the subject under 50 characters
when possible and never over 70. Add a wrapped body only when necessary to
explain solution rationale, breaking changes, migrations, reverts, or issue
references.

Before every completion commit containing game-code changes, run `make` from
this repository root and require it to finish without build errors. Fix
in-scope failures before committing. If `make` fails because of unrelated
existing work that must not be changed, do not commit; report the exact blocker
and preserve that work.

Never compile the game or run tests unless game code changed. If only
documentation or other non-code files changed, do not run `make`, invoke a
compiler, or run any test suite or test command.

## Bug root-cause evidence

Do not speculate about bug causes. Examine only direct evidence from the
codebase and data, and always follow the evidence through until the precise
root cause of every reported bug is determined. If available evidence does not
yet prove a precise root cause, continue investigating and report the cause as
undetermined rather than offering a hypothesis.

When logging bugs in `../todo/`, record only known facts and never speculation.
Include relevant function names, file names, and line numbers. Do not log a bug
in `../todo/` until direct evidence establishes its precise root cause.

## Audit findings destination gate

Never start or run an audit until its findings destination is declared and
known to persist after the audit process exits. Before launch, record the exact
repository report path and durable artifact directory where findings, inputs,
per-case evidence, and aggregate verdicts will be written. Temporary
directories, process-local filesystems, browser virtual filesystems, and
unreported console output are not valid final destinations.

Do not report case counts, findings, passes, failures, or blocked results until
the corresponding files exist at those declared paths and can be named in the
handoff. If an audit discovers evidence outside the declared destination,
preserve or export it there before continuing or making any completion claim.
If no suitable destination is known, do not run the audit; establish the
destination first.

## Browser multiplayer test harness

The complete reproducible browser multiplayer test-harness source lives in
this repository. Treat these tracked files as the authoritative harness:

- `tests/web/nostr_multiplayer_smoke_test.py` is the main two-browser runner.
- `tests/web/test_nostr_multiplayer_audit_tools.py` contains harness regression
  tests.
- `tools/run_nostr_visual_audit.py` and
  `tools/run_nostr_visual_display_matrix.py` orchestrate visual audit runs.
- `tools/nostr_*` contains the seeded-action, coverage, frame, pixel, route,
  transition, sprite, and packaged-oracle helpers.
- `Makefile` owns supported build, test, and audit entry points.
- `.agents/skills/audit-browser-multiplayer-gameplay/SKILL.md` defines the
  production audit contract.

Generated or externally supplied runtime pieces are intentionally not tracked
as harness source: `build-web/dist/`, `build-web/selenium-venv/`, browser and
WebDriver installations, public Nostr relays, and run evidence under
`artifacts/`. Do not treat their absence from Git as missing harness source.
Do not introduce a parent-directory runtime dependency to provide any of these
pieces.

## Archived decompiled-source evidence

The repository tracks a deterministic archive of the decompiled reference
corpus at `docs/evidence/decompiled-source/decompiled.tar.xz`. Its checksum lives
beside it as `decompiled.tar.xz.sha256`.

When decompiled evidence is required, run
`scripts/extract_decompiled_source.sh`. The script verifies the archive and
always extracts it to the fixed repository-local `decompiled/` directory.
That directory is ignored by Git. Treat extracted contents as read-only
research evidence: never edit them, stage them, compile them, package them, or
make production/build/runtime code depend on them.

Use `tools/create_decompiled_source_archive.py <source-directory>` only when
the archive must be deliberately regenerated. It sorts entries, normalizes
archive ownership and timestamps, and excludes `.DS_Store` metadata so equal
source trees produce equal archive bytes.

## Production-fix completion gate

Never classify, describe, count, or report a bug as fixed until all of these
conditions are proved:

1. The defect is corrected in implementation.
2. The correction is wired into the real production execution path used by
   the affected user workflow.
3. The correction is included in the production build, package, or deployment
   artifact that users run.
4. The original failure is reproduced through that production path before the
   correction and is no longer reproducible through the same path afterward.
5. Relevant regression checks and the repository's required `make` gate pass.

Unit tests, mocks, debug paths, audit switches, environment-variable overrides,
fallbacks, feature flags, isolated harnesses, source-only changes, successful
compilation, and unshipped artifacts do not independently prove a production
fix. If any condition above is missing, report the work as partial or unverified
and keep the bug open. Never substitute "implemented," "ready to deploy," or
"works in tests" for "fixed."

## Reconstruction repository structure

```text
reconstruction/
├── include/aoe/       Public C++ interfaces and shared domain types
├── src/               C++ implementations and executable entry points
├── tests/             C++ unit/integration tests and SDL smoke-test scripts
├── resources/         Tracked native scenarios and campaign fixtures
├── docs/              Indexed guides, contracts, fidelity, evidence, and audits
├── tools/             Audit, report, fixture, and evidence-generation tools
│   └── dat_metadata/  DAT metadata extractor plus generator tests
├── scripts/           Build/bundle verification and visual smoke automation
├── generated/         Tool-generated JSON evidence and coverage reports
├── cmake/             CMake templates, including macOS bundle metadata
├── artifacts/         Local visual-test captures and logs; do not track
├── build*/            Local CMake build trees and app bundles; do not track
├── .codebase-memory/  Tracked codebase knowledge-graph artifact
├── CMakeLists.txt     Primary CMake build and test definition
├── Makefile           Convenience build/test targets
├── README.md          Project scope, quickstart, and documentation entry point
└── AGENTS.md          Repository-specific instructions and structure map
```

### Source layout

- `include/aoe/` and `src/` normally mirror each subsystem by basename.
  Put public declarations in headers and implementation in matching `.cpp`
  files.
- `src/main.cpp` starts the SDL application.
- `src/multiplayer_roster_headless.cpp` provides the headless multiplayer
  roster executable used by tests.
- `tests/*_tests.cpp` contains deterministic subsystem coverage.
- `tests/*_sdl_smoke.sh` contains end-to-end UI, rendering, audio, and
  multiplayer smoke checks.

### Data and evidence layout

- `resources/` contains human-editable runtime and audit fixtures that ship
  with or exercise the reconstruction.
- `generated/` contains reproducible outputs from `tools/`; update generator
  and generated output together when contracts change.
- `docs/README.md` indexes all prose by genre and lifetime.
- `docs/guide/` contains reader-facing build, controls, architecture, content,
  and status material.
- `docs/contracts/` contains durable reconstruction-native behavior.
- `docs/fidelity/`, `docs/assets/`, and `docs/evidence/` contain bounded
  original-behavior findings, mappings, and pinned evidence.
- `docs/ui/` contains focused interaction and presentation contracts.
- `docs/audits/` contains dated snapshots, never durable contracts.
- `artifacts/` contains disposable screenshots, bitmap captures, and logs from
  local audit runs. Keep these out of commits unless explicitly requested.

### Build layout

- Use an out-of-source `build*` directory. Never place compiler output under
  `src/` or `include/`.
- `cmake/Info.plist.in` defines macOS application-bundle metadata.
- `scripts/verify_macos_bundle.sh` validates produced app bundles.
- `scripts/visual_smoke_test.py` drives screenshot-based runtime checks.

## Code discovery

Prefer codebase-memory graph tools for code discovery:

1. `search_graph`
2. `trace_path`
3. `get_code_snippet`
4. `query_graph`
5. `get_architecture`

Run `index_repository` first when no fresh index exists. Fall back to text or
file search for literals, errors, configuration, scripts, documentation, or
when graph results are insufficient.

# Agent instructions

## Parent instructions

Read and follow `../AGENTS.md` before working in this repository. It defines
workspace modification boundaries, Git scope, external-input handling, and
mandatory build/runtime isolation. Rules here supplement those parent rules.

Preserve unrelated working-tree changes. Do not edit generated or build output
unless the task specifically requires regenerating it.

## Reconstruction repository structure

```text
reconstruction/
├── include/aoe/       Public C++ interfaces and shared domain types
├── src/               C++ implementations and executable entry points
├── tests/             C++ unit/integration tests and SDL smoke-test scripts
├── resources/         Tracked native scenarios and campaign fixtures
├── docs/              Focused UI and behavior design notes
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
├── README.md          Project scope, setup, controls, and implementation status
├── ARCHAEOLOGY.md     Evidence log linking reconstruction choices to research
└── *.md               Fidelity contracts, evidence reports, maps, and audits
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
- `docs/` contains focused product/UI contracts. Root Markdown files contain
  broader fidelity findings, runtime evidence, asset maps, and project status.
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

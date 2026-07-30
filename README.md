# Native macOS reconstruction

Self-contained, clean-room reconstruction of a deterministic Age of
Empires-style RTS for macOS. Product source, tests, native fixtures, generated
evidence, and packaged resources live inside this repository. Supplied
commercial binaries and assets are research evidence only.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Or use:

```sh
make
```

Detailed build, packaging, and compatibility notes:

- [Build guide](docs/guide/BUILD.md)
- [macOS compatibility](docs/guide/MACOS_COMPATIBILITY.md)
- [Self-containment contract](docs/SELF_CONTAINMENT.md)

## Run

Launch executable or generated application bundle from build directory. Game
does not require, probe, or load commercial asset directories.

Controls and content:

- [Controls and scenario editing](docs/guide/CONTROLS.md)
- [Random maps](docs/guide/RANDOM_MAPS.md)
- [Civilization content](docs/guide/CIVILIZATION_CONTENT.md)
- [Current implementation status](docs/guide/IMPLEMENTATION_STATUS.md)

## Documentation

[Documentation index](docs/README.md) separates reader guides, native
contracts, fidelity findings, asset maps, runtime evidence, UI notes, and
point-in-time audits.

## Scope

Project reconstructs behavior from bounded evidence while keeping native
formats and implementation independent. Exact-original claims require pinned
data, executable control flow, or reproducible runtime evidence. Unknown
commercial behavior remains explicit instead of inferred.

Full scope and implemented-feature catalog:
[project status](docs/guide/STATUS.md).

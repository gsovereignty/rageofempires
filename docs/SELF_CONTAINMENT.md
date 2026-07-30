# Self-containment inventory

Product builds and runtime use only files inside this repository. Sprite
archives selected for runtime use are materialized under `game_data/` and
packaged with the executable. Runtime never reads their parent research copies.

| Family | Product consumer | Phase | Required | Reconstruction behavior |
|---|---|---|---|---|
| Native scenarios and campaigns | scenario/campaign loaders | runtime | yes | tracked text fixtures in `resources/` |
| Rules, units, buildings, technologies | simulation | compile/runtime | yes | code-native canonical tables |
| Terrain | SDL renderer | runtime | yes | packaged HD terrain textures and archive-backed/procedural fallback |
| Units, buildings, resources, projectiles | SDL renderer | runtime | yes | packaged `game_data` sprites with procedural fallback |
| UI, command icons, cursors, fonts | SDL renderer | runtime | yes | procedural/text fallback and system font rendering |
| Music, effects, ambience | audio system | runtime | yes | packaged MP3 music, WAV ambience, and DAT-linked DRS effects |
| Legacy DAT/DRS/SLP | SDL renderer and bounded parser tests | runtime/test | yes | packaged under `game_data/Data` |
| Save games and recordings | persistence | runtime | yes | user-data paths selected by application/platform |

## Resolution contract

Runtime has no parent-workspace search, current-working-directory asset
fallback, absolute workspace path, environment override, symlink escape, or
download. `configured_asset_root()` resolves only packaged `game_data` beside
the executable or inside its macOS bundle.

Tests requiring legacy formats construct bounded data in their build tree or
receive an explicit path as an opt-in research invocation. Such input is not
part of normal CMake configuration, build, CTest, packaging, or runtime.

## Provenance

Everything under `resources/` is reconstruction-authored text data.
`game_data/` contains user-supplied sprite, texture, music, ambience, and sound
archive runtime inputs copied into the repository so builds and installed
games remain hermetic. Generated evidence under `generated/` comes from
tracked tools.

## Enforcement

`scripts/check_self_containment.py` scans build configuration, scripts, tests,
headers, and runtime source. CTest runs it as `aoe_self_containment_guard` and
rejects known parent research paths plus runtime asset-root escape hatches.

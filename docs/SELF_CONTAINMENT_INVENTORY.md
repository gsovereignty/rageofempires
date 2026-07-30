# Self-containment inventory

Product builds and runtime use only tracked files inside this repository.
Commercial installation files remain optional research evidence and are never
read by the game, normal tests, packaging, or installed bundle.

| Family | Product consumer | Phase | Required | Reconstruction behavior |
|---|---|---|---|---|
| Native scenarios and campaigns | scenario/campaign loaders | runtime | yes | tracked text fixtures in `resources/` |
| Rules, units, buildings, technologies | simulation | compile/runtime | yes | code-native canonical tables |
| Terrain | SDL renderer | runtime | yes | procedural reconstruction renderer |
| Units, buildings, resources, projectiles | SDL renderer | runtime | yes | procedural reconstruction renderer |
| UI, command icons, cursors, fonts | SDL renderer | runtime | yes | procedural/text fallback and system font rendering |
| Music, effects, ambience, narration | audio system | runtime | no | reconstruction-native synthesized cues; silence where no native cue exists |
| Legacy DAT/DRS/SLP/WAV | audit tools and bounded parser tests | research/test | no | tracked synthetic fixtures or explicit research-tool input |
| Save games and recordings | persistence | runtime | yes | user-data paths selected by application/platform |

## Resolution contract

Runtime has no parent-directory search, current-working-directory asset
fallback, absolute workspace path, environment override, symlink escape, or
download. `configured_asset_root()` deliberately returns no commercial archive
root. Callers therefore select reconstruction-owned procedural/native behavior.

Tests requiring legacy formats construct bounded data in their build tree or
receive an explicit path as an opt-in research invocation. Such input is not
part of normal CMake configuration, build, CTest, packaging, or runtime.

## Provenance

Everything under `resources/` is reconstruction-authored text data and tracked
by this repository. Generated evidence under `generated/` comes from tracked
tools; it is documentation/audit input, not a runtime dependency. No supplied
commercial binary, archive, media file, or decompiler output is redistributed.

## Enforcement

`scripts/check_self_containment.py` scans build configuration, scripts, tests,
headers, and runtime source. CTest runs it as `aoe_self_containment_guard` and
rejects known parent research paths plus runtime asset-root escape hatches.

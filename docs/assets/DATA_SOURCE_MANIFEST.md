# HD and 1999 data-source manifest

## Scope and result

This manifest separates data packaged or encoded in reconstruction from data
used only as research evidence. It compares tracked `game_data/` tree with
supplied HD Edition tree and supplied 1999 retail tree by file size and SHA-256,
not by filename alone.

All 1,438 packaged files match HD tree byte-for-byte. Of those, 530 also match
at least one 1999 file byte-for-byte; 908 differ from every supplied 1999 file.
No packaged file is reconstruction-authored, 1999-only, or loaded from either
external research directory at build or runtime.

Machine-readable, per-file results live in
[`generated/data_source_manifest.json`](../../generated/data_source_manifest.json).
Regenerate them with:

```sh
python3 tools/generate_data_source_manifest.py \
  --game-data game_data \
  --hd-root ../original-assets-hd/app \
  --classic-root ../original-assets-1999/Binary \
  --output generated/data_source_manifest.json
```

`hd_and_1999_identical` means provenance cannot be distinguished from bytes:
tracked payload is present in both editions. Repository history added whole
packaged tree in one HD-shaped import, so this report treats HD as immediate
packaged source while recording exact 1999 identity separately.

## Packaged runtime data

Build copies every tracked file under `game_data/` beside executable and into
macOS bundle. Product code resolves this local copy only.

| Data family | Runtime use | Edition result |
|---|---|---|
| `Data/empires2_x1_p1.dat` | Unit, graphic, sound, terrain, civilization, technology, and effect metadata | HD; not byte-identical to supplied 1999 data |
| `Data/graphics.drs` | Unit, building, resource, projectile, shadow, and effect SLPs | HD; not byte-identical to supplied 1999 archive |
| `Data/interfac.drs` | Palette 50500, cursors, command/HUD/interface SLPs | HD; not byte-identical to supplied 1999 archive |
| `Data/terrain.drs` | Grass, water, beach, shallows, and other terrain SLPs | HD; not byte-identical to supplied 1999 archive |
| `Data/sounds.drs`, `sounds_x1.drs` | DAT-selected WAV resources; later archive wins duplicate IDs | HD set; base `sounds.drs` has byte-identical 1999 copy, expansion archive does not |
| `Sound/music`, `Sound/stream`, `Sound/terrain` | Music, frontend streams, and terrain ambience | Mixed by file: exact classification recorded per file; many base tracks are byte-identical to 1999, expansion tracks are HD-only |
| `Campaign`, `Scenario`, `Random`, `AI` | Legacy campaign/media loading and import-compatible content | HD tree; shared base files recorded as byte-identical to 1999 |
| `Data/Slp`, `Data/*.Dat`, palettes, blend/filter/mask data | Loose UI/media and renderer support data | HD tree; shared files recorded individually |
| `Bin` language files | Legacy localized string lookup | HD tree; `language.dll` payload also exists in 1999 |

Not every packaged file is currently opened during an ordinary match. It is
still build/runtime input because CMake deploys complete tracked tree.
`generated/live_content_assets_inventory.json` gives narrower static/live
consumer coverage; this provenance manifest gives complete packaged coverage.

## Data encoded into source and generated evidence

Reconstruction also consumes HD data during explicit research/generation and
commits derived, non-binary results:

- `empires2_x1_p1.dat` supplies bounded rule tables and generated metadata for
  units, buildings, technologies, civilizations, graphics, animation, sounds,
  projectiles, economy, trade, religion, defense, garrison, and victory.
- `graphics.drs`, `interfac.drs`, `terrain.drs`, and sound DRS archives prove
  referenced resource existence, frame/palette data, UI mappings, shadows,
  projectile directions, audio joins, and renderer coverage.
- HD `AoK HD.exe` plus its decompiled read-only corpus supplies executable
  control-flow, filename, UI-layout, fog, cursor, audio, and behavior evidence.
- Generated JSON may retain temporary extraction path in its `source` field;
  this records generation provenance. Product build/runtime does not read it.

These derivatives are implementation evidence, not runtime links to
`original-assets-hd/`.

Loader also accepts optional `Data/sounds_x2.drs`, but no such file is tracked
or packaged. It therefore contributes no reconstruction data in this manifest.

## 1999-specific use

1999 tree serves comparison and classic/base-game provenance:

- 530 packaged payloads have independently proved byte-identical 1999 copies.
  This includes base campaign/media, base audio, language data, and assorted
  renderer/support files listed exactly in machine manifest.
- Classic frontend policy intentionally selects `Sound/stream/open.mp3` and
  `town.mp3`; their tracked payloads match supplied 1999 files. Expansion
  executable evidence instead names `xopen.mp3` and `xtown.mp3`.
- 1999 `manifest.json` describes supplied retail tree and supports edition
  inventory comparisons. It is research input only and neither copied nor
  loaded by product.

No reconstruction rule table, generated metadata catalog, executable behavior
claim, or packaged file is presently proved to come exclusively from supplied
1999 data. Shared bytes support classic provenance claims, but cannot reveal
which edition first authored them.

## Source-loading boundary

Normal build and runtime use only tracked `game_data/`, `resources/`, source,
and generated inputs inside repository. External HD and 1999 paths appear only
in this documentation, self-containment checks, and explicit research commands.
Research tools accept caller-supplied paths; required outputs must be
materialized under `generated/` before product use.

Relevant original loading behavior was checked against read-only decompiled HD
source: it names `data\\empires2_x1_p1.dat`, `graphics.drs`, and
`interfac.drs`. Reconstruction preserves data roles while replacing external
installation discovery with packaged-root resolution.

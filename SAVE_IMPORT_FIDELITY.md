# Classic Genie save inspection

## Non-requirement

Commercial saved-game compatibility is not required for this project. This
read-only inspector is optional archaeology tooling; inability to load or
write commercial saves does not block releases or project completion. Native
Save110 is the supported runtime contract.

This module performs bounded, read-only inspection of the classic Genie
save/record envelope associated with Age of Kings and *The Conquerors*. It
does not load a commercial saved game into the reconstruction and does not
claim compatibility with project Save110.

## Pinned evidence

Primary implementation evidence is the open `aoc-mgz` parser at commit
[`9c1e9cc93998887a56f336dc4489555f4ad5577a`](https://github.com/happyleavesaoc/aoc-mgz/tree/9c1e9cc93998887a56f336dc4489555f4ad5577a).
Its pinned
[`mgz/fast/header.py`](https://github.com/happyleavesaoc/aoc-mgz/blob/9c1e9cc93998887a56f336dc4489555f4ad5577a/mgz/fast/header.py)
reads two little-endian 32-bit envelope fields, inflates `header_len - 8`
bytes as raw DEFLATE (`wbits=-15`), reads the seven-byte game signature plus
padding and a 32-bit float save version, then reads log version from the body.
Its pinned
[`mgz/util.py`](https://github.com/happyleavesaoc/aoc-mgz/blob/9c1e9cc93998887a56f336dc4489555f4ad5577a/mgz/util.py)
lists recognized signatures including `VER 9.3`, `VER 9.4`, UserPatch
variants, and `MCP 9.F`.

Format structure was cross-checked against `aoc-mgx-format`, commit
[`c08d672006c225c4941172ddb98f657e2fda4af5`](https://github.com/stefan-kolb/aoc-mgx-format/tree/c08d672006c225c4941172ddb98f657e2fda4af5),
whose project describes itself as an AoC savegame format specification and
ships a Ruby parser plus fixture corpus.

`.gax` is commonly identified as the *The Conquerors* saved-game extension;
`.gam`, `.ga1`, and `.mgx` are accepted for cautious Genie-family inspection.
Extension alone never proves file identity.

## Inspection contract

- Preserve the complete input bytes exactly.
- Preserve compressed and inflated header bytes separately.
- Validate the eight-byte envelope and declared header extent.
- Inflate exactly one complete raw-DEFLATE stream; trailing compressed bytes
  or truncation are malformed.
- Default bounds: 64 MiB file, 16 MiB compressed header, 64 MiB inflated
  header.
- Expose game signature, save-version float, and log version only when their
  pinned offsets and signature are proved.
- Player count, map dimensions, tick, and object state remain unset. Their
  layouts vary by engine/save version and are not proved by this bounded
  inspector.
- Emit structured `unsupported`, `malformed`, and `limit_exceeded`
  diagnostics with byte offsets.

No lossless conversion is currently offered. Existing legacy scenario/object
mappings do not cover a complete in-progress classic simulation, so conversion
would discard state. `convert_classic_save_losslessly` therefore always
returns no scenario.

Hermetic fixtures generate raw-DEFLATE envelopes in memory and verify exact
raw preservation, proved metadata, unknown signatures, truncation, invalid
lengths, decompression bounds, structured diagnostics, and conversion refusal.

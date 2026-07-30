# UI font and text-rendering evidence

## Verdict

No exact classic Age of Conquerors font contract is proved by supplied assets.
Do not bind a guessed font family, size, weight, line height, or color.

The supplied `Data/interfac.drs` contains 221 `slp`, 126 `bina`, and 50 `wav`
entries. It contains no bitmap-font or outline-font entry. Pinned openage commit
`9a5a7ccbfc20c2de658fc746462cd4a69aa758ef` recognizes PE resource types
`fontdir` (7) and `font` (8), but its AoC conversion path supplies no original
font catalog, family/size mapping, or UI text-color contract. Openage's DejaVu
dependency is an openage engine choice, not evidence for original AoC.

## Supplied HD executable evidence

`decompiled/pe-metadata-and-imports.txt` proves that the separately supplied
AoK HD executable imports:

- `AddFontResourceA` and `CreateFontIndirectA`
- `GetTextMetricsA` and `GetTextExtentPoint32A`
- `TextOutA`, `DrawTextA`, `SetTextColor`, and `SetBkColor`

`decompiled/AoK-HD-patched.c`, function at address `0x004f3b30`, makes 19
`AddFontResourceA` calls for 18 unique external files under `data/fonts/`
(`georgia.ttf` is registered twice):

```text
centbd_.ttf  centit_.ttf  centrg_.ttf
comic.ttf    comicbd.ttf
georgia.ttf  georgiab.ttf georgiai.ttf georgiaz.ttf
lblack.ttf   lbrite.ttf   lbrited.ttf  lbritedi.ttf lbritei.ttf
lcallig.ttf  papyrus.ttf  poorich.ttf  vineritc.ttf
```

The same function allocates 37 font slots. Thirty-five slots are loaded through
three adjacent localized strings per slot: family, numeric height, and style.
The exact base IDs are 110, 113, 134, 137, then 116, with subsequent groups
through 209; slot 5 is left empty. Slot names include
`RGE_FONT_BUTTON1`, `RGE_FONT_GAME`, `RGE_FONT_TEXT`,
`RGE_FONT_TECH_TREE_NODE`, and history/config variants. Exact family, height,
weight, and italic values for these 35 slots live in resource strings not
present in current extracted evidence. Slot 36 alone is directly constructed as
Georgia, height 14, weight 700.

The parser in `FUN_004f3010` proves the string grammar: the family string is
passed to the font constructor; the second string is parsed by `atol`; the
third maps `B`/`b` to weight 700 instead of 400 and `I`/`i` to the italic
flag. Strikeout is supplied separately by the slot table. Because the
localized string provider is absent, those values remain unresolved rather
than inferred from slot labels.

After creating each font, code selects it into an HDC and records
`tmAveCharWidth` plus `tmHeight + max(1, tmExternalLeading)`. This proves HD
uses runtime GDI metrics rather than a fixed bitmap-glyph table.

`generated/ui_executable_evidence.json` records all 37 slot outcomes, every
resource-string triplet, the 18 unique font paths, exact direct Georgia slot,
GDI metrics, and imported drawing APIs. Regenerate it from user-owned inputs:

```sh
python3 tools/dat_metadata/generate_ui_executable_evidence.py \
  /path/to/AoK-HD-patched.c /path/to/AoK-HD.exe \
  /path/to/Data/interfac.drs
```

These findings are exact for supplied HD decompilation, not automatically
portable to classic AoC. Referenced TTF files are absent from supplied asset
tree, so no file hash, OpenType family metadata, glyph coverage, or measured
metrics can be validated.

## Colors

Executable calls contain direct black (`0x000000`), white (`0xffffff`), and red
(`0x0000ff` in Windows `COLORREF` byte order) examples plus many colors read
from object fields. This proves colors are contextual. It does not prove one
global text palette or associate exact colors with all UI roles.

## Bounded fallback

Until user-owned font files and matching executable resource strings are
available:

1. Keep font selection configurable and classify it `fallback`, never `exact`.
2. Use platform text shaping and measured glyph advances; do not invent fixed
   character widths from the HD average-width cache.
3. Preserve explicit caller-provided foreground/background colors. Do not
   infer role colors from isolated `SetTextColor` call sites.
4. Re-audit from original classic `age2_x1.exe`/language resources for classic
   fidelity, or from the 19 listed TTF files plus resource strings for HD
   fidelity.

No exact-font renderer patch was added: current evidence supplies neither font bytes nor
the 35 localized slot-value triplets. A safe future renderer API should accept
an explicit font file plus measured point/pixel size and role colors, and keep
its evidence classification visible; binding Georgia 14 bold globally would
misapply the one proved special-purpose slot.

## Implemented bounded text fallback

Runtime language files accept strictly validated UTF-8 and reject overlong
forms, surrogate code points, malformed continuations, and control characters.
Strict known-key validation and English fallback remain. Count text uses
explicit singular/other keys with bounded `{count}` interpolation; this is a
declared grammar boundary, not complete locale-specific plural equivalence.

No matching supplied font bytes exist, so current SDL debug font remains
ASCII-only. Localized browser chrome uses explicit fallback behavior: ASCII
remains unchanged, common Latin diacritics fold to base glyphs, and unsupported
code points render `?` rather than disappearing or producing malformed text.
This remains classified `fallback`, not exact font selection or shaping.
Focused tests cover UTF-8, invalid forms, singular/plural paths, and missing
glyphs. SDL browser smoke coverage exercises English and long localized text.

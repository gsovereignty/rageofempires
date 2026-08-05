# Legacy language resource fidelity

Localization reads user-supplied Windows `language.dll`, `language_x1.dll`,
and `language_x1_p1.dll` without bundling or executing them. Runtime never
probes a parent installation. Optional DLLs and fonts must already be inside
configured, packaged game data root.

## Locale selection

Eleven shipped-language profiles map normalized locale, exact Win32 LANGID,
archive directory aliases, and audio aliases: English, German, French, Spanish,
Italian, Brazilian Portuguese, Dutch, Russian, Japanese, Korean, and Simplified
Chinese. Regional tags select by primary language.

Options key `L` cycles profiles. Apply changes text and audio immediately;
Save+Apply persists locale and explicit language-file selection atomically.
`AOE_LOCALE` and `AOE_LANGUAGE_FILE` are explicit per-launch overrides.

Packaged lookup checks only `Bin/<selected-language>/language.dll`,
`language_x1.dll`, and `language_x1_p1.dll`, in base-to-patch precedence.
Missing packs log a precise warning and use validated built-in English.
Unsupported locale identifiers are rejected rather than guessed.

## Resource boundary

Parser validates DOS/PE signatures, PE32/PE32+ headers, section RVA mapping,
resource/data bounds, numeric type 6 (`RT_STRING`), exact 16-string blocks,
and UTF-16 surrogate pairs. Input is capped at 64 MiB. IDs use Windows formula
`(block_id - 1) * 16 + slot`.

Only selected LANGID is read. Later DLLs replace earlier values with same ID.
Mapped strings override built-in English. Complete numeric extraction remains
in `LegacyLanguageReport::extracted`; unmapped IDs also remain in `unknown`.
`legacy_ui_string_catalog()` contains every proven semantic runtime binding:
seven main-menu commands. Reconstruction-native chrome IDs are never guessed.

Every reconstruction-owned C++ literal also has deterministic FNV-1a key in
`generated/localization_literal_catalog.tsv`. Language packs may use
`literal "English source" "Translation"`; loader computes same stable key.
Central UI renderer resolves that override before drawing. Generator scans all
production C++ sources, rejects hash collisions, and its regression test proves
checked-in catalog is current. Direct SDL debug text outside central renderer
is forbidden by test.

## Grammar, fonts, layout, and audio

Plural selection implements one/other plus Russian one/few/many,
French/Portuguese zero-or-one, and invariant Japanese/Korean/Chinese forms.
Named `{argument}` formatting rejects malformed names and unresolved markers.

All 35 recovered localized font triplets decode from RT_STRING IDs 110-211.
Height is bounded; `B` and `I` select bold and italic. Original font bytes stay
user-owned. On macOS, every non-ASCII production debug-text call passes through
UTF-8 CoreText when packaged `GEORGIAB.TTF` exists, preserving glyphs and
measured advances. Bounded ASCII folding is used when packaged font load fails.

Taunts, scenario narration, and campaign narration select active locale audio
alias. Narration then tries English inside same packaged root as deterministic
fallback. No external installation search occurs.

Original evidence: `AoK-HD-patched.c` lines 144954-145610 prove
`bin\\%s\\language.dll` selection and `LoadStringA`; lines 289499-299194 prove
measured GDI text extents. `generated/ui_executable_evidence.json` pins all 35
font slots and styles from user-owned supplied resources.

Tests use generated temporary PE fixtures. They cover exact IDs, precedence,
unknown preservation, UTF conversion, malformed input, every language profile,
directory aliases, plural families, named formatting, and font parsing. No
original DLL, font, audio, or extracted payload is committed.

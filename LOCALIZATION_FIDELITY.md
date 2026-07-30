# Legacy language resource fidelity

The localization subsystem can read user-supplied Windows
`language.dll`/`language_x1.dll` files without bundling or modifying them.
`extract_pe_string_resources` accepts only an explicit external path and a
numeric Windows language ID.

The parser is deliberately bounded:

- DOS and PE signatures, PE32/PE32+ optional headers, section RVA mapping,
  resource-directory bounds, and data bounds are validated.
- Only numeric resource type 6 (`RT_STRING`) is traversed.
- Each numeric block is decoded as exactly 16 length-prefixed UTF-16 strings.
- String IDs use the Windows formula `(block_id - 1) * 16 + slot`.
- Invalid or unpaired UTF-16 surrogates are rejected.
- Input is capped at 64 MiB; no DLL code is loaded or executed.

`load_legacy_language_sources` takes sources in precedence order. A later
source, normally `language_x1.dll`, replaces an earlier value with the same
numeric ID. Only the requested language ID is read. The caller supplies an
explicit numeric-ID-to-`StringTable`-key catalog; no default or heuristic
mapping exists. Extracted IDs absent from that catalog are preserved in
`LegacyLanguageReport::unknown`.

Mapped strings override the built-in English table. Missing mapped IDs retain
English text. A catalog entry naming an unknown runtime key is rejected.

Tests construct minimal PE fixtures in the temporary directory and cover exact
ID calculation, source precedence, unknown-ID reporting, UTF-16-to-UTF-8
conversion, malformed surrogate rejection, and invalid mapping rejection.
No original language DLL or extracted string payload is committed.

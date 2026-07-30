# Commercial Campaign Import Fidelity

## Non-requirement

Commercial campaign compatibility is not required for this project. This
module is optional, bounded archaeology tooling; incomplete containers,
branching, briefings, and cinematics do not block releases or project
completion. Native Campaign files are the supported runtime contract.

## Supported slice

`inspect_legacy_campaign` recognizes classic AoE/AoC campaign container version
`1.00`, used by `.cpn` and `.cpx`. It parses the fixed campaign header and
ordered scenario index, extracts every embedded payload byte-for-byte, and
passes each payload to the bounded commercial scenario inspector.

This is container inspection, not reconstruction campaign progression.
Branching, unlocks, briefings, cinematics, difficulty changes, completion
state, and original-runtime ordering beyond physical index order are not
inferred.

## Commit-pinned evidence

The layout follows Siege Engineers' MIT-licensed `genie-rs` commit
`a77200aef567b40b7db51cf47c1fda8db75e8e67`:

- [`CampaignHeader` and CPX versions](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-cpx/src/lib.rs)
  identify classic `1.00`, AoE1 DE `1.10`, and AoE2 DE `2.00`.
- [Classic header/index parsing](https://github.com/SiegeEngineers/genie-rs/blob/a77200aef567b40b7db51cf47c1fda8db75e8e67/crates/genie-cpx/src/read.rs)
  proves the four-byte version, 256-byte campaign name, scenario count, and
  each classic entry's signed 32-bit size/offset, 255-byte display name,
  255-byte filename, and two padding bytes.
- The same reader seeks to each indexed offset and returns exactly its declared
  size as the raw embedded scenario.

Real validation uses two fixtures from that exact commit:

- `Armies at War A Combat Showcase.cpn`;
- `DER FALL VON SACSAHUAMAN - TEIL I.cpx`.

Both are version `1.00`, contain one indexed entry, and their embedded scenario
passes the reconstruction's bounded scenario decoder. They are external test
inputs and are not bundled.

## Validation and preservation

- Campaign file limit: 256 MiB.
- Scenario count: 1–256.
- Every index record must be complete.
- Signed sizes must be positive; offsets must be nonnegative.
- Payloads must start after the complete index and remain inside the file.
- Entry ranges may not overlap.
- The two proved padding bytes must be zero.
- Campaign, entry, and filename byte strings must be nonempty.
- Physical index order is preserved even when payload offsets use another
  order.
- Every indexed payload is retained in `raw_payload`, including malformed or
  unsupported scenarios.
- Every byte outside the header/index and indexed entry ranges is retained with
  its original offset in `unindexed_payload`.
- Decoded and unsupported embedded scenario counts are reported separately.

Fixed classic names are retained as raw byte strings in `std::string`; the
inspector does not guess a locale or transcode uncertain legacy encodings.

Versions `1.10` and `2.00` return `unsupported_version`. Their string and index
layouts differ, and they are outside the requested classic AoE/AoC slice.

## Not claimed

The container does not by itself prove a campaign graph, scenario prerequisites,
unlock policy, briefing order, cinematic association, save/progress schema, or
original menu behavior. No such semantics are synthesized. Importing extracted
scenarios into current `Scenario` remains subject to
`SCENARIO_IMPORT_FIDELITY.md` and its explicit unsupported-content report.

## Tests

Hermetic tests prove:

- two-entry names, filenames, and physical order;
- one valid embedded scenario and one preserved malformed payload;
- exact decoded/unsupported counts;
- preservation of an unindexed gap;
- overlapping-range rejection;
- unsupported-version reporting.

Pinned real `.cpn` and `.cpx` fixtures prove classic index and embedded-scenario
compatibility.

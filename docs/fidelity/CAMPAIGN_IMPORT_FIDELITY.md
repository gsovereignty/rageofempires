# Commercial Campaign Import Fidelity

## Runtime contract

Classic campaign containers are supported user-owned runtime inputs. The game
inspects and losslessly preserves each container, converts its ordered embedded
scenarios into a private native cache, and uses normal campaign progression.
Native manifests remain supported authoring inputs.

## Supported slice

`inspect_legacy_campaign` recognizes `1.00`, `1.10`, and `2.00` campaign
container headers used by `.cpn`, `.cpx`, and `.cpx2`. It parses fixed and
tagged variable-length index layouts, the campaign header and
ordered scenario index, extracts every embedded payload byte-for-byte, and
passes each payload to the bounded commercial scenario inspector.

`import_legacy_campaign` converts decoded payloads with user-supplied packaged
`VER 5.7` DAT data. Import publishes atomically into user data. Physical index
order becomes linear play order. Victories unlock the next scenario and write
atomic digest-bound progress. Defeats retain the current scenario. Briefing,
debrief, completion, and next-unlocked states use the campaign presentation.

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

Unknown signatures return `unsupported_version`. Truncated strings, indexes,
invalid signed ranges, overlaps, and out-of-file payloads return bounded
product errors.

## Preserved conversion limits

Container bytes remain exact and serializer round-trip byte-for-byte. Native
runtime currently maps additional valid commercial ground textures to closest
gameplay terrain, omits a partial trigger graph when unsupported trigger nodes
would leave dangling references, and rejects independently invalid entity
placements. These choices permit bounded play while preserving raw evidence;
they do not claim exact scenario-script or texture fidelity.

## Tests

Hermetic tests prove:

- two-entry names, filenames, and physical order;
- one valid embedded scenario and one preserved malformed payload;
- exact decoded/unsupported counts;
- preservation of an unindexed gap;
- overlapping-range rejection;
- unsupported-version reporting.
- exact serializer round trips;
- synthetic `.cpx2` tagged metadata and payload parsing;
- `.cpx2` install, launchable native scenario generation, victory completion,
  and progress reload.

All nine supplied original `.cpn` and `.cpx` containers parse with every
embedded scenario decoded. Background SDL runtime smoke launches supplied
campaign containers through packaged local DAT/assets. Synthetic fixtures avoid
tracking proprietary bytes.

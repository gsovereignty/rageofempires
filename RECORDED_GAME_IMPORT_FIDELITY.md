# Commercial Recorded-Game Inspection Fidelity

## Non-requirement

Commercial recorded-game compatibility is not required for this project. This
inspector and converter are optional archaeology tooling; unsupported actions
or incomplete conversion do not block releases or project completion. Native
Replay64 is the supported runtime contract.

## Scope

The reconstruction now performs bounded inspection of classic AoC `.mgx`
recorded games. It validates and inflates the compressed header envelope,
extracts the proved eight-byte game-version string, then walks body records
whose framing is established by the commit-pinned parser:

- action records;
- synchronization records;
- game-start messages;
- chat messages.

Every parsed record retains its original file offset and exact raw bytes. At
the first unknown outer/message record, inspection stops and preserves the
complete remaining tail byte-for-byte. It does not guess an unknown record's
length and resume scanning.

This is not commercial save-game import. It can produce a native Replay v62
only for a completely represented action stream with explicit caller-supplied
timing, entity, player, and contextual-primary mappings.

## Commit-pinned evidence

Evidence is `aoc-mgx-format` commit
`c08d672006c225c4941172ddb98f657e2fda4af5`:

- [`parser/parser.rb`](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/parser/parser.rb)
  defines the leading signed 32-bit compressed-header length and next-header
  field, raw-DEFLATE header data, and body outer tags 1/2/4.
- The same file defines action length/tag/raw payload/execution tick,
  synchronization interval and conditional seven-word data, game-start/chat
  message selection, and unknown-tail consumption.
- [`header/version.rb`](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/parser/lib/aoc-mgx/header/version.rb)
  proves the eight-byte game-version field.
- [`body/commands.rb`](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/parser/lib/aoc-mgx/body/commands.rb)
  lists observed action tags.
- The commit-pinned action specifications define contextual primary action
  (`00`), move (`03`), stop (`01`), train (`77`), research (`65`), tribute
  (`6c`), multipurpose diplomacy (`67` with action type zero), and resign
  (`0b`). Their fields and constants are decoded only when the payload has the
  exact proved shape. Raw action bytes remain preserved in every case.
- Primary action is deliberately not renamed `attack`: the pinned schema says
  the same tag also represents gather, repair, conversion, healing, relic
  collection, and embark according to target context.
- Multipurpose `67` payloads for game speed, cheats, allied victory, treason,
  or AI policy remain unsupported and retain their exact bytes.
- [`body/chat.rb`](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/parser/lib/aoc-mgx/body/chat.rb)
  proves signed length-prefixed chat.

The parser's detailed `RecordedGame` header structure is experimental and not
applied at decompressed offset zero in its executable parser. A real pinned
fixture disproves doing so. Therefore patch, owner, player-count, speed, map,
trigger, and player metadata are not extracted from assumed offsets.

## Validation contract

- File limit: 512 MiB.
- Inflated header limit: 256 MiB.
- Compressed-header length must exceed its eight-byte envelope and remain
  inside the file.
- Raw DEFLATE must reach end-of-stream and consume every compressed byte.
- Action payload limit: 1 MiB; length must include at least its action tag and
  leave the execution-tick field.
- Chat limit: 64 KiB.
- Record limit: 5,242,880.
- Synchronization and game-start fixed fields must be complete.
- Floats inspected in synchronization records must be finite.
- Unknown outer/message tags terminate structured parsing without data loss.

The result preserves:

- compressed header bytes;
- complete decompressed header bytes;
- game-version string;
- raw bytes and file offset for every proved record;
- action tag/execution tick, synchronization interval, message tag, and chat
  text where their framing is proved;
- typed fields for exact validated action payloads, plus per-action schema
  validity and diagnostics;
- a typed neutral timeline carrying file offset, commercial `execute_at`, all
  decoded fields, and exact raw action payload;
- a required-mapping envelope containing every proved action-stream tick,
  player number, entity ID, commercial unit ID, and technology ID;
- every contextual-primary record offset whose meaning must come from
  independently decoded object state;
- exact unsupported tail and its original offset.

## Replay conversion contract

Conversion returns the exact action list, decoded/unsupported counts,
unsupported-tag counts, and concrete blockers. It returns a Replay only when
all records are representable and the unsupported tail is empty.

Commercial `execute_at` values are never assumed to be reconstruction ticks.
Every observed value requires an explicit mapping. Commercial object IDs,
players, unit IDs, and technology IDs likewise require explicit mappings where
the native command needs them. The experimental variable-layout header parser
is not a proved mapping source, so the envelope inventories only fields from
strictly decoded actions.

Lossless native conversions cover:

- stop, one command per explicitly mapped selected entity;
- integral-coordinate move with explicit selection and complete player/entity
  mappings;
- human train with building/unit mappings, preserving batch count as repeated
  queue commands at the same tick;
- research with building/player/technology mappings;
- diplomacy with exact allied/neutral/enemy encoding and player mappings;
- canonical resign when player number and player ID agree, disconnect is zero,
  and an explicit player mapping exists.
- contextual primary gather of a proved herdable, convert, heal, relic
  collection, and embark, when the caller explicitly maps the preserved record
  offset to that context and supplies complete player/entity/tick mappings.
  One native command is emitted per explicitly encoded selected entity.

Primary context is never inferred from the action payload. Tag `00` carries
only issuing player, target object ID, selected object IDs (or a previous-
selection sentinel), and target coordinates. The validated header prefix does
not provide a proved object-state table. Therefore target ownership, object
type, health, carried relic state, transport capacity, and actor capability
must come from an independent decoder. Record offset is the stable key because
every parsed action preserves it exactly.

The remaining decoded actions stay exact partial timeline entries:

- contextual primary action retains target, coordinates, issuing player,
  explicit IDs or previous-selection sentinel. Missing context, selection
  reuse, or incomplete mappings refuse the complete Replay atomically while
  preserving raw bytes;
- targeted attack and repair remain blockers even with proved context because
  native `GameCommand` has no exact targeted-attack or repair variant;
- general gather remains a blocker unless independent state proves the native
  herdable-target command is exact;
- move with fractional/out-of-range coordinates or previous-selection reuse
  cannot be represented losslessly;
- tribute retains both float amount and transaction fee; the native command
  represents neither combination losslessly;
- non-diplomacy multipurpose `67` payloads remain unsupported.

## Fixture evidence

Hermetic tests cover one record of every outer supported kind, all eight typed
action schemas, mapping-envelope inventory, neutral timeline ordering, strict
constants, raw unsupported-action preservation, unsupported counts,
explicit-map replay conversion, corrupt DEFLATE, oversized action length, and
unknown-tail preservation.

The real pinned fixture `Die Schilderung einer Schlacht.mgx` is approximately
5 MiB (5,280,844 bytes), identifies as `VER 9.4`, yields 2,494 proved records,
64 strictly decoded actions, 124 unsupported actions, and preserves the
remaining 5,046,881-byte tail. It remains external and is not bundled.

## Save and replay separation

Commercial `.gax`/`.ga1` saved games are not decoded. Their ability to resume
simulation is distinct from an `.mgx` action stream.

Reconstruction Replay v62 remains a native deterministic command format and
is not wire/file compatible with `.mgx`. A report with partial actions is not
silently promoted to Replay.

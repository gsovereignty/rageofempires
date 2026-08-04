# Multiplayer Fidelity Contract

## Scope

This document separates original Age of Kings/Age of Conquerors observations
from a bounded reconstruction-native multiplayer design. It does not claim
DirectPlay, Zone, original packet, matchmaking, or save compatibility. Supplied
HD executable Steam transport evidence and recovered outer control records are
pinned in `../evidence/COMMERCIAL_MULTIPLAYER_PROTOCOL_EVIDENCE.md`.

## Evidence hierarchy

1. **Original Microsoft manual.** The *Age of Empires II: The Age of Kings*
   manual is first-party evidence for visible player roles, lobby behavior,
   latency presentation, diplomacy, chat, and save/restore behavior.
   [Original Microsoft manual](https://manuals.plus/m/bede02f48ca7b2aae252168379555520636b871dcd6c6fdba8ba2766ea0186e2.pdf)
2. **Commit-pinned AoC traffic documentation.** The open-source `openage`
   repository at commit `9a5a7ccbfc20c2de658fc746462cd4a69aa758ef`
   documents packet captures from four hosts running AoC 1.0c. It distinguishes
   synchronization, chat, and player-action packets; communication turns,
   latency, and de-sync data; player/network IDs; and eight lobby slots.
   [General protocol observations](https://github.com/SFTtech/openage/blob/9a5a7ccbfc20c2de658fc746462cd4a69aa758ef/doc/reverse_engineering/networking/01-general.md),
   [synchronization observations](https://github.com/SFTtech/openage/blob/9a5a7ccbfc20c2de658fc746462cd4a69aa758ef/doc/reverse_engineering/networking/03-sync.md),
   and
   [lobby observations](https://github.com/SFTtech/openage/blob/9a5a7ccbfc20c2de658fc746462cd4a69aa758ef/doc/reverse_engineering/networking/04-lobby.md).
3. **Commit-pinned recorded-game decoder.** `aoc-mgx-format` commit
   `c08d672006c225c4941172ddb98f657e2fda4af5` describes recorded player
   actions, synchronization records, and chat records. It corroborates an
   action-stream representation; it is not an original networking
   implementation.
   [Recorded action structures](https://github.com/stefan-kolb/aoc-mgx-format/tree/c08d672006c225c4941172ddb98f657e2fda4af5/spec/body/actions),
   [synchronization record](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/spec/body/synchronization/Synchronization.md),
   and
   [chat record](https://github.com/stefan-kolb/aoc-mgx-format/blob/c08d672006c225c4941172ddb98f657e2fda4af5/spec/body/chat/0xFFFFFFFF%20%28Chat%29.md).
4. **Current reconstruction.** `GameCommand`, Replay v63, Save v109, Scenario
   v65, deterministic triggers, and simulation tests define available project
   primitives only.

At the pinned openage commit, the current input/network implementation
documentation still marks network input as `TODO`. Its reverse-engineering
notes are packet observations, not a complete reusable lockstep engine.

## Transferable original facts

The Microsoft manual proves:

- up to eight participants can connect over a network or the Internet; modem
  and serial connections are limited to two (manual, pp. 22–23);
- a creator hosts the lobby, chooses settings, and starts only after every
  occupied participant reports ready (manual, pp. 23–24);
- players select civilization, player number/starting color, and team
  (manual, p. 24);
- multiple humans may select one player number and share unrestricted control
  of one civilization, including conflicting orders (manual, p. 24);
- team selection initially sets allied diplomacy and allied victory; unlocked
  teams may change alliances during play (manual, p. 24);
- lobby chat exists, in-game chat can address allies, and map signals can be
  sent to allies (manual, pp. 23–24 and 54–55);
- game speed is constrained by the slowest computer; the UI reports slow frame
  rate and network latency, with yellow at 300 ms–1 s and red above 1 s
  (manual, p. 25);
- a disconnected participant cannot rejoin the running game (manual, p. 25);
- multiplayer games can be saved and restored; restore requires at least two
  opposing participants, and the original host must host a restored game that
  includes computer players (manual, p. 25);
- after resignation, a participant may observe but cannot issue commands or
  chat (manual, p. 26).

The commit-pinned traffic notes additionally observe:

- TCP session connection followed by UDP lobby and in-game traffic in the
  captured AoC 1.0c environment;
- separate synchronization, chat, and action packet families;
- periodic synchronization carrying communication-turn, latency, and
  last-known-in-sync information;
- player-issued actions for movement, production, research, diplomacy, save,
  resignation, tribute, and other commands;
- distinct connection/player identifiers and a non-unique player number used
  for cooperative shared control;
- eight lobby network-ID, civilization, and team slots;
- a close relationship between captured action data and recorded-game action
  data.

These support a deterministic action-stream design. They do not prove that any
specific reconstruction protocol matches the original wire behavior.

## Unknown original behavior

The evidence does not establish:

- the complete DirectPlay/Zone connection, discovery, NAT, or relay protocol;
- authoritative checksum algorithm or exact state covered by it;
- action acknowledgement, retransmission, duplicate suppression, or loss
  recovery rules;
- exact communication-turn duration, adaptive input delay, or slow-peer
  scheduling;
- timeout, drop-vote, AI takeover, host migration, or pause rules;
- packet-size limits and malformed-packet handling;
- authentication, identity, privacy, or moderation policy;
- exact chat routing and ordering under packet loss;
- whether saved multiplayer state contains every lobby/session field;
- any live reconnect path, which the manual explicitly says was unavailable.

All are `UNKNOWN`. Captured field labels and model-generated interpretations
are hypotheses unless directly demonstrated by the pinned source.

## Bounded reconstruction v1

### Supported topology and roles

- Exactly two human processes participate: one owns blue, one owns red.
- One process is session coordinator (“host”), but has no privileged
  simulation commands.
- Computer players, cooperative same-slot control, observers, live joining,
  host migration, matchmaking, ranking, relays, and original clients are out
  of scope.
- Transport is one reliable ordered byte stream per peer. This chooses
  simplicity over reproducing captured TCP/UDP behavior.

### Immutable handshake

Before tick zero, every occupied participant must agree exactly on:

```text
protocol_version
build_id
game_command_schema_version
save_version
scenario_version
scenario_digest
content_and_rules_digest
tick_rate
input_delay_ticks
deterministic_seed
roster {
  player_slot, occupied, team, cooperative_control,
  civilization, controllers {kind, id}
}
directed_diplomacy
host_slot
```

Each peer sends the complete canonical configuration and independently
computes its stable FNV-1a-64 configuration digest. The received canonical
configuration must be an exact field-for-field echo before that peer may become
ready. Mismatches terminate negotiation as protocol, build, schema, scenario,
content, settings, or roster failures. Player slots,
scenario, rules, seed, and delay do not mutate after start.

Every frame is length-prefixed and contains protocol version, message kind,
session ID, monotonic sender sequence, and payload. Reject unknown kinds,
duplicate singleton fields, noncanonical encodings, oversized lengths, invalid
enums, and trailing bytes before changing session state.

### Lockstep input

At local simulation tick `T`, a local command is assigned execution tick
`T + input_delay_ticks`. Each participant submits exactly one bundle for each
execution tick, including an explicit empty bundle.

Bundle identity is:

```text
(session_id, execution_tick, player_slot, bundle_sequence)
```

Commands inside a bundle retain local issue order. The canonical global order
is:

```text
(execution_tick, player_slot, command_index)
```

Packet arrival order, wall-clock time, host arrival order, and container
iteration order never affect simulation order. A duplicate bundle with
identical bytes is ignored; the same identity with different bytes terminates
the session as inconsistent input.

Tick `T` advances only when valid bundles for `T` exist for every occupied
slot. Apply
all commands in canonical order, then call exactly one simulation update.
Commands rejected by deterministic game rules remain rejected on every peer;
the rejection result is included in the next hash checkpoint.

### State hash

Every 50 committed simulation ticks, every peer calculates SHA-256 over a
versioned canonical state encoding. Prefix every section and variable field by
domain, index, and length. Include:

- tick, match rules/outcome/countdowns, economies, ages, civilizations,
  technologies, diplomacy, market prices, formations, exploration;
- all authoritative unit/building/projectile/effect fields in stable entity-ID
  order, including next-ID allocation state;
- objective, trigger, and message runtime state;
- deterministic AI/RNG state when those systems become supported;
- the last applied bundle identity for every occupied slot.

Exclude rendering, audio, input UI, camera, selection highlights, filesystem
paths, wall-clock timestamps, network metrics, and caches.

Peers exchange `(checkpoint_tick, hash)`. They compare only the same committed
tick. A mismatch pauses before the next tick and writes a diagnostic containing
handshake digest, recent canonical bundle bytes, and both hashes. Version 1
does not elect a winner, repair state, or continue after mismatch.

### Timing, timeout, and disconnection

- Send a heartbeat at least once per second while no bundle traffic exists.
- After 5 seconds without required input, stop simulation advancement and show
  the waiting peer; do not synthesize an empty bundle.
- After 30 seconds without traffic, mark the session suspended. Preserve the
  last committed tick and allow a coordinated save, but do not replace the
  peer with AI or transfer ownership.
- Version 1 has no live reconnect. Resume creates a new lobby from a saved
  checkpoint.

These durations are reconstruction policy, not original values.

### Chat, diplomacy, and resignation

Chat is a separate bounded message stream with host-assigned display sequence.
It carries sender slot, audience (`all` or `allies`), UTF-8 text, and maximum
byte length. Chat does not enter simulation hashes or native Replay.

Diplomacy, tribute, signals that affect simulation, speed changes, and
resignation are typed game commands and follow lockstep ordering. Ephemeral
allied map signals are instead a bounded sequenced side channel, like chat, and
do not enter hashes or replay. After a
terminal resignation outcome, no further state-changing command is accepted.
Observer-after-resignation behavior is deferred.

### Save and resume

Saving uses a lockstep barrier:

1. Coordinator proposes barrier tick `B`.
2. Every occupied participant commits through `B`, exchanges the state hash
   for `B`, and stops.
3. Only a complete matching quorum writes the current native Save.
4. Write a separate multiplayer envelope containing protocol/build IDs,
   scenario/content digests, seed, roster/slot ownership, input delay, barrier
   tick/hash, and last accepted bundle sequence per player.
5. Resume is a new session handshake that accepts only the exact Save and
   envelope digests.

Partial or hash-mismatched barriers produce no resumable checkpoint. There is
no hot reconnect or arbitrary participant substitution.

## Limits

Initial conservative limits:

- two to eight occupied participant slots in the transport-independent core;
- 1 MiB maximum framed message;
- 64 KiB maximum input bundle;
- 256 commands per player per tick;
- 4 KiB maximum chat text;
- 4,096 buffered future ticks;
- 50-tick hash interval;
- sequence and tick counters are unsigned 64-bit and reject wraparound.

These are protocol validation and resource-bound contracts, not claims about
the original game.

## Implementation status

The bounded contract above is the target design. The current transport-
independent core implements only the following subset:

| Area | Implemented now | Deferred target |
| --- | --- | --- |
| Roles | Stable slots 0-7; occupied-slot ownership checks; immutable controllers/team/cooperative-control/civilization roster and directed diplomacy; explicit host slot | Authenticated identity and remote lobby policy |
| Handshake | Protocol 3, build, command/save/scenario schemas, scenario/content digests, cadence, delay, seed, exact canonical config echo/digest, categorized mismatch, all-participant hello/ready, explicit-host start | Authenticated identity and remote lobby policy |
| Frames | Length prefix, 1 MiB cap, strict parse/trailing-byte rejection, localhost TCP stream with partial/coalesced reads | General remote transport integration and 64 KiB bundle limit |
| Turns | Negotiated input delay schedules commands from issue tick `T` at `T+delay`; every occupied slot submits, including explicit empty bundles; apply order is ascending stable slot; replay records execution tick and source; sequence equals execution tick | Independent bundle sequence |
| Duplicates | Identical frame accepted; conflicting same player/tick causes de-sync | Session-qualified bundle identity |
| Hash | Current native Save bytes, including full roster/state, plus ID-allocation cursors hashed with FNV-1a-64; configurable interval, default 50 | Canonical SHA-256 state encoding every 50 ticks |
| Timing | Monotonic one-second ping/pong heartbeat, RTT and last-peer-traffic metrics, 5-second waiting stall, 30-second suspension at the last committed tick, green/yellow/red latency bands; injected-clock tests; timing never changes command ordering/outcomes | Broader adaptive network policy |
| Pause/speed | Host proposal, peer acknowledgement, and host commit at a committed-tick barrier; unilateral/duplicate controls rejected; pause blocks simulation advancement while transport remains live; bounded reconstruction slow/normal/fast cadence; state persisted in checkpoint envelope v3 | Commercial speed values/adaptation remain unknown |
| Drop policy | Transport loss suspends but is not terminal; only explicit host drop or explicit peer disconnect terminates; no AI takeover, ownership transfer, live reconnect, or host migration | Coordinated checkpoint UI during suspension |
| Disconnect | Terminal disconnect; no rejoin | Coordinated save on suspension |
| Replay | Accepted commands recorded in canonical apply order | Rejection-result and bundle metadata |
| Command codec | All current `GameCommand` variants; frames carry stable slot source; most commands reuse canonical Replay records | Version-independent binary command schema |
| Supplied HD seam | Strict synthetic-golden codecs for recovered Steam P2P outer records `0x1f5`, `0x1f8`, and fixed-size `0x259`; exact observed lobby metadata keys; runtime-loaded licensed-service adapter ABI; deterministic eight-account mock covering discovery, metadata, auth, reliable channel-0 P2P, migration, and rejoin | Live Steam adapter proof, legacy inner commercial packet schemas, and original-client interoperability proof |
| Localhost driver | Legacy blocking/nonblocking host-join runtime remains intact. Additive configured star-relay harness accepts up to seven bound peer streams, parses a canonical ascending occupied roster, routes protocol-3 frames by stable slot, broadcasts join-origin bundles, preserves stream framing/duplicates, waits all-slot quorum, and propagates whole-session disconnect | Main-match integration and remote lobby policy |
| SDL proof | Existing two-process match smoke remains. A dedicated real-SDL localhost lobby harness renders all occupied rows/readiness plus checkpoint/reconnect state. Its two-phase smoke joins all eight slots, verifies one hash, transfers a strict checkpoint, then creates a fresh eight-slot relay from that checkpoint and verifies the resumed hash across every peer | Discovery and remote-network operation |
| Chat | Host-sequenced `all`/`allies` UTF-8 messages, 4 KiB byte cap, bounded lobby/running transport log, excluded from simulation hash and replay | UI composition/history controls and moderation policy |
| Save/resume | Host-requested committed-tick barrier, complete occupied-roster state-hash quorum, mismatch rejection, durable atomic native Save plus checkpoint envelope v3 containing exact config/save digests, roster, tick/hash, and ascending slot/sequence pairs; strict load for a new exact-config lobby. Local SDL harness exposes matched transfer and bounded fresh-lobby reconnect states | Remote checkpoint transport |

Transport-independent core supports eight slots. Configured localhost TCP
harness supports one host plus up to seven explicitly bound peer streams.
The main match SDL runtime remains one-host/one-join. The dedicated localhost
lobby/recovery SDL executable covers the complete eight-slot relay and strict
fresh-lobby reconnect flow, but does not provide matchmaking, discovery,
authentication, hot reconnect, host migration, cooperative slots, Internet
transport, or original-protocol compatibility. Caller-counted timeout steps
are not seconds.
The current FNV hash is a deterministic regression checksum, not the target
cryptographic/canonical hash and not a claim about the original algorithm.

## Verification contract

### Determinism and ordering

- Run two simulations with the same handshake and randomized packet
  fragmentation, duplication, and interleaving; hashes must match every
  checkpoint.
- Reverse arrival order for simultaneous multi-slot commands; canonical results
  must remain identical.
- Route fragmented frames through a three-peer localhost star, repeat an
  identical turn bundle, require all three before advance, compare hashes and
  replay source order, then prove one EOF terminates host and remaining peer.
- Exercise conflicting shared targets, simultaneous terminal outcomes,
  triggers, creation-ID allocation, and queued commands.
- Compare uninterrupted execution with current native-Save barrier/resume.

### Network faults

- Drop, duplicate, delay, and reconnect transport fragments without duplicating
  bundles.
- Confirm a missing explicit empty bundle stalls the tick.
- Confirm 5-second waiting and 30-second suspension never advance simulation.
- Reject conflicting duplicate identities, stale-session frames, tick/sequence
  wraparound, oversized lengths, truncated payloads, and trailing bytes.

### Hash coverage

- Mutate each authoritative serialized field independently and require a hash
  change.
- Mutate camera/audio/UI/cache state and require no hash change.
- Force a known mismatch and verify pause plus deterministic diagnostic output.
- Verify hashes are stable across supported platforms and build modes.

### Roles, chat, and resume

- Reject commands for a slot the sender does not own.
- Verify allied/all chat routing and stable display sequence without changing
  simulation hashes.
- Verify defeat/resignation does not permit further state commands.
- Reject restore with changed scenario/content/build/protocol digest, roster,
  slot ownership, barrier tick, or checkpoint hash.
- Verify no live-rejoin or host-migration path is accidentally exposed.

Rows marked implemented above form the testable prototype. All deferred rows
remain design requirements until implemented and tested. Existing native
Replay and Save determinism are prerequisites, not a network subsystem. Resume
always creates a new lobby after strict save/envelope verification; neither
the checkpoint API nor runtime exposes live reconnect.

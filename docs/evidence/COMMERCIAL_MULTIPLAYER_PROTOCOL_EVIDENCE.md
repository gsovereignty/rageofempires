# Supplied HD Multiplayer Protocol Evidence

## Result

The supplied `AoK HD.exe` cannot interoperate through the reconstruction's
TCP protocol. Its reachable multiplayer path is coupled to Steam lobby,
identity, P2P transport, and authentication APIs. Direct-IP or LAN discovery
alone cannot make an original HD client join.

This finding keeps `BUG-MULTIPLAYER-001` open. The implementation added here
only pins the recovered byte-level outer Steam P2P control records. It does not
claim original-client compatibility.

## Read-only original evidence

Supplied executable SHA-256:

```text
02ccff32765e19f4f75be5454c74e85bd78cc2fccea44ba21176371f715aa1fb  AoK HD.exe
```

`decompiled/pe-metadata-and-imports.txt:680-729` proves imports for
`SteamMatchmaking`, `SteamMatchmakingServers`, and `SteamNetworking`.

Recovered `AoK-HD-patched.c` behavior:

- `FUN_004a1640` calls Steam lobby `JoinLobby` for the selected 64-bit lobby
  identifier.
- `FUN_004a1cc0` and `FUN_004a1e60` read lobby metadata keys `name_salt`,
  `title_salt`, `SlotCount`, and `SlotsFilled`.
- `FUN_004a2140` adds Steam lobby filters for game/map/resource/victory,
  latency, cheats, and full-lobby state.
- `FUN_004a29f0`, `FUN_004a3b60`, and `FUN_004a3c30` send/read through
  `SteamNetworking` P2P channel 0 using reliable delivery mode 2.
- `FUN_004a2d80` builds a fixed 0x410-byte authentication record: little-endian
  kind `0x259`, 32-bit ticket length, 1024-byte ticket field, and 64-bit sender
  Steam ID at offset `0x408`.
- `FUN_004a2ee0` passes received ticket bytes and sender Steam ID to Steam
  authentication. `FUN_004a2a60` consumes Steam's asynchronous validation
  result.
- `FUN_004a3c30` recognizes little-endian control kinds `0x1f5` and `0x1f8`.
  `FUN_004a4010` proves `0x1f5` is a 12-byte record containing kind plus the
  local 64-bit Steam ID. `FUN_004a4060` emits the 4-byte `0x1f8` record after
  authentication handling.
- `FUN_004a34d0` keeps ten authentication slots internally while game lobby
  presentation exposes eight player slots. It rejects creation outside the
  join window and duplicate slot assignment.

These facts prove framing and dependency boundaries. Further inspection shows
that `FUN_0049c470` is not an inner Steam-message dispatcher: after accounting
and a receive guard it forwards the sender, byte count, peer identifiers, and
flags into legacy network core `FUN_0049b4f0`. `FUN_004a3c30` passes every P2P
record not consumed as `0x1f5`, `0x1f8`, or `0x259` through that same boundary.
Consequently Steam adds authentication and transport around the pre-existing
legacy game protocol; recovering its packet schemas requires the legacy send,
receive, acknowledgement, and command dispatch graph, not more Steam framing.

## Reconstruction artifact

`commercial_multiplayer_protocol` implements strict codecs for the three
proved outer records and exposes exact observed lobby-key spellings. Synthetic
goldens pin byte order, sizes, offsets, maximum ticket length, truncation, and
wrong-kind rejection. No original binary, Steam ticket, credential, or service
secret is copied into the repository.

`commercial_multiplayer_service` adds a runtime-loaded adapter boundary for
locally licensed Steamworks integration. Its hermetic backend proves eight
accounts, discovery/full-lobby behavior, owner-only metadata, identity tickets,
authentication rejection, reliable channel-0 P2P ordering, explicit and
automatic host migration, and leave/rejoin. ABI and ownership rules are in
`../contracts/COMMERCIAL_MULTIPLAYER_ADAPTER.md`. This is service-boundary
coverage, not evidence of byte-compatible inner gameplay packets.

## Irreducible interoperability gap

An original-client proof needs all of:

1. a licensed, authenticated 32-bit Windows Steam environment running the
   supplied client;
2. a Steam application identity accepted by that client and service;
3. a locally licensed ABI-v1 Steam adapter and live callbacks proving lobby,
   P2P session establishment, and per-peer auth tickets;
4. recovered inner lobby/game packet schemas and behavior;
5. a two-client capture proving discovery/join, eight-slot lobby state,
   match start, commands, synchronization, disconnect behavior, cooperative
   slots, and any supported migration behavior.

Repository contains supplied binaries but no authenticated Steam test identity
or hermetic Steam service implementation. Synthetic codec tests cannot replace
that external proof. Therefore no production original-compatible mode is
exposed and TODO status remains `CONFIRMED`.

## Inner protocol recovery checkpoint

`FUN_0049b4f0` is bounded as the legacy receive root, but it is not one flat
packet switch. It first calls `FUN_0049b330` for file-transfer records, then
`FUN_00496ed0` for pregame notifications, handles transport/control records,
and finally passes ordered payloads to `FUN_0049a2a0`. This ordering matters:
decoding only the final switch mistakes file, pregame, and reliability traffic
for simulation commands.

The decompiled receive graph proves these first-byte families:

| Family | Proven receive behavior |
|---|---|
| `'#'`, `'$'`, `'%'`, `'^'` | File announce/data/terminal/ack traffic, dispatched by `FUN_0049b330`. |
| `'5'` | Pregame subtypes `'0'`, `'2'`, `'4'`-`'9'`, including slot notification, file request, start notification, and host-migration exit. |
| `'I'` | Peer rejection/disconnect during setup. |
| `'Z'` | Variable shared-player/options record: 12-byte envelope, packed `0x98`-byte mini-options, then variable data. |
| `'{'` | Setup text/name record. |
| `'A'` | Acknowledge; 32-bit serial at offset 4. |
| `'R'` | Ready/options summary; byte fields at offsets 1-6 and 12, 32-bit value at offset 8. |
| `'1'`/`'2'` | Ping/pong; 32-bit timestamp at offset 4. |
| `'3'`/`'4'` | Debug ping/pong; 32-bit timestamp at offset 4. |
| `'?'` | Missing-player report; 64-bit peer identifier at offsets 8 and 12. |
| `'Y'` | Missing-serial response; 32-bit serial at offset 4. |
| `'X'` | Retransmit request; 32-bit serial at offset 4. |
| `'K'` | Kill-player control record. |
| `'C'` | Chat record with setup/in-game form selected by reliable-mode state. |
| other ordered records | 8-byte legacy header or 12-byte reliable header, then `FUN_0049a2a0` payload switch. |

`FUN_0049a2a0` proves ordered payload opcodes `+`, `8`, `>`, `C`, `D`, `K`,
`M`, `N`, `P`, `Q`, `S`, `U`, `W`, and `}`. Their observed roles include
pause/resume requests, turn progress, chat fan-out, kill/drop, checksum or
state reporting, and player-status changes. This is useful dispatch evidence,
not yet a complete wire contract.

### Exact bytes still missing

Current decompiler output cannot safely define byte-compatible builders for
this graph without instruction-level recovery:

1. Calls to `FUN_0048ffc0` have a lost `thiscall` receiver and shifted
   arguments. Decompiled call expressions frequently show a 12- or 16-byte
   send sourced from locals for which only bytes 0, 1, and 4-7 are assigned.
   Whether bytes 2-3 and 8-15 are initialized, retained, or transport-owned
   must be recovered at each machine-code call site. Filling them with zero
   would invent protocol bytes.
2. Reliable records change from an 8-byte to 12-byte header according to
   `mRGE_Guaranteed_Delivery` (`+0x1dd8`). Exact header ownership and sequence
   initialization span `FUN_00493400`, `FUN_00493850`, `FUN_00493a30`,
   `FUN_00493e60`, `FUN_004940a0`, and the provider vtable call at
   `FUN_0048ffc0`; no isolated packet function proves the contract.
3. Shared options use `FUN_00490130` and `FUN_004906f0` to translate between a
   packed `0x98`-byte form and a `0x238`-byte internal form. Many fields are
   bit-packed, reserved, or derived from runtime state. Field semantics and
   valid state transitions remain unnamed.
4. File traffic includes a `0x1a0`-byte announcement and `0x20c`-byte data
   record, but position, payload length, checksum, retry window, and terminal
   subtypes span `FUN_00491e20`, `FUN_004923e0`, `FUN_00495af0`,
   `FUN_00495e50`, `FUN_00496380`, `FUN_00496400`, `FUN_004990d0`,
   `FUN_00499510`, and `FUN_0049a9a0`.
5. Provider callbacks and system messages feed join, destroy-player, and host
   migration through `FUN_00499a00`. Packet codecs alone cannot emulate these
   service events or prove reconnect/migration behavior.
6. Simulation payloads enter game/replay command handling outside this network
   cluster. Existing classic recorded-game support is intentionally partial,
   so it cannot serve as a complete shared-command codec.

Binary provenance also constrains raw recovery. The decompiled image is the
4,444,160-byte unpacked/patched executable with SHA-256
`e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`.
The supplied 56,320-byte Steam image has SHA-256
`02ccff32765e19f4f75be5454c74e85bd78cc2fccea44ba21176371f715aa1fb`
and does not expose the same code body at virtual address `0x49b4f0` to static
disassembly. Raw instruction work can validate the unpacked image, but final
original-client compatibility still requires live authenticated captures from
the supplied client.

This checkpoint prevents partial schemas from being promoted as compatible.
Repository-only work can continue by disassembling every listed builder and
recovering the shared replay command graph, but it cannot complete required
live interoperability proof without licensed environment and captures listed
above.

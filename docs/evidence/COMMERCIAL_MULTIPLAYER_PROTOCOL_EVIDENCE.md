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

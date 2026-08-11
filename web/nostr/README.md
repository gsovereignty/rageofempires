# Browser Nostr runtime

This module is browser-only. It is bundled as one IIFE and loaded before the
Emscripten application. Static hosting remains sufficient.

Pinned public APIs (Applesauce 6.2 release family):

- `RelayPool` from `applesauce-relay`: `req(relays, filters, options)` exposes
  typed per-relay `OPEN`, `EVENT`, `EOSE`, `CLOSED`, and error results;
  `publish(relays, event, options)` returns per-relay `PublishResponse`; `close()`
  tears down relays and reconnect timers.
- `EventStore` from `applesauce-core`: `add(event, relay)` validates and inserts;
  `hasEvent(id)` provides raw-ID deduplication; `dispose()` tears down store
  subscriptions.
- `EventFactory` from `applesauce-core`: `fromKind(kind)`, `content(value)`,
  `modifyPublicTags(operation)`, `as(signer)`, and `sign()` build and sign.
- `PrivateKeySigner` from `applesauce-signers` creates an ephemeral in-memory
  browser identity and implements `getPublicKey()` plus `signEvent()`.
- Relay authentication is never configured. Requests set `waitForAuth: false`.
  Relay status fields `authRequiredForRead` and `authRequiredForPublish` cause
  that relay to be removed from the active pool.

Kinds follow NIP-78: addressable `30078` for current lobby; regular stored `78`
for append-only match history. Every event is additionally scoped by `t`, `v`,
and `m` tags.

Private keys remain inside the signer object. They are never serialized,
persisted, logged, or passed through the WASM boundary.

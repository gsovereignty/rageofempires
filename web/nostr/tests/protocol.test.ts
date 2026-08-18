import assert from "node:assert/strict";
import test from "node:test";

import {EventFactory, EventStore} from "applesauce-core";
import type {NostrEvent} from "applesauce-core/helpers/event";
import {PrivateKeySigner} from "applesauce-signers";

import {
  APP_TAG,
  LOBBY_KIND,
  lobbyDiscoveryFilters,
  makeMatchReference,
  matchSubscriptionFilters,
  MATCH_KIND,
  parseMatchReference,
  sameRelayPool,
  validateIntent,
  validateRelays,
} from "../src/protocol.js";
import {
  collectPublishQuorum,
  parseLobbyAnnouncement,
  readyPublishRelays,
  relayStatusFingerprint,
  relayPoolDigest,
} from "../src/runtime.js";

test("pinned Applesauce factory signs and EventStore deduplicates", async () => {
  const signer = new PrivateKeySigner(new Uint8Array(32).fill(7));
  const event = await EventFactory.fromKind(MATCH_KIND)
    .content('{"family":"test","protocol":1}')
    .modifyPublicTags((tags) => [...tags, ["t", APP_TAG]])
    .as(signer)
    .sign();
  const store = new EventStore();
  assert.equal(store.hasEvent(event.id), false);
  assert.equal(store.add(event, "wss://relay.example/")?.id, event.id);
  assert.equal(store.hasEvent(event.id), true);
  assert.equal(store.add(event, "wss://relay-two.example/")?.id, event.id);
  assert.equal(store.getByFilters({ids: [event.id]}).length, 1);
  store.dispose();
});

test("public match reference round trips bounded relay hints", () => {
  const reference = makeMatchReference({
    hostPubkey: "a".repeat(64),
    matchId: "b".repeat(64),
    relays: ["wss://relay.damus.io", "wss://nos.lol"],
  });
  assert.deepEqual(parseMatchReference(reference), {
    hostPubkey: "a".repeat(64),
    matchId: "b".repeat(64),
    relays: ["wss://relay.damus.io/", "wss://nos.lol/"],
  });
});

test("relay and intent bounds reject unsupported inputs", () => {
  assert.throws(() => validateRelays(["ws://localhost"], true), /wss/);
  assert.throws(() => validateRelays(["wss://one.example"], false), /2-20/);
  assert.doesNotThrow(() => validateRelays(
    Array.from({length: 20}, (_, index) => `wss://relay-${index}.example`),
    false,
  ));
  assert.throws(() => validateRelays(
    Array.from({length: 21}, (_, index) => `wss://relay-${index}.example`),
    false,
  ), /2-20/);
  assert.throws(() => validateIntent({
    intent_id: "x",
    kind: 9999,
    tags: [["t", APP_TAG]],
    content: "{}",
  }), /unsupported/);
  assert.doesNotThrow(() => validateIntent({
    intent_id: "lobby-1",
    kind: LOBBY_KIND,
    tags: [["t", APP_TAG]],
    content: "{}",
  }));
});

test("ordered relay pool has stable SHA-256 identity", async () => {
  assert.equal(
    await relayPoolDigest(["wss://one.example/", "wss://two.example/"]),
    "acf341ed957885b01511f287f7f380ea8e7a63b247b0fc84fca3b3dec77c292d",
  );
  assert.notEqual(
    await relayPoolDigest(["wss://two.example/", "wss://one.example/"]),
    await relayPoolDigest(["wss://one.example/", "wss://two.example/"]),
  );
});

test("production relay identity rejects subsets and reordered pools", () => {
  const production = ["wss://one.example/", "wss://two.example/"];
  assert.equal(sameRelayPool(production, [...production]), true);
  assert.equal(sameRelayPool(production, production.slice(0, 1)), false);
  assert.equal(sameRelayPool(production, [...production].reverse()), false);
});

test("publication uses ready relays without changing configured pool", () => {
  const configured = [
    "wss://ready-one.example/",
    "wss://offline.example/",
    "wss://disabled.example/",
    "wss://ready-two.example/",
  ];
  const statuses = new Map([
    [configured[0], {connected: true, ready: true}],
    [configured[1], {connected: false, ready: false}],
    [configured[2], {connected: true, ready: true}],
    [configured[3], {connected: true, ready: true}],
  ]);
  assert.deepEqual(
    readyPublishRelays(configured, new Set([configured[2]]), statuses),
    [configured[0], configured[3]],
  );
  assert.equal(configured.length, 4);
});

test("publication completes when relay quorum accepts", async () => {
  const attempted: string[] = [];
  const results = await collectPublishQuorum(
    ["wss://one/", "wss://silent/", "wss://two/"],
    2,
    async (relay) => {
      attempted.push(relay);
      if (relay.includes("silent")) return new Promise(() => {});
      return [{from: relay, ok: true, message: "saved"}];
    },
  );
  assert.deepEqual(attempted, ["wss://one/", "wss://silent/", "wss://two/"]);
  assert.equal(results.filter((result) => result.ok).length, 2);
});

test("relay status fingerprint changes only bridge-relevant fields", () => {
  const ready = {
    connected: true,
    ready: true,
    authRequiredForRead: false,
    authRequiredForPublish: false,
  };
  assert.equal(relayStatusFingerprint(ready), relayStatusFingerprint({...ready}));
  assert.notEqual(
    relayStatusFingerprint(ready),
    relayStatusFingerprint({...ready, ready: false}),
  );
});

test("match subscriptions stay within ordinary relay tag limits", () => {
  const filters = matchSubscriptionFilters("host-public-key", "match-id");
  assert.equal(filters.length, 2);
  for (const filter of filters) {
    assert.deepEqual(filter["#m"], ["match-id"]);
    assert.equal(
      Object.keys(filter).filter((key) => key.startsWith("#")).length,
      1,
    );
  }
  assert.deepEqual(filters[0].authors, ["host-public-key"]);
});

test("discovery filter searches current public application lobbies", () => {
  assert.deepEqual(lobbyDiscoveryFilters(5000), [{
    kinds: [LOBBY_KIND],
    "#t": [APP_TAG],
    since: -2500,
  }]);
});

test("discovery accepts only open compatible unexpired production lobbies", () => {
  const now = 5000;
  const host = "a".repeat(64);
  const match = "b".repeat(64);
  const relays = ["wss://one.example/", "wss://two.example/"];
  const content = {
    protocol: 1,
    family: "lobby",
    match_id: match,
    revision: 3,
    host_pubkey: host,
    config_digest: "config",
    compatibility_digest: "compatible",
    hello_frame: "hello",
    relays,
    status: "open",
    expires_at: 5200,
    open: true,
  };
  const event = {
    id: "c".repeat(64),
    pubkey: host,
    kind: LOBBY_KIND,
    created_at: 4900,
    tags: [["m", match], ["d", match], ["t", APP_TAG], ["v", "1"],
      ["expiration", "5200"]],
    content: JSON.stringify(content),
    sig: "d".repeat(128),
  } as NostrEvent;
  assert.deepEqual(parseLobbyAnnouncement(event, relays, "compatible", now), {
    eventId: event.id,
    hostPubkey: host,
    matchId: match,
    revision: 3,
    expiresAt: 5200,
    open: true,
  });
  assert.equal(parseLobbyAnnouncement(event, relays, "wrong", now), null);
  assert.equal(parseLobbyAnnouncement(event, [...relays].reverse(), "compatible", now), null);
  assert.equal(parseLobbyAnnouncement(event, relays, "compatible", 5300), null);
  assert.equal(parseLobbyAnnouncement({
    ...event,
    content: JSON.stringify({...content, status: "full", open: true}),
  } as NostrEvent, relays, "compatible", now), null);
});

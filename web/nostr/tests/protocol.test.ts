import assert from "node:assert/strict";
import test from "node:test";

import {EventFactory, EventStore} from "applesauce-core";
import {PrivateKeySigner} from "applesauce-signers";

import {
  APP_TAG,
  LOBBY_KIND,
  makeMatchReference,
  matchSubscriptionFilters,
  MATCH_KIND,
  parseMatchReference,
  validateIntent,
  validateRelays,
} from "../src/protocol.js";

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
  assert.throws(() => validateRelays(["wss://one.example"], false), /2-10/);
  assert.doesNotThrow(() => validateRelays(
    Array.from({length: 10}, (_, index) => `wss://relay-${index}.example`),
    false,
  ));
  assert.throws(() => validateRelays(
    Array.from({length: 11}, (_, index) => `wss://relay-${index}.example`),
    false,
  ), /2-10/);
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

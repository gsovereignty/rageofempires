import {EventFactory, EventStore} from "applesauce-core";
import type {NostrEvent} from "applesauce-core/helpers/event";
import {RelayPool} from "applesauce-relay";
import type {GroupReqMessage, PublishResponse, RelayStatus} from "applesauce-relay/types";
import {PrivateKeySigner} from "applesauce-signers";
import type {Subscription} from "rxjs";

import {
  APP_TAG,
  applicationTags,
  EventIntent,
  LaunchConfig,
  LOBBY_KIND,
  makeMatchReference,
  MATCH_KIND,
  MAX_BRIDGE_BYTES,
  MAX_CONTENT_BYTES,
  MAX_TAGS,
  MAX_TAG_PARTS,
  MAX_TAG_PART_BYTES,
  parseMatchReference,
  PROTOCOL_VERSION,
  randomMatchId,
  validateIntent,
  validateRelays,
} from "./protocol.js";

export type BridgeChannel = "event" | "status" | "publish";
export type BridgeEmitter = (channel: BridgeChannel, json: string) => void;

type RuntimeDiagnostics = {
  matchId: string;
  publicKey: string;
  hostPublicKey: string;
  matchReference: string;
  relays: string[];
  disabledRelays: string[];
  eoseRelays: string[];
  relayStatus: Record<string, RelayStatus>;
  recentPublications: Array<{
    eventId: string;
    results: Array<{relay: string; ok: boolean; message: string}>;
  }>;
  cachedEvents: number;
};

function boundedEventEnvelope(event: NostrEvent, relay: string): string {
  if (event.kind !== LOBBY_KIND && event.kind !== MATCH_KIND) {
    throw new Error("unsupported subscribed event kind");
  }
  if (event.content.length > MAX_CONTENT_BYTES || event.tags.length > MAX_TAGS) {
    throw new Error("subscribed event exceeds bridge bound");
  }
  for (const tag of event.tags) {
    if (tag.length > MAX_TAG_PARTS ||
        tag.some((part) => typeof part !== "string" || part.length > MAX_TAG_PART_BYTES)) {
      throw new Error("subscribed event tag exceeds bridge bound");
    }
  }
  const json = JSON.stringify({
    relay,
    event_id: event.id,
    pubkey: event.pubkey,
    kind: event.kind,
    created_at: event.created_at,
    tags: event.tags,
    content: event.content,
  });
  if (new TextEncoder().encode(json).byteLength > MAX_BRIDGE_BYTES) {
    throw new Error("subscribed event envelope exceeds bridge bound");
  }
  return json;
}

export class AoeNostrClient {
  private pool = new RelayPool();
  private store = new EventStore();
  private signer = new PrivateKeySigner();
  private subscriptions: Subscription[] = [];
  private signedEvents = new Map<string, NostrEvent>();
  private disabledRelays = new Set<string>();
  private eoseRelays = new Set<string>();
  private relayStatus = new Map<string, RelayStatus>();
  private recentPublications: RuntimeDiagnostics["recentPublications"] = [];
  private relays: string[] = [];
  private matchId = "";
  private publicKey = "";
  private hostPublicKey = "";
  private matchReference = "";
  private quorum = 2;
  private running = false;

  constructor(private readonly emit: BridgeEmitter) {}

  async initialize(input: LaunchConfig): Promise<void> {
    this.shutdown();
    this.pool = new RelayPool();
    this.store = new EventStore();
    this.signer = new PrivateKeySigner();
    this.running = true;
    this.publicKey = await this.signer.getPublicKey();
    if (input.role === "host") {
      this.relays = validateRelays(input.relays, input.one_relay_development);
      this.matchId = randomMatchId();
      this.hostPublicKey = this.publicKey;
    } else {
      if (!input.match_reference) throw new Error("join requires match reference");
      const reference = parseMatchReference(input.match_reference);
      if (reference.relays.length === 1 && !input.one_relay_development) {
        throw new Error("one-relay match requires development-mode opt-in");
      }
      this.relays = reference.relays;
      this.matchId = reference.matchId;
      this.hostPublicKey = reference.hostPubkey;
    }
    this.matchReference = makeMatchReference({
      hostPubkey: this.hostPublicKey,
      matchId: this.matchId,
      relays: this.relays,
    });
    this.quorum = this.relays.length === 1 ? 1 : 2;
    this.observeRelayStatus();
    this.openMatchSubscription();
    this.status("initialized", {
      role: input.role,
      pubkey: this.publicKey,
      host_pubkey: this.hostPublicKey,
      match_id: this.matchId,
      match_reference: this.matchReference,
      relays: this.relays,
      quorum: this.quorum,
    });
  }

  private status(type: string, fields: Record<string, unknown> = {}): void {
    this.emit("status", JSON.stringify({type, ...fields}));
  }

  private observeRelayStatus(): void {
    const subscription = this.pool.status$.subscribe({
      next: (statuses: Record<string, RelayStatus>) => {
        for (const [url, status] of Object.entries(statuses)) {
          this.relayStatus.set(url, status);
          this.status("relay", {
            relay: url,
            connected: status.connected,
            ready: status.ready,
            auth_required: status.authRequiredForRead || status.authRequiredForPublish,
          });
          if ((status.authRequiredForRead || status.authRequiredForPublish) &&
              !this.disabledRelays.has(url)) {
            this.disabledRelays.add(url);
            this.pool.remove(url, true);
            this.status("relay_disabled", {relay: url, reason: "authentication required"});
          }
        }
      },
      error: (error: unknown) => this.status("relay_status_error", {message: String(error)}),
    });
    this.subscriptions.push(subscription);
  }

  private openMatchSubscription(): void {
    const filters = [
      {
        kinds: [LOBBY_KIND],
        authors: [this.hostPublicKey],
        "#d": [this.matchId],
        "#m": [this.matchId],
        "#t": [APP_TAG],
        "#v": [String(PROTOCOL_VERSION)],
      },
      {
        kinds: [MATCH_KIND],
        "#m": [this.matchId],
        "#t": [APP_TAG],
        "#v": [String(PROTOCOL_VERSION)],
      },
    ];
    const subscription = this.pool.req(this.relays, filters, {
      waitForAuth: false,
      resubscribe: true,
      reconnect: true,
    }).subscribe({
      next: (message: GroupReqMessage) => this.receivePoolMessage(message),
      error: (error: unknown) => this.status("subscription_error", {message: String(error)}),
    });
    this.subscriptions.push(subscription);
  }

  private receivePoolMessage(message: GroupReqMessage): void {
    if (!this.running) return;
    if (message.type === "OPEN") {
      this.eoseRelays.delete(message.from);
      this.status("backfill_open", {relay: message.from});
      return;
    }
    if (message.type === "EOSE") {
      this.eoseRelays.add(message.from);
      this.status("eose", {relay: message.from});
      return;
    }
    if (message.type === "CLOSED") {
      this.status("subscription_closed", {relay: message.from, reason: message.reason});
      return;
    }
    if (message.type === "ERROR") {
      this.status("subscription_error", {relay: message.from, message: String(message.error)});
      return;
    }
    const event = message.event;
    if (!applicationTags(event.tags, this.matchId)) return;
    const existed = this.store.hasEvent(event.id);
    const accepted = this.store.add(event, message.from);
    if (!accepted) {
      this.status("event_rejected", {relay: message.from, event_id: event.id});
      return;
    }
    this.status("event_observed", {relay: message.from, event_id: event.id});
    if (!existed) this.emit("event", boundedEventEnvelope(event, message.from));
  }

  async publish(untrusted: EventIntent): Promise<void> {
    const intent = validateIntent(untrusted);
    try {
      const event = await EventFactory.fromKind(intent.kind)
        .content(intent.content)
        .modifyPublicTags((tags) => [...tags, ...intent.tags])
        .as(this.signer)
        .sign();
      if (intent.cache) this.signedEvents.set(event.id, event);
      const active = this.relays.filter((relay) => !this.disabledRelays.has(relay));
      const results = await this.pool.publish(active, event, {
        reconnect: true,
        retries: 3,
        timeout: 15000,
      });
      this.publishResult(intent.intent_id, event, results);
    } catch (error) {
      this.emit("publish", JSON.stringify({
        intent_id: intent.intent_id,
        ok: false,
        message: String(error),
        results: [],
      }));
    }
  }

  private publishResult(
    intentId: string,
    event: NostrEvent,
    results: PublishResponse[],
  ): void {
    const publication = {
      eventId: event.id,
      results: results.map((result) => ({
        relay: result.from,
        ok: result.ok,
        message: result.message ?? "",
      })),
    };
    this.recentPublications.push(publication);
    if (this.recentPublications.length > 32) this.recentPublications.shift();
    this.emit("publish", JSON.stringify({
      intent_id: intentId,
      event_id: event.id,
      ok: results.filter((result) => result.ok).length >= this.quorum,
      results: publication.results,
    }));
  }

  async republish(eventId: string): Promise<void> {
    const event = this.signedEvents.get(eventId);
    if (!event) {
      this.status("republish_unavailable", {event_id: eventId});
      return;
    }
    const active = this.relays.filter((relay) => !this.disabledRelays.has(relay));
    const results = await this.pool.publish(active, event, {
      reconnect: true,
      retries: 3,
      timeout: 15000,
    });
    this.publishResult(`republish:${eventId}`, event, results);
  }

  shutdown(): void {
    this.running = false;
    for (const subscription of this.subscriptions.splice(0)) subscription.unsubscribe();
    this.pool.close();
    this.store.dispose();
    this.signedEvents.clear();
    this.disabledRelays.clear();
    this.eoseRelays.clear();
    this.relayStatus.clear();
    this.recentPublications = [];
  }

  diagnostics(): RuntimeDiagnostics {
    return {
      matchId: this.matchId,
      publicKey: this.publicKey,
      hostPublicKey: this.hostPublicKey,
      matchReference: this.matchReference,
      relays: [...this.relays],
      disabledRelays: [...this.disabledRelays],
      eoseRelays: [...this.eoseRelays],
      relayStatus: Object.fromEntries(this.relayStatus),
      recentPublications: [...this.recentPublications],
      cachedEvents: this.signedEvents.size,
    };
  }
}

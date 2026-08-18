import {EventFactory, EventStore} from "applesauce-core";
import type {NostrEvent} from "applesauce-core/helpers/event";
import {RelayPool} from "applesauce-relay";
import type {GroupReqMessage, PublishResponse, RelayStatus} from "applesauce-relay/types";
import {PrivateKeySigner} from "applesauce-signers";
import type {Subscription} from "rxjs";

import {
  applicationTags,
  EventIntent,
  LaunchConfig,
  LOBBY_KIND,
  makeMatchReference,
  MATCH_KIND,
  matchSubscriptionFilters,
  MAX_BRIDGE_BYTES,
  MAX_CONTENT_BYTES,
  MAX_TAGS,
  MAX_TAG_PARTS,
  MAX_TAG_PART_BYTES,
  parseMatchReference,
  randomMatchId,
  sameRelayPool,
  validateRelay,
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
  relayPoolDigest: string;
  disabledRelays: string[];
  eoseRelays: string[];
  relayStatus: Record<string, RelayStatus>;
  recentPublications: Array<{
    intentId: string;
    eventId: string;
    results: Array<{relay: string; ok: boolean; message: string}>;
  }>;
  recentSubscriptionMessages: Array<{
    type: string;
    relay: string;
    detail?: string;
  }>;
  cachedEvents: number;
};

export async function relayPoolDigest(relays: string[]): Promise<string> {
  const bytes = new TextEncoder().encode(relays.join("\n"));
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest), (byte) =>
    byte.toString(16).padStart(2, "0")
  ).join("");
}

export function readyPublishRelays(
  relays: string[],
  disabledRelays: ReadonlySet<string>,
  relayStatus: ReadonlyMap<string, Pick<RelayStatus, "connected" | "ready">>,
): string[] {
  return relays.filter((relay) => {
    const status = relayStatus.get(relay);
    return !disabledRelays.has(relay) && status?.connected === true &&
      status.ready === true;
  });
}

export function relayStatusFingerprint(
  status: Pick<RelayStatus, "connected" | "ready" |
    "authRequiredForRead" | "authRequiredForPublish">,
): string {
  return [
    status.connected,
    status.ready,
    status.authRequiredForRead,
    status.authRequiredForPublish,
  ].join(":");
}

export async function collectPublishQuorum(
  relays: string[],
  quorum: number,
  publishRelay: (relay: string) => Promise<PublishResponse[]>,
): Promise<PublishResponse[]> {
  if (quorum < 1 || relays.length < quorum) return [];
  return new Promise((resolve) => {
    const results: PublishResponse[] = [];
    let settled = 0;
    let complete = false;
    const finish = (responses: PublishResponse[]) => {
      if (complete) return;
      results.push(...responses);
      settled += 1;
      const accepted = results.filter((result) => result.ok).length;
      if (accepted >= quorum || settled === relays.length) {
        complete = true;
        resolve(results);
      }
    };
    for (const relay of relays) {
      void publishRelay(relay).then(finish).catch((error: unknown) => finish([{
        from: relay,
        ok: false,
        message: String(error),
      }]));
    }
  });
}

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
  private matchSubscriptions = new Map<string, Subscription>();
  private signedEvents = new Map<string, NostrEvent>();
  private disabledRelays = new Set<string>();
  private eoseRelays = new Set<string>();
  private relayStatus = new Map<string, RelayStatus>();
  private emittedRelayStatus = new Map<string, string>();
  private recentPublications: RuntimeDiagnostics["recentPublications"] = [];
  private recentSubscriptionMessages: RuntimeDiagnostics["recentSubscriptionMessages"] = [];
  private relays: string[] = [];
  private relayDigest = "";
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
    const configuredRelays = validateRelays(
      input.relays, input.one_relay_development
    );
    if (input.role === "host") {
      this.relays = configuredRelays;
      this.matchId = randomMatchId();
      this.hostPublicKey = this.publicKey;
    } else {
      if (!input.match_reference) throw new Error("join requires match reference");
      const reference = parseMatchReference(input.match_reference);
      if (reference.relays.length === 1 && !input.one_relay_development) {
        throw new Error("one-relay match requires development-mode opt-in");
      }
      if (!sameRelayPool(reference.relays, configuredRelays)) {
        throw new Error("match reference relay pool differs from production");
      }
      this.relays = configuredRelays;
      this.matchId = reference.matchId;
      this.hostPublicKey = reference.hostPubkey;
    }
    this.relayDigest = await relayPoolDigest(this.relays);
    this.matchReference = makeMatchReference({
      hostPubkey: this.hostPublicKey,
      matchId: this.matchId,
      relays: this.relays,
    });
    this.quorum = this.relays.length === 1 ? 1 : 2;
    this.observeRelayStatus();
    for (const relay of this.relays) this.openRelaySubscription(relay);
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
          const fingerprint = relayStatusFingerprint(status);
          if (this.emittedRelayStatus.get(url) === fingerprint) continue;
          this.emittedRelayStatus.set(url, fingerprint);
          this.status("relay", {
            relay: url,
            connected: status.connected,
            ready: status.ready,
            auth_required: status.authRequiredForRead || status.authRequiredForPublish,
          });
          if ((status.authRequiredForRead || status.authRequiredForPublish) &&
              !this.disabledRelays.has(url)) {
            this.setRelayEnabled(url, false, "authentication required");
          }
        }
      },
      error: (error: unknown) => this.status("relay_status_error", {message: String(error)}),
    });
    this.subscriptions.push(subscription);
  }

  private openRelaySubscription(relay: string): void {
    this.matchSubscriptions.get(relay)?.unsubscribe();
    const filters = matchSubscriptionFilters(this.hostPublicKey, this.matchId);
    const subscription = this.pool.req([relay], filters, {
      waitForAuth: false,
      resubscribe: true,
      reconnect: true,
    }).subscribe({
      next: (message: GroupReqMessage) => this.receivePoolMessage(message),
      error: (error: unknown) => this.status("subscription_error", {message: String(error)}),
    });
    this.matchSubscriptions.set(relay, subscription);
  }

  setRelayEnabled(
    untrustedRelay: string,
    enabled: boolean,
    reason = "user control",
  ): void {
    const relay = validateRelay(untrustedRelay);
    if (!this.running || !this.relays.includes(relay)) {
      throw new Error("relay is not part of active match");
    }
    if (enabled) {
      if (!this.disabledRelays.delete(relay)) return;
      this.eoseRelays.delete(relay);
      this.relayStatus.delete(relay);
      this.status("relay_enabled", {relay, reason});
      this.openRelaySubscription(relay);
      return;
    }
    if (this.disabledRelays.has(relay)) return;
    this.disabledRelays.add(relay);
    this.eoseRelays.delete(relay);
    this.matchSubscriptions.get(relay)?.unsubscribe();
    this.matchSubscriptions.delete(relay);
    this.pool.remove(relay, true);
    this.relayStatus.delete(relay);
    this.status("relay_disabled", {relay, reason});
  }

  refreshSubscriptions(): void {
    if (!this.running) return;
    for (const relay of this.relays) {
      if (this.disabledRelays.has(relay)) continue;
      this.eoseRelays.delete(relay);
      this.status("backfill_open", {relay});
      this.openRelaySubscription(relay);
    }
  }

  private receivePoolMessage(message: GroupReqMessage): void {
    if (!this.running) return;
    const detail = message.type === "CLOSED" ? message.reason :
      message.type === "ERROR" ? String(message.error) : undefined;
    this.recentSubscriptionMessages.push({
      type: message.type,
      relay: message.from,
      ...(detail ? {detail} : {}),
    });
    if (this.recentSubscriptionMessages.length > 64) {
      this.recentSubscriptionMessages.shift();
    }
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
      const active = readyPublishRelays(
        this.relays, this.disabledRelays, this.relayStatus,
      );
      const results = await collectPublishQuorum(
        active,
        this.quorum,
        (relay) => this.pool.publish([relay], event, {
          reconnect: true,
          retries: false,
          timeout: 15000,
        }),
      );
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
      intentId,
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
    const active = readyPublishRelays(
      this.relays, this.disabledRelays, this.relayStatus,
    );
    const results = await collectPublishQuorum(
      active,
      this.quorum,
      (relay) => this.pool.publish([relay], event, {
        reconnect: true,
        retries: false,
        timeout: 15000,
      }),
    );
    this.publishResult(`republish:${eventId}`, event, results);
  }

  shutdown(): void {
    this.running = false;
    for (const subscription of this.matchSubscriptions.values()) {
      subscription.unsubscribe();
    }
    this.matchSubscriptions.clear();
    for (const subscription of this.subscriptions.splice(0)) subscription.unsubscribe();
    this.pool.close();
    this.store.dispose();
    this.signedEvents.clear();
    this.disabledRelays.clear();
    this.eoseRelays.clear();
    this.relayStatus.clear();
    this.emittedRelayStatus.clear();
    this.recentPublications = [];
    this.recentSubscriptionMessages = [];
  }

  diagnostics(): RuntimeDiagnostics {
    return {
      matchId: this.matchId,
      publicKey: this.publicKey,
      hostPublicKey: this.hostPublicKey,
      matchReference: this.matchReference,
      relays: [...this.relays],
      relayPoolDigest: this.relayDigest,
      disabledRelays: [...this.disabledRelays],
      eoseRelays: [...this.eoseRelays],
      relayStatus: Object.fromEntries(this.relayStatus),
      recentPublications: [...this.recentPublications],
      recentSubscriptionMessages: [...this.recentSubscriptionMessages],
      cachedEvents: this.signedEvents.size,
    };
  }
}

export const APP_TAG = "aoe-reconstruction";
export const PROTOCOL_VERSION = 1;
export const LOBBY_KIND = 30078;
export const MATCH_KIND = 78;

export const MAX_BRIDGE_BYTES = 1024 * 1024;
export const MAX_CONTENT_BYTES = 768 * 1024;
export const MAX_TAGS = 64;
export const MAX_TAG_PARTS = 8;
export const MAX_TAG_PART_BYTES = 4096;
export const MAX_RELAYS = 4;
export const MAX_RELAY_BYTES = 512;

export type LaunchConfig = {
  role: "host" | "join";
  relays: string[];
  one_relay_development: boolean;
  match_reference?: string;
};

export type EventIntent = {
  intent_id: string;
  kind: number;
  tags: string[][];
  content: string;
  cache?: boolean;
};

export type MatchReference = {
  hostPubkey: string;
  matchId: string;
  relays: string[];
};

const encoder = new TextEncoder();

export function byteLength(value: string): number {
  return encoder.encode(value).byteLength;
}

export function validHex64(value: string): boolean {
  return /^[0-9a-f]{64}$/.test(value);
}

export function validateRelay(url: string): string {
  if (byteLength(url) > MAX_RELAY_BYTES) throw new Error("relay URL too long");
  const parsed = new URL(url);
  if (parsed.protocol !== "wss:") throw new Error("relay must use wss://");
  parsed.hash = "";
  parsed.search = "";
  return parsed.toString();
}

export function validateRelays(
  relays: string[],
  oneRelayDevelopment: boolean,
): string[] {
  if (!Array.isArray(relays) || relays.length > MAX_RELAYS ||
      relays.length < (oneRelayDevelopment ? 1 : 2)) {
    throw new Error("match requires 2-4 relays (or one development relay)");
  }
  const normalized = relays.map(validateRelay);
  if (new Set(normalized).size !== normalized.length) {
    throw new Error("relay list contains duplicates");
  }
  return normalized;
}

function base64UrlEncode(value: string): string {
  const bytes = encoder.encode(value);
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_")
    .replace(/=+$/u, "");
}

function base64UrlDecode(value: string): string {
  const padded = value.replaceAll("-", "+").replaceAll("_", "/") +
    "=".repeat((4 - value.length % 4) % 4);
  const binary = atob(padded);
  const bytes = Uint8Array.from(binary, (character) => character.charCodeAt(0));
  return new TextDecoder("utf-8", {fatal: true}).decode(bytes);
}

export function makeMatchReference(reference: MatchReference): string {
  if (!validHex64(reference.hostPubkey) || !validHex64(reference.matchId)) {
    throw new Error("invalid match identity");
  }
  const relays = validateRelays(reference.relays, reference.relays.length === 1);
  return `aoe-nostr:1:${reference.hostPubkey}:${reference.matchId}:` +
    base64UrlEncode(JSON.stringify(relays));
}

export function parseMatchReference(value: string): MatchReference {
  if (byteLength(value) > 4096) throw new Error("match reference too long");
  const parts = value.trim().split(":");
  if (parts.length !== 5 || parts[0] !== "aoe-nostr" || parts[1] !== "1") {
    throw new Error("invalid public match reference");
  }
  const hostPubkey = parts[2];
  const matchId = parts[3];
  if (!validHex64(hostPubkey) || !validHex64(matchId)) {
    throw new Error("invalid public match reference identity");
  }
  const decoded: unknown = JSON.parse(base64UrlDecode(parts[4]));
  if (!Array.isArray(decoded) || !decoded.every((relay) => typeof relay === "string")) {
    throw new Error("invalid public match reference relays");
  }
  return {
    hostPubkey,
    matchId,
    relays: validateRelays(decoded, decoded.length === 1),
  };
}

export function randomMatchId(): string {
  const bytes = crypto.getRandomValues(new Uint8Array(32));
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export function validateIntent(intent: EventIntent): EventIntent {
  if (typeof intent.intent_id !== "string" || intent.intent_id.length < 1 ||
      intent.intent_id.length > 128) throw new Error("invalid intent ID");
  if (intent.kind !== LOBBY_KIND && intent.kind !== MATCH_KIND) {
    throw new Error("unsupported application event kind");
  }
  if (typeof intent.content !== "string" ||
      byteLength(intent.content) > MAX_CONTENT_BYTES) {
    throw new Error("event content exceeds bridge bound");
  }
  if (!Array.isArray(intent.tags) || intent.tags.length > MAX_TAGS) {
    throw new Error("event tag count exceeds bridge bound");
  }
  for (const tag of intent.tags) {
    if (!Array.isArray(tag) || tag.length < 2 || tag.length > MAX_TAG_PARTS) {
      throw new Error("invalid event tag");
    }
    for (const part of tag) {
      if (typeof part !== "string" || byteLength(part) > MAX_TAG_PART_BYTES) {
        throw new Error("event tag element exceeds bridge bound");
      }
    }
  }
  return intent;
}

export function applicationTags(tags: string[][], matchId: string): boolean {
  return tags.some((tag) => tag[0] === "t" && tag[1] === APP_TAG) &&
    tags.some((tag) => tag[0] === "v" && tag[1] === String(PROTOCOL_VERSION)) &&
    tags.some((tag) => tag[0] === "m" && tag[1] === matchId);
}

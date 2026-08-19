import {AoeNostrClient, BridgeChannel} from "./runtime.js";
import {sameRelayPool} from "./protocol.js";
import type {EventIntent, LaunchConfig} from "./protocol.js";

type EmscriptenModule = {
  HEAPU8: Uint8Array;
  _malloc(size: number): number;
  _free(pointer: number): void;
  _aoe_nostr_enqueue_event(pointer: number, size: number): void;
  _aoe_nostr_enqueue_status(pointer: number, size: number): void;
  _aoe_nostr_publish_result(pointer: number, size: number): void;
  browserNostrDiagnostics?: () => unknown;
  browserNostrGameDiagnostics?: unknown;
  browserShutdownDiagnostics?: unknown;
  browserNostrShutdownDiagnostics?: unknown;
  canonicalNostrRelays?: string[];
  canonicalNostrRelayDigest?: string;
};

type AoeNostrFacade = {
  initialize(config: LaunchConfig): Promise<void>;
  publish(intent: EventIntent): void;
  subscribe(): void;
  republish(eventId: string): void;
  setRelayEnabled(relay: string, enabled: boolean): void;
  refreshSubscriptions(): void;
  selectLobby(matchReference: string): void;
  shutdown(): void;
  diagnostics(): unknown;
};

declare global {
  var Module: EmscriptenModule | undefined;
  var AoeNostrRuntime: AoeNostrFacade;
}

const encoder = new TextEncoder();

export function makeShutdownDiagnostics(
  context: unknown,
  clientDiagnostics: unknown,
  hadClient: boolean,
  wallTimeMs: number,
): Record<string, unknown> {
  return {
    context: context ?? null,
    client: clientDiagnostics ?? null,
    hadClient,
    wallTimeMs,
  };
}

function emit(channel: BridgeChannel, json: string): void {
  const module = globalThis.Module;
  if (!module) throw new Error("Emscripten module is not initialized");
  const bytes = encoder.encode(json);
  const pointer = module._malloc(bytes.length || 1);
  try {
    module.HEAPU8.set(bytes, pointer);
    if (channel === "event") module._aoe_nostr_enqueue_event(pointer, bytes.length);
    else if (channel === "status") module._aoe_nostr_enqueue_status(pointer, bytes.length);
    else module._aoe_nostr_publish_result(pointer, bytes.length);
  } finally {
    module._free(pointer);
  }
}

let client: AoeNostrClient | undefined;

const facade = {
  async initialize(config: LaunchConfig): Promise<void> {
    client?.shutdown();
    client = new AoeNostrClient(emit);
    if (globalThis.Module) {
      globalThis.Module.browserNostrShutdownDiagnostics = null;
      globalThis.Module.browserNostrDiagnostics = () => ({
        ...(client?.diagnostics() as Record<string, unknown> ?? {}),
        shutdown: globalThis.Module?.browserNostrShutdownDiagnostics ?? null,
        game: globalThis.Module?.browserNostrGameDiagnostics ?? null,
      });
    }
    try {
      const canonicalRelays = globalThis.Module?.canonicalNostrRelays;
      if (!canonicalRelays || !sameRelayPool(config.relays, canonicalRelays)) {
        throw new Error("runtime relay pool differs from packaged production");
      }
      await client.initialize(config);
      const diagnostics = client.diagnostics();
      if (diagnostics.relayPoolDigest !==
          globalThis.Module?.canonicalNostrRelayDigest) {
        throw new Error("runtime relay digest differs from packaged production");
      }
    } catch (error) {
      emit("status", JSON.stringify({type: "fatal", message: String(error)}));
    }
  },
  publish(intent: EventIntent): void {
    void client?.publish(intent);
  },
  subscribe(): void {
    // Subscription is established atomically by initialize(). Kept as narrow
    // bridge compatibility surface for future filter changes.
  },
  republish(eventId: string): void {
    void client?.republish(eventId);
  },
  setRelayEnabled(relay: string, enabled: boolean): void {
    client?.setRelayEnabled(relay, enabled);
  },
  refreshSubscriptions(): void {
    client?.refreshSubscriptions();
  },
  selectLobby(matchReference: string): void {
    client?.selectLobby(matchReference);
  },
  shutdown(): void {
    const hadClient = client !== undefined;
    const finalDiagnostics = client?.diagnostics() ?? null;
    if (globalThis.Module) {
      globalThis.Module.browserNostrShutdownDiagnostics =
        makeShutdownDiagnostics(
          globalThis.Module.browserShutdownDiagnostics,
          finalDiagnostics,
          hadClient,
          Date.now(),
        );
    }
    client?.shutdown();
    client = undefined;
  },
  diagnostics(): unknown {
    return client?.diagnostics() ?? null;
  },
};

globalThis.AoeNostrRuntime = facade;

export {facade as AoeNostrRuntime};

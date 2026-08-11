import {AoeNostrClient, BridgeChannel} from "./runtime.js";
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
};

type AoeNostrFacade = {
  initialize(config: LaunchConfig): Promise<void>;
  publish(intent: EventIntent): void;
  subscribe(): void;
  republish(eventId: string): void;
  shutdown(): void;
  diagnostics(): unknown;
};

declare global {
  var Module: EmscriptenModule | undefined;
  var AoeNostrRuntime: AoeNostrFacade;
}

const encoder = new TextEncoder();

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
      globalThis.Module.browserNostrDiagnostics = () => ({
        ...(client?.diagnostics() as Record<string, unknown> ?? {}),
        game: globalThis.Module?.browserNostrGameDiagnostics ?? null,
      });
    }
    try {
      await client.initialize(config);
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
  shutdown(): void {
    client?.shutdown();
    client = undefined;
  },
  diagnostics(): unknown {
    return client?.diagnostics() ?? null;
  },
};

globalThis.AoeNostrRuntime = facade;

export {facade as AoeNostrRuntime};

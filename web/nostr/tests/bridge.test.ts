import assert from "node:assert/strict";
import test from "node:test";

import {makeShutdownDiagnostics} from "../src/bridge.js";

test("shutdown diagnostics preserve app and client provenance", () => {
  assert.deepEqual(
    makeShutdownDiagnostics(
      {reason: "sdl-quit-event", monotonicMs: 12},
      {publicKey: "a".repeat(64)},
      true,
      34,
    ),
    {
      context: {reason: "sdl-quit-event", monotonicMs: 12},
      client: {publicKey: "a".repeat(64)},
      hadClient: true,
      wallTimeMs: 34,
    },
  );
});

test("shutdown diagnostics represent missing context without omission", () => {
  assert.deepEqual(makeShutdownDiagnostics(undefined, undefined, false, 5), {
    context: null,
    client: null,
    hadClient: false,
    wallTimeMs: 5,
  });
});

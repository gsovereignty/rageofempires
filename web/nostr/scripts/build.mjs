import {build} from "esbuild";
import {resolve} from "node:path";

const outfile = process.env.AOE_NOSTR_BUNDLE ||
  resolve(process.cwd(), "../../build-web/nostr/aoe_nostr.js");

await build({
  entryPoints: [resolve(process.cwd(), "src/bridge.ts")],
  bundle: true,
  format: "iife",
  globalName: "AoeNostrBundle",
  outfile,
});

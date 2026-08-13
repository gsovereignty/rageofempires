---
name: audit-sprite-screenshots
description: Capture, isolate, batch-review, and baseline every reachable production sprite rendered by this project's packaged scenarios. Use for sprite screenshots, rendered-sprite corpora, terrain-over-sprite overlap detection, sprite isolation, transparent RGBA comparison, human sprite-review pages, or sprite decision baselines under reconstruction/resources/*.scenario. Use audit-visual-fidelity instead for whole-screen gameplay, animation, effects, terrain, HUD, menus, and terminal-flow audits.
---

# Audit sprite screenshots

## Ownership boundary

Own isolated rendered-sprite capture, corpus completeness, transparent RGBA
comparison, terrain overlap, and decision baselines. Do not claim whole-screen
animation/UI fidelity, live multiplayer behavior, synchronization, or pointer
mapping. Route those to `audit-visual-fidelity`,
`audit-browser-multiplayer-gameplay`, `diagnose-gameplay-sync`, or
`audit-pointer-coordinates`.

Read repository `AGENTS.md` files first. Treat `resources/*.scenario` as fixture source of truth. Consult `decompiled/` and original assets only as read-only fidelity evidence.

Read requested scenarios and coverage targets from invocation. Seek exhaustive rendered-sprite coverage when caller gives no narrower scope. Do not broaden into whole-screen visual or UI auditing; use `audit-visual-fidelity` for that work.

## Enforce runtime safety

- Before capture, prove no game process is active and descriptor use is below half soft limit.
- Run one background game process per scenario. Never allow more than four game instances across entire agent tree.
- Use two scenario agents per wave by default; increase to at most four only after previous wave finishes without descriptor growth, unreaped children, zombies, or `Too many open files` errors.
- Give each scenario agent exclusive ownership of `artifacts/scenario-audits/<scenario-stem>/`. Tell it other agents share repository, it must not modify or revert shared files, edit shared audit documents, or create commits.
- Keep one long-lived process per scenario. Close images, logs, pipes, automation responses, command sessions, and process handles promptly.
- Between waves, terminate and reap owned children, prove descriptor count returned to baseline, and inspect process table. On `EMFILE`, `ENFILE`, or `Too many open files`, launch nothing until recovery; mark affected scenarios `blocked` and retry with fresh agents.

## Capture sprite cases

- Run `python3 tools/capture_visual_overlap.py <executable> <scenario> --capture-dir <new-dir> --output-dir <review-dir> --tick <tick>`. It selects dummy video/audio, software rendering, direct scenario loading, compact-map support, one-process timeout/reaping, manifest validation, and batch review. Exit `1` means review candidates exist, not capture failure.
- For direct integration, set `AOE_OVERLAP_CAPTURE_DIR`; optionally set `AOE_OVERLAP_CAPTURE_TICK` and `AOE_OVERLAP_CAPTURE_EXIT`. Capture actual and terrain-only images from one render state, replay selected legacy textures into transparent RGBA composites, and record placement plus schema-version-1 metadata without advancing simulation.
- Preserve palette, frame, facing, flip, shadows, multipart composition, zoom, clipping, transparent holes, scale, anchor, layering, and occlusion.
- Use `tools/visual_overlap_audit.py` only for manually supplied actual, terrain-only, expected RGBA sprite, and placement inputs. Treat exit `0` as clean, `1` as overlap candidates, and `2` as invalid input.
- Treat production sprite/frame, anchor, palette, and aligned terrain-only generation as pre-approved when derived deterministically from renderer state. Do not stop for per-asset approval.
- Reject black warm-up frames, frontend captures, wrong-window captures, uncorrelated screenshots, procedural rendering, placeholders, synthetic assets, debug assets, missing assets, and fallback rendering.

## Build complete corpus

- Add every rendered sprite case across requested scenarios, civilizations, ages, facings, animation states, damage and construction stages, terrains, elevations, resolutions, and playthrough matrix cells to one schema-version-1 JSON manifest.
- Use stable unique case IDs. Record entity, sprite/frame, scenario, tick, camera, terrain, civilization, age, ownership, state, facing, and resolution.
- Capture loaded legacy RGBA units, buildings, and resources automatically.
- Mark procedural rendering, unsupported isolation, rubble/death effects, projectile/impact instances, and missing or uncorrelated layer inputs `blocked`. Never silently exclude them from manifest or coverage totals.

## Batch review and baseline

- Run `python3 tools/batch_visual_overlap_audit.py <manifest.json> --output-dir artifacts/visual-overlap-review` only after whole requested corpus is captured. Exit `1` means review candidates exist, not infrastructure failure.
- Deliver one self-contained `review.html` for human review. Include clean and flagged cases; never interrupt run for individual candidate review. Preserve combined `report.json`, red-contour annotations, actual views, terrain-only views, expected sprites, and downloadable human decisions JSON.
- Accept human decisions only as `bug`, `intentional`, or `uncertain`.
- Create or update baseline with `python3 tools/visual_overlap_decisions.py <report.json> --decisions <decisions.json> --output <baseline.json>`.
- Compare later runs with `python3 tools/visual_overlap_decisions.py <report.json> --baseline <baseline.json> --output <comparison.json>` and surface new, changed, missing, and unresolved cases.
- Convert confirmed bugs into regression cases. Retain intentional overlaps in explicit baseline. Keep uncertain cases open without counting them as bugs or passes.

## Confirm and report

Report only reproducible mismatches supported by scenario and tick, expected versus actual rendering, exact reproduction actions, screenshot evidence, relevant original or decompiled reference, and duplicate-group identifier when applicable. Do not count failed launches, infrastructure failures, hypotheses, incomplete checks, expected design differences, uncertain human decisions, or duplicates as bugs or passes.

Track each scenario as `pending`, `running`, `passed`, `bugs-found`, or `blocked`. Retry blocked scenarios with fresh agents. Never treat blocked as complete.

Write one concise report under `docs/audits/` containing scope/build, scenario ledger, deduplicated bugs, reproduction steps, evidence paths, sprite coverage matrix, manifest totals, human-decision totals, remaining gaps, and exact status/bug totals.

Run `make` from reconstruction root and require success. Preserve unrelated changes, stage only audit-related tracked files, create focused commit, then report totals, build result, report path, and commit hash.

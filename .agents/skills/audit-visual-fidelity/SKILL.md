---
name: audit-visual-fidelity
description: Exhaustively audit this project's packaged scenarios through deterministic real-gameplay screen captures, predicting and comparing every reachable sprite, animation, effect, terrain element, HUD component, minimap item, menu, and terminal screen against original-game evidence. Use for production visual-fidelity, rendering, display, sprite, animation, or UI audits under reconstruction/resources/*.scenario.
---

# Audit visual fidelity

Read repository `AGENTS.md` files first. Treat `resources/*.scenario` as fixture source of truth. Consult `decompiled/` and original assets only as read-only fidelity evidence.

## Enforce runtime cap

- Assign exactly one fresh subagent exclusive ownership of each scenario.
- Run waves containing at most four scenario agents.
- Never permit more than four game instances across entire agent tree. Count foreground, background, headless, dummy-video, retries, repairs, orphaned, zombie, coordinator-owned, and subagent-owned instances.
- Keep coordinator game-free.
- Wait for every current-wave agent to return and terminate its process before starting another wave.
- Inspect process table between waves. Stop spawning until every stale instance is cleaned or reaped.

Tell every scenario agent it is not alone, must not modify or revert shared files, and must not edit shared audit documents or create commits. Give it exclusive ownership of:

`artifacts/scenario-audits/<scenario-stem>/`

## Drive deterministic gameplay

Run fixture as real gameplay in background/headless mode unless real UI input is essential. Use unique automation directories, PIDs, and window identity. Never select windows through ambiguous shared process names.

Record deterministic launch configuration, camera, actions, selection, tick schedule, and outcome. Exercise fixture purpose through completion or terminal result. Add deliberate directions, selections, damage states, and UI states needed to expose fixture content.

Capture screen every `X` ticks. Choose and record smallest practical `X` that exposes animation-frame progression without useless duplicate frames. Also capture immediately before and after state transitions, including:

- initial loaded gameplay;
- entity selection and HUD;
- issued action;
- animation midpoint and frame progression;
- damage, construction, work, death, or destruction transitions;
- completed action or terminal screen.

Reject black warm-up frames, frontend captures mistaken for gameplay, wrong-window captures, and uncorrelated screenshots.

## Predict before comparison

For each captured tick, derive expected presentation before judging image. Use scenario state, simulation observation, original assets and metadata, relevant decompiled behavior, and documented original-game presentation.

Record expected and actual:

- visible entities, position, ownership, civilization, and age variant;
- sprite identity, palette, transparency, scale, anchor, facing, shadow, layering, and occlusion;
- animation state, frame order, timing, looping, projectile, hit, work, gather, carry, repair, construction, death, decay, and destruction;
- terrain blending, elevation, water, footprints, fire, smoke, rubble, and effects;
- HUD portrait, icons, text, values, commands, buttons, panels, selection, minimap, menus, and statistics;
- viewport-edge behavior and supported resolutions.

Require production assets. Confirm any procedural, placeholder, synthetic, debug, missing-asset, or fallback rendering as a bug when reproduced. Never accept fallback presentation as correct production rendering.

## Confirm findings

Report only reproducible mismatches supported by:

- affected scenario and tick;
- expected versus actual presentation;
- exact reproduction actions;
- screenshot or frame-sequence evidence;
- relevant original or decompiled reference;
- duplicate-group identifier when same root defect appears elsewhere.

Seek exhaustive coverage and at least 100 distinct confirmed bugs, but never invent, inflate, split duplicates, or misclassify findings to meet quota. If fewer exist, report evidence-backed total and remaining coverage gaps.

Do not count failed launches, infrastructure failures, hypotheses, incomplete checks, expected design differences, or duplicate manifestations as bugs or passes.

## Improve this skill

Treat each completed wave as feedback on this workflow. Ask scenario agents to include concise process lessons: missed states, weak evidence, capture failures, useful tick intervals, reliable launch/cleanup methods, better original-reference sources, and repeated ambiguity in instructions.

After each wave, let coordinator review lessons and update this skill when change is reusable, evidence-backed, and likely to improve later audits. Never let scenario agents edit skill concurrently. Reject one-off workarounds, unverified guesses, fixture-specific trivia, duplicate guidance, and changes that weaken deterministic evidence, production-fidelity rules, or four-instance cap.

Keep improvements concise and imperative. Prefer tightening existing instructions over adding prose. Add a script or reference only when repeated work proves it useful. Validate skill after every edit, inspect diff, and preserve unrelated changes. Apply accepted improvements before launching next wave when safe; otherwise queue them for coordinator after active processes end. Record material skill changes in final audit handoff and include them in focused audit-related commit.

## Track and consolidate

Maintain scenario status as `pending`, `running`, `passed`, `bugs-found`, or `blocked`. Retry blocked scenarios with fresh agents. Never treat blocked as complete.

Require each agent to return:

- scenario and exact launch configuration;
- capture interval, actions, and ticks;
- outcome and prediction-versus-actual ledger;
- confirmed bugs and evidence paths;
- sprites, animations, and UI states covered;
- untested states and blockers;
- process termination confirmation.

After all scenarios complete, write one concise report under `docs/audits/` containing scope/build, completion ledger, deduplicated bugs, affected fixtures, reproduction steps, evidence paths, sprite/animation/UI coverage matrix, capture intervals and frame totals, remaining gaps, and exact status/bug totals. Reference fixtures rather than repeating their contents.

Run `make` from reconstruction root and require success. Preserve unrelated changes, stage only audit-related tracked files, create focused commit, then report totals, build result, report path, and commit hash.

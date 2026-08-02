---
name: audit-visual-fidelity
description: Exhaustively audit this project's packaged scenarios through deterministic real-gameplay screen captures, predicting and comparing every reachable sprite, animation, effect, terrain element, HUD component, minimap item, menu, and terminal screen against original-game evidence. Use for production visual-fidelity, rendering, display, sprite, animation, or UI audits under reconstruction/resources/*.scenario. Also supports an explicitly requested `playthrough` option for foreground, real-cursor, end-to-end coverage of every supported map-size and game-type combination.
---

# Audit visual fidelity

Read repository `AGENTS.md` files first. Treat `resources/*.scenario` as fixture source of truth. Consult `decompiled/` and original assets only as read-only fidelity evidence.

Read audit targets from skill invocation, including optional minimum confirmed-bug target. If caller gives no target, seek exhaustive coverage without imposing a numeric quota.

Use normal scenario-audit mode by default. Activate `playthrough` mode only when the current user request explicitly says `playthrough` and confirms the computer can remain unused for the run. Never infer this option from requests for exhaustive, interactive, end-to-end, or gameplay coverage. If availability is not explicit, explain that this mode takes foreground mouse and keyboard control and ask before starting.

## Run foreground playthroughs

- Give one fresh playthrough agent exclusive control. Run exactly one game instance and no other gameplay agents. Tell the user immediately before taking foreground control.
- Discover supported map sizes and game types from the shipped game UI and repository evidence. Record the full Cartesian coverage matrix before launching the first match; never silently omit, merge, or substitute a combination.
- For every map-size and game-type combination, start at the packaged frontend and use visible cursor movement, clicks, keyboard input, and rendered UI only. Do not use semantic gameplay commands, direct scenario loading, tick advancement, state injection, debug shortcuts, or automation APIs to drive or bypass interaction. Read-only state observation may correlate evidence but must not control play.
- Capture the foreground window after every cursor action and UI transition, at regular gameplay intervals, immediately around every meaningful state change, and through the complete terminal flow. Include frontend, setup choices, loading, opening state, selection, commands, economy, exploration, combat, damage, death, construction, age progression, objectives, pause/options, victory or defeat, statistics tabs, and return navigation when reachable.
- Play each match through an actual terminal result using ordinary player-visible controls. Do not count resignation, forced termination, debug victory, timeout, or direct state mutation as completion unless the selected game type defines that action as its normal objective.
- Keep screenshots correlated with cursor coordinates, window identity, wall-clock time, observed game time, chosen settings, action ledger, and result. Reject evidence after focus loss, user interference, ambiguous window selection, hidden overlays, or cursor-coordinate drift; restore focus and repeat the affected interaction.
- Pause between matches when foreground ownership, input safety, file-descriptor budget, or evidence correlation is uncertain. Never continue while the user is using the computer.
- Report every matrix cell as `passed`, `bugs-found`, or `blocked`, with duration, result, interaction coverage, evidence paths, and gaps. Deduplicate findings across combinations while preserving every affected cell.

## Enforce runtime cap

- In normal scenario-audit mode, assign exactly one fresh subagent exclusive ownership of each scenario.
- Run waves containing at most four scenario agents.
- Never permit more than four game instances across entire agent tree. Count foreground, background, headless, dummy-video, retries, repairs, orphaned, zombie, coordinator-owned, and subagent-owned instances.
- Keep coordinator game-free.
- Wait for every current-wave agent to return and terminate its process before starting another wave.
- Inspect process table between waves. Stop spawning until every stale instance is cleaned or reaped.

## Enforce file-descriptor cap

- Keep one long-lived game process per scenario for the entire capture run. Advance and capture that process in place. Never relaunch the game once per tick, frame, selection, or screenshot.
- Avoid persistent shell pipelines, background capture loops, and unused interactive exec sessions. Close screenshot files, logs, pipes, automation responses, and process handles promptly after each operation.
- Before each wave, prove a trivial process can start and record the soft open-file limit plus current coordinator open-file count. Start no wave when process creation fails or descriptor use is at least half the soft limit.
- Use two scenario agents per wave by default. Increase to at most four only after the previous wave completed without descriptor growth, unreaped children, zombies, or `Too many open files` errors.
- After every wave, wait for all agents, terminate and reap every owned child, close all command sessions, then repeat the descriptor preflight. Do not start the next wave until the count returns to its pre-wave baseline or lower.
- On `EMFILE`, `ENFILE`, or `Too many open files`, launch nothing else. End the current wave, reap children and zombies through their owning launcher, close sessions, and retry the preflight with bounded backoff. Mark affected scenarios `blocked` and retry them with fresh agents only after recovery.

Tell every scenario agent it is not alone, must not modify or revert shared files, and must not edit shared audit documents or create commits. Give it exclusive ownership of:

`artifacts/scenario-audits/<scenario-stem>/`

## Drive deterministic gameplay

Run fixture as real gameplay in background/headless mode unless real UI input is essential. Use unique automation directories, PIDs, and window identity. Never select windows through ambiguous shared process names.

Set `AOE_MAIN_MENU=0` for direct scenario captures and `AOE_AUDIT_ANY_MAP_SIZE=1` for compact audit fixtures; verify reported map dimensions. Inspect the first frame before continuing, and reject the run immediately if it shows the frontend. Launch the packaged app executable when available and require startup-log proof that optional original sprites loaded. Record all asset-loader flags; never use `AOE_DISABLE_LEGACY_ASSETS=1` when validating production presentation.

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

## Batch terrain-over-sprite review

- Detect terrain replacing opaque sprite pixels with `tools/visual_overlap_audit.py`. Supply an actual gameplay capture, matching terrain-only capture, expected production RGBA sprite frame, and exact top-left or screen-anchor placement. Treat exit `0` as clean, `1` as overlap candidates, and `2` as invalid input.
- Generate actual, terrain-only, and sprite inputs from the same tick, camera, resolution, terrain-animation frame, fog state, elevation, palette, and random seed. Preserve transparent sprite holes. Treat production sprite/frame, anchor, palette, and aligned terrain-only generation as pre-approved when derived deterministically from renderer state; do not stop for per-asset approval.
- Add every rendered sprite case across all scenarios, civilizations, ages, facings, animation states, damage/construction stages, terrains, elevations, resolutions, and playthrough matrix cells to one schema-version-1 JSON manifest. Use stable unique case IDs and record entity, sprite/frame, scenario, tick, camera, terrain, civilization, age, ownership, state, facing, and resolution in case metadata.
- Run `python3 tools/batch_visual_overlap_audit.py <manifest.json> --output-dir artifacts/visual-overlap-review` only after the whole requested corpus is captured. Exit `1` means review candidates exist, not infrastructure failure.
- Deliver one self-contained `review.html` for the human to audit the entire rendered-sprite corpus at once. Include clean and flagged cases; never interrupt the run for individual candidate review. Preserve combined `report.json`, red-contour annotations, actual views, terrain-only views, expected sprites, and downloadable human decisions JSON.
- Accept human decisions only as `bug`, `intentional`, or `uncertain`. Convert confirmed bugs into regression cases, retain intentional overlaps in an explicit baseline, and keep uncertain cases open without counting them as bugs or passes. On later runs, surface only new candidates, changed candidates, missing baseline entries, and unresolved cases.
- Mark cases with missing or uncorrelated layer inputs `blocked`; never silently exclude them from the whole-game manifest or coverage totals.

## Confirm findings

Report only reproducible mismatches supported by:

- affected scenario and tick;
- expected versus actual presentation;
- exact reproduction actions;
- screenshot or frame-sequence evidence;
- relevant original or decompiled reference;
- duplicate-group identifier when same root defect appears elsewhere.

Seek exhaustive coverage. When caller supplies minimum confirmed-bug target, pursue it through broader valid coverage, but never invent, inflate, split duplicates, or misclassify findings to meet quota. If fewer exist, report evidence-backed total and remaining coverage gaps.

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

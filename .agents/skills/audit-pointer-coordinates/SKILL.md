---
name: audit-pointer-coordinates
description: Thoroughly test this reconstruction's cursor tracking, SDL event conversion, hover and click hit-testing, drag selection, HUD controls, world picking, minimap targeting, camera centering, resizing, fullscreen, letterboxing, and Retina/high-DPI behavior; reproduce and log evidence-backed coordinate bugs. Use when cursor position feels wrong, controls activate away from the pointer, minimap clicks select or center on wrong locations, pointer behavior changes by resolution, or an agent must run a systematic pointer-input audit.
---

# Audit pointer coordinates

## Ownership boundary

Own coordinate conversion, pointer hit testing, drag selection, world/minimap
picking, resizing, fullscreen, letterboxing, and high-DPI behavior. Do not
broaden pointer-driven setup into gameplay, Nostr protocol, synchronization,
or visual-fidelity acceptance. Route failures outside coordinate mapping to
`audit-browser-multiplayer-gameplay`, `test-nostr-multiplayer`,
`diagnose-gameplay-sync`, or `audit-visual-fidelity`.

Read repository `AGENTS.md` files first. Keep original `decompiled/` source read-only; inspect relevant original mouse, hit-test, minimap, and viewport code before judging intended behavior. Do not use any GSD workflow unless current user explicitly requests GSD.

Audit and log bugs. Do not fix product code unless current request also asks for fixes. Preserve unrelated changes.

Read [test-matrix.md](references/test-matrix.md) before running audit. Use [bug-report-template.md](references/bug-report-template.md) for every confirmed bug.

## Establish coordinate contract

Map complete production path before testing:

1. OS/global position, when foreground input is unavoidable.
2. SDL window event position.
3. `SDL_ConvertEventToRenderCoordinates` result.
4. frontend logical position, HUD position, viewport position, or minimap position.
5. resolved control, world tile, minimap tile, drag rectangle, or camera target.
6. visible response and resulting semantic game state.

Use codebase-memory graph tools before text search for code discovery. Find every coordinate conversion and every consumer. Trace mouse motion, button down/up, wheel, resize, fullscreen, renderer logical-presentation, menu hit testing, HUD hit testing, world picking, minimap inverse projection, drag selection, and camera centering. Search literals and environment proof hooks with `rg` only when graph results are insufficient.

Record coordinate-space units beside every stage. Prove where conversion occurs. Flag double conversion, missing conversion, stale state, inconsistent dimensions, inclusive/exclusive edge disagreement, clamping outside interactive geometry, and separate render/hit formulas for targeted testing.

## Build independent oracle

Never validate code only against same formula used by production. Derive expected results from declared UI rectangles, rendered geometry, scenario state, and independently calculated anchor points.

For controls, generate center, four inset corners, each edge, one point immediately outside every edge, gaps, letterbox/cropped areas, and distant negative controls. Expected hit comes from visible control bounds.

For minimap, choose known map tiles: four corners, center, quarter points, both diagonals, player starts, visible units, and camera center. Derive expected click point from rendered tile marker or independently specified geometry. Compare resolved tile and resulting camera center. Account only for unavoidable many-tiles-per-pixel quantization; calculate and record tolerance before seeing result.

For world picking, project known tile centers to screen, inject clicks, then compare selected or commanded tile. Include camera offsets, viewport edges, elevation where supported, and HUD exclusion.

## Run deterministic layers

Run cheapest layers first; continue after failures to measure full scope.

### Pure tests

Exercise conversion, hit-test, projection, and inverse-projection helpers without SDL window. Add temporary local harnesses only under `artifacts/`; do not alter tracked files unless user asked for test implementation. Require:

- transform identity at native logical size;
- exact-once conversion across scale and offset;
- forward/inverse round trips within predeclared tolerance;
- no hit outside visible geometry;
- hover cleared after hit-to-miss motion;
- down/up consistency and drag endpoint consistency;
- stable normalized target after resize/fullscreen transitions.

### Production event path

Prefer existing test hooks that inject SDL events and log semantic outcomes. Use one long-lived game process, deterministic scenario, unique automation directory, exact PID/window identity, and correlated timestamps. Capture input coordinate, every converted coordinate, resolved target, state change, and screenshot.

Use background-safe semantic commands and non-activating window screenshots for setup and observation. They cannot prove real pointer mapping by themselves. Use real foreground pointer input only for cases injection cannot represent. Tell user before foreground control, keep interval short, and return to background operation immediately.

Never infer click coordinates from full-resolution Retina screenshot pixels. Use logical window screenshots or explicitly measured pixel-density conversion.

### Render/action agreement

For every sampled point, require all applicable observations to agree:

- cursor appears at intended visible location;
- hover belongs to control under cursor or clears on miss;
- click activates same control as hover;
- minimap click resolves to tile represented at that pixel;
- camera centers on resolved minimap tile within documented clamping limits;
- world click resolves to visibly targeted tile;
- resize, fullscreen toggle, and focus changes do not leave stale coordinates.

Repeat failures from clean launch at least twice. Include one adjacent passing control point when possible.

## Classify results

Use `passed`, `bug`, `blocked`, or `not-tested` per matrix cell.

Call issue `bug` only when reproducible mismatch has independent expected result, actual result, production reachability, and evidence. Keep hypotheses separate. Deduplicate manifestations sharing one likely root cause, while listing every affected mode and coordinate.

Treat every cybersecurity claim as unverified. Apply repository five-field proof (`BOUNDARY`, `SOURCE`, `PATH`, `HARM`, `CONTRACT`) before any security classification. If any field missing, mark claim `MISCLASSIFIED`, retain underlying product bug, and continue audit.

## Log audit

Write disposable raw captures and logs beneath:

`artifacts/pointer-coordinate-audits/<timestamp>/`

Write consolidated tracked report beneath:

`docs/audits/<YYYY-MM-DD>-pointer-coordinate-audit.md`

Report:

- build/version and exact launch configuration;
- window, drawable, renderer-logical, viewport, and HUD dimensions;
- DPI, display scale, window mode, resolution, map size, and frontend/game state;
- complete matrix with no silent omissions;
- coordinate-stage traces for failures;
- deduplicated bug records using template;
- passing boundary controls and negative controls;
- blocked/not-tested cases with exact reason;
- coverage totals and remaining gaps;
- evidence paths and commands needed to reproduce.

Do not report blocked cells as passes. Do not claim exhaustive coverage while any required matrix cell remains `not-tested`; say bounded audit and name gaps.

## Improve this skill after every audit

Treat each completed audit as evidence about workflow quality. Before finishing, review commands, matrix results, raw evidence, report drafting, and blockers. Record concise lessons under `Skill improvement` in audit report:

- missed coordinate spaces, surfaces, transitions, or negative controls;
- tests that produced false confidence or shared production oracle;
- unreliable launch, input, capture, cleanup, or correlation steps;
- repeated manual work suitable for deterministic script;
- ambiguity that caused inconsistent classification or incomplete evidence;
- existing instruction that prevented a real failure or wasted work.

Update this skill in same audit run when lesson is reusable, evidenced, and changes future agent behavior. Prefer tightening existing text. Add reference or script only when instruction alone cannot make repeated work reliable. Test any new script with representative passing and failing input.

Accept improvement only when at least one condition holds:

- same problem occurred in two independent matrix cells or runs;
- one problem caused false pass, lost evidence, wrong coordinate-space attribution, unsafe foreground control, leaked process, or incomplete required coverage;
- repository gained new pointer surface, automation hook, presentation mode, or durable contract absent from matrix.

Reject speculative advice, fixture-specific coordinates, transient machine details, copied audit findings, duplicate rules, broad prose, and changes made only to explain one bug. Never weaken independent-oracle, reproducibility, complete-matrix, background-safety, security-proof, build, cleanup, or evidence requirements.

After editing:

1. Re-read full changed skill and directly referenced resources.
2. Compare diff against lesson; remove unrelated edits.
3. Run skill-creator `quick_validate.py`; use disposable dependency environment if host lacks validator modules.
4. Run `git diff --check` on skill files.
5. Re-run smallest audit check affected by change when practical.
6. Record lesson, accepted or rejected decision, files changed, validation result, and remaining uncertainty in audit report.

Do not rewrite history or silently erase useful rules. If current audit evidence conflicts with existing guidance, preserve both facts in report and change skill only after resolving conflict through another independent check. Include accepted skill edits in focused audit commit after required `make` succeeds.

## Finish

Terminate owned game processes and close automation sessions. Run `make` from repository root. If tracked files changed, stage only current audit/skill files and create focused commit after `make` succeeds. If `make` fails because of unrelated existing work, do not commit; report exact blocker. Return report path, confirmed bug count, matrix totals, build result, commit hash or explicit no-commit reason, and most important gaps.

# Browser pointer coordinate audit — 2026-08-06

## Result

The fixed browser scenario passed every pointer/display case required by
`web/BROWSER_BUILD_PLAN.md` in production WebAssembly under Chrome. No new
browser coordinate defect was reproduced.

The retained raw result is
`artifacts/browser-risk-spike/pointer-display-evidence.json`; screenshots use
the `pointer-*.png` names beside it. Raw artifacts are intentionally ignored.

## Coordinate path and independent oracle

Production C++ publishes only rendered logical target centers. The browser
test independently maps those logical points through the live canvas CSS
rectangle:

`page = canvas origin + logical target * CSS size / SDL logical size`

Trusted W3C mouse actions then enter the normal DOM, Emscripten SDL event,
`SDL_ConvertEventToRenderCoordinates`, world-picking, selection, and command
paths. Success is judged from simulation state, not DOM hitboxes or a test-only
command path:

- villager and military clicks must change production selection;
- resource right-click must increase gold through gathering;
- enemy-building right-click must reduce its production hit points;
- runtime fallback count must remain zero.

Each display case starts from a fresh production page and fixed scenario, so
prior movement or combat cannot contaminate its oracle.

## Required browser matrix

| Case | Edge sample | Live display evidence | Result |
|---|---:|---|---|
| DPR 1, 100% | center | CSS 1280x720; backing 1280x720; DPR 1 | pass |
| DPR 2, 100% | left, 4 logical px | CSS 1280x720; backing 2560x1440; DPR 2 | pass |
| DPR 1, 125% zoom | right, 4 logical px | DPR 1; visual viewport scale 1.25 | pass |
| Active resize | top, 4 logical px | CSS 1096.875x617; backing 1097x617 | pass |
| Fullscreen entered by click | bottom, 4 logical px | fullscreen true; CSS/backing 800x450 | pass |
| Return from fullscreen | center | fullscreen false; CSS 1096.875x617 | pass |
| Letterboxed aspect | center | 1000x562.5 canvas inside 1000px-tall viewport | pass |

Across the matrix, center plus left, right, top, and bottom near-edge samples
were exercised for all four required target classes. Every case completed
villager selection, resource targeting, military selection, and enemy-building
targeting.

## Additional pointer surfaces

| Surface | Result | Evidence |
|---|---|---|
| World left click | pass | Villager and military production selections in all seven cases |
| World right click | pass | Gathering and combat state changes in all seven cases |
| CSS/backing conversion | pass | DPR 1 and 2 evidence above |
| Browser zoom conversion | pass | Visual viewport scale 1.25 with successful state changes |
| Resize/fullscreen conversion | pass | Successful state changes after each transition |
| Minimap | not repeated | Existing 2026-08-04 native audit retains PTR-001; browser build plan does not require a minimap action |
| Drag selection | not required by browser plan | No browser drag-selection claim made |
| Wheel/technology-tree overlays | not required by browser plan | No browser overlay claim made |
| Second physical display | unavailable | DPR transition was covered through browser device emulation |

## Validation

- `python3 -m py_compile tests/web/browser_risk_spike_test.py`: pass
- `python3 tests/web/browser_risk_spike_test.py --browser chrome --display-matrix`: pass
- Missing HTTP responses: none
- Runtime asset fallbacks: zero at every retained checkpoint
- Uncaught browser exceptions: none

The earlier native minimap issue remains a separate known product-impact bug;
this report neither retests nor closes it.

# Cursor fidelity

## Exact archive contract

The supplied `Data/interfac.drs` contains SLP resource 51000 with 19 frames.
`generated/cursor_catalog.json` records every frame's signed width, height,
hotspot X, and hotspot Y directly from its 32-byte SLP frame header. All 19
dimension and hotspot records are exact archive facts.

| Frame | Size | Hotspot |
|---:|---:|---:|
| 0 | 24×32 | 0,0 |
| 1 | 26×38 | 13,19 |
| 2 | 42×22 | 21,11 |
| 3 | 37×37 | 1,0 |
| 4 | 35×33 | 0,0 |
| 5 | 54×29 | 27,15 |
| 6 | 27×32 | 0,0 |
| 7 | 35×35 | 0,17 |
| 8 | 35×37 | 0,13 |
| 9 | 28×46 | 0,0 |
| 10 | 39×39 | 0,0 |
| 11 | 40×28 | 5,0 |
| 12 | 36×22 | 18,9 |
| 13 | 32×38 | 0,0 |
| 14 | 32×38 | 0,0 |
| 15 | 43×42 | 2,0 |
| 16 | 45×43 | 7,0 |
| 17 | 32×26 | 15,17 |
| 18 | 45×41 | 23,40 |

These hotspot values are consumed as signed coordinates. They are not inferred
from visible pixels or clamped in the catalog.

## Exact executable selector contract

The supplied HD executable decompilation loads `mcursors.shp` with resource
ID `0xc738` (51000). This is an exact HD executable filename/resource binding,
independently matching the live archive.

The cursor manager vtable at `0x7804dc` binds offset `+0x1c` to
`FUN_004dcca0`. That function accepts only `0 <= frame < 19`, stores the
argument directly as the selected outer-region frame, and reads the matching
32-byte SLP record. `FUN_004dd2d0` copies record offsets `+0x18/+0x1c` into
the per-frame hotspot arrays. Rendering passes those values unchanged.

Two state bindings are proved:

| State | Frame | Executable evidence | Hotspot |
|---|---:|---|---:|
| normal/restored arrow | 0 | `FUN_005b2f20`: `LoadCursorA(IDC_ARROW)`, then `FUN_004ed910(0)` | 0,0 |
| modal busy/wait | 6 | `FUN_005b2ec0`: `LoadCursorA(IDC_WAIT)`, then `FUN_004ed910(6)` | 0,0 |

`FUN_005b2f70` also restores frame 0. The binary's direct call references to
`FUN_004ed910` are exactly the frame-6 call and two frame-0 calls above.

No timer or automatic frame-advance path exists in this manager. A selection
is static and persists until another selector call, so exact cadence is
`0 ms`/none rather than a guessed animation rate.

Visibility is separate from selection. `FUN_004ed780` shows the custom cursor
through manager vtable `+0x38`; `FUN_004ed7e0` hides it through `+0x34`.
`FUN_004ed830` switches between custom and system cursor modes.

## Gameplay mapping limits

No reviewed data flow proves frame values for select, move, attack, gather,
build, repair, heal, convert, invalid action, or any of the eight scroll
directions. Gameplay reaches the manager's inner-region setter dynamically
after hit testing, but the recovered arguments do not establish semantic enum
values. Artwork and frame ordering are not evidence.

The pure runtime contract therefore maps the two proved states above and fails
every unproved gameplay state closed to frame 0 with explicit
`unknown_fallback` evidence. SDL requests the proved normal state at startup;
it does not present guessed action cursors. Hardware cursors are not included
in renderer screenshots, so the contract and decoded archive metadata are the
auditable fidelity evidence.

## Reproduction

Generate the catalog from user-owned inputs:

```sh
python3 tools/dat_metadata/generate_cursor_catalog.py \
  /path/to/Data/interfac.drs \
  --hd-executable /path/to/AoK-HD.exe
```

Focused tests validate DRS bounds, SLP table bounds, synthetic signed hotspot
extraction, all 19 live records, the two executable selectors, static cadence,
and frame-0 fallback for every unproved gameplay state.

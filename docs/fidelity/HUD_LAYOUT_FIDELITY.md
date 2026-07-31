# In-game HUD layout fidelity

## Exact archive evidence

`generated/hud_layout_catalog.json` audits installer-owned `interfac.drs` and
the pinned local executable. `interfac.drs` contains unlinked SLP 51141 with:

- one physical frame;
- exact 1280×1024 dimensions;
- hotspot 0,0;
- SLP property value 16;
- bounded outline and command-table offsets;
- pinned archive and payload SHA-256 identities.

The executable contains exact strings `game_b%d.slp`, `map1024.bmp`, and
`Game Screen`.

## Exact game-background call chain

Pinned callsite `0x005f33f1..0x005f3446` closes filename selection and load
source:

- `%d` is local player's civilization byte at player offset `+0x15d`;
- `game_b%d.slp` therefore selects civilization artwork, not resolution;
- loader receives resource ID `-1` and loose-file mode, resolving through
  `slp/game_b%d.slp`, not an `interfac.drs` resource ID.

`FUN_005e7cb0` proves background composition uses at least eight frames:

- frame 0 tiles horizontally across top;
- frame 6 draws at origin over that strip;
- frames 2 and 3 alternate across bottom span;
- frames 1 and 4 cap left and right ends of bottom span;
- frame 5 is centered in that span;
- frame 7 anchors horizontally relative to a sibling view.

`FUN_005f37c0` positions that sibling at
`x=((screen_width-frame6.width-260)/2-frame7.width/2)+frame6.width`,
`y=6`, width `frame7.width`, height 20. `FUN_005e7cb0` then uses
`sibling.x-(frame7.width-sibling.width)/2`; with the recovered equal widths,
the final frame-7 anchor equals the sibling x.

Other compositor operands are literal translations:

- `bottom = screen_height - frame1.height`;
- bottom tiling begins at `frame1.width - frame2.width`, uses frame 3 when
  `tile_index % 4 == 3`, and stops at
  `frame2.width + screen_width - frame4.width`;
- frame 4 x is `screen_width - frame4.width`;
- frame 5 x is
  `((frame4.x-frame1.width)/2-frame5.width/2)+frame1.width`;
- frame 5 y is `bottom-frame5.height/2`.

SLP 51141 has one frame. It cannot be `game_b%d.slp` consumed by this call
chain. Prior promotion of SLP 51141 as HUD background is disproved.

The recovered loose files are now present under `Data/Slp`. All 18 use eight
property-7, direct-BGRA frames. The decoder preserves their BGRA colors,
per-pixel alpha, premultiplied-alpha runs, outlines, hotspots, and transparent
pixels. Runtime composition selects the file with the active view player's
civilization value, exactly matching the byte passed to `game_b%d.slp` at
`0x005f33f1..0x005f341b`:

| Civilization byte | Civilization | File |
|---:|---|---|
| 1 | Britons | `game_b1.slp` |
| 2 | Franks | `game_b2.slp` |
| 3 | Teutons | `game_b3.slp` |
| 4 | Goths | `game_b4.slp` |
| 5 | Celts | `game_b5.slp` |
| 6 | Vikings | `game_b6.slp` |
| 7 | Byzantines | `game_b7.slp` |
| 8 | Japanese | `game_b8.slp` |
| 9 | Chinese | `game_b9.slp` |
| 10 | Persians | `game_b10.slp` |
| 11 | Saracens | `game_b11.slp` |
| 12 | Turks | `game_b12.slp` |
| 13 | Mongols | `game_b13.slp` |
| 14 | Spanish | `game_b14.slp` |
| 15 | Huns | `game_b15.slp` |
| 16 | Koreans | `game_b16.slp` |
| 17 | Aztecs | `game_b17.slp` |
| 18 | Mayans | `game_b18.slp` |

Frame metadata common to all files is:

| Frame | Dimensions / hotspot | Use |
|---:|---|---|
| 0 | 32×32, (0,0) | repeated top tile |
| 1 | 322–325×175, (0,0) | bottom-left cap |
| 2 | 34×175, (0,0) | bottom repeating tile |
| 3 | 34×175, (0,0) | every fourth bottom tile |
| 4 | 384–391×175, hotspot x 0 to -7 | bottom-right cap |
| 5 | 118–239×107–130, civ-specific negative hotspot | centered ornament |
| 6 | 392×25, (-2,-1), or 396×28, (0,0) | origin overlay |
| 7 | 165×21, (-2,-5), or 169×32, (0,0) | sibling-relative overlay |

Focused installed-asset tests decode and validate every frame rather than
assuming this summary.

## Reconstruction geometry

Current renderer uses the requested logical window dimensions. At 1280×1024:

- world viewport: `(0, 0, 1280, 849)`;
- HUD band begins at y=849 because frame 1 is exactly 175 pixels high;
- command slots use the recovered 5-column by 3-row geometry;
- the minimap uses the recovered 326×164 bottom-right frame.

The same `screen_height - 175` split is evaluated at 1024×768 and 640×480.
Artwork is neither stretched nor cropped. At 640 pixels wide the original
left and right cap widths exceed the span and therefore overlap; no executable
evidence authorizes scaling or cropping them.

## Exact relative layout contract

Pinned `FUN_005f37c0` proves geometry relative to stored screen fields. It does
not prove producers or semantic names for those fields:

- `bottom = screen_height - stored_bottom_height`;
- optional top child is
  `(0, stored_top, screen_width, top_child_visible ? 30 : 0)`;
- main child starts at
  `stored_top + (top_child_visible ? 30 : 0)` and ends inclusively at
  `bottom`, giving height `bottom - main_y + 1`;
- command slots form a 5-column by 3-row grid for indices 0 through 14:
  `(37 + 41*(index%5), bottom + 31 + 41*(index/5), 40, 40)`;
- large anchored panel is
  `(screen_width-336, screen_height-169, 326, 164)`;
- top status strip is `(2, 2, 420, 16)`;
- centered top control is `(screen_width/2-155, 16, 310, 20)`;
- five top-right controls use
  `(screen_width-260 + 50*index, 3, 50, 19)`.
- eight bottom-right controls use:
  `(w-308,h-154,35,35)`, `(w-309,h-49,35,35)`,
  `(w-96,h-156,25,25)`, `(w-69,h-162,25,25)`,
  `(w-60,h-137,25,25)`, `(w-61,h-59,25,25)`,
  `(w-74,h-35,25,25)`, and `(w-102,h-39,25,25)`.

These are exact relative operands. The recovered frame-1 height now also
proves the absolute bottom split. Semantic roles for several child pointers
remain unproved.

Exact Wood/Food/Gold/Stone/Population field coordinates remain unproved. The
runtime therefore uses a reconstruction-native readability contract:

- one ordered field per Wood, Food, Gold, Stone, and Population;
- 10 logical pixels of safe left/right margin and 6-pixel guard bands;
- row width capped at 780 pixels, left-anchored, so wide screens retain one
  compact original-style status group;
- deterministic left-to-right distribution of integer remainder pixels;
- 16×16 resource icons, 4-pixel icon/text padding, fixed 8-pixel debug-font
  cells, and one renderer clip rectangle per field;
- same five bounded fields when icon assets are absent;
- Population always keeps `POP current/capacity`; `PAUSED` has priority over
  optional `IDLE villagers/military` when narrow width cannot fit both.

Dark field backing makes the text readable over native `game_b%d.slp` top
ornaments. Truncation only handles large values or long localization after
geometry and renderer clips enforce separation. Field clips are cleared before
the existing information-panel clip is restored.

Contract tests cover logical widths 640, 800, 1024, 1280, and 1920 with normal,
nine-digit, paused, large-population, long-label, icon, and no-icon cases.
`aoe_hud_layout_sdl_smoke` captures all five widths with both asset paths,
plus 640×480 at deterministic 1× and 2× renderer output. Its pixel validator
requires text in every field, exact foreground-free inter-field guard bands,
and no resource foreground beyond the row. `AOE_HUD_STRESS_VALUES=1` affects
display values only; `AOE_HUD_OUTPUT_SCALE=1|2` creates physical-output test
variants while production layout remains logical-coordinate based.

This responsive policy is not an exact recovered field map. Exact evidence
remains limited to the surrounding status strip and screen-relative controls
listed above.

Runtime frame inspection proves 50721 frames 36 and 37 contain action artwork,
not reusable button chrome. Command slots use procedural normal, pressed,
selected, and disabled chrome. Bounded candidate frames render for mapped
actions with `unknown` semantic evidence; missing candidates retain labels.

## Original button and icon assets

The renderer uses `interfac.drs` command sheet 50721 (`btncmd`) for bounded
candidate action artwork, sheet 50730 (`ico_unit`) for proven unit icons,
sheet 50729 (`btntech`) for technology icons, and canonical building sheet
50706 (`ico_bld2`) for DAT-indexed building icons and portraits.
Procedural button chrome prevents any action frame from repeating behind every
command. Candidate action mappings are not promoted to exact dispatch.
`btngame`, `ico_game`, and `icomap_b/c/d` are not present in the recovered
installation; `btngame2x.slp` is present but no recovered in-game
command-grid callsite selects it.

## Remaining evidence gaps

- the semantic identity of the sibling pointer anchoring frame 7;
- portrait and information-child pointer identities;
- hover and disabled chrome frames;
- exact command semantics and page ordering for non-unit icons;
- authorization to alter the unavoidable cap overlap below their combined
  native width.

Missing or undecodable loose HUD files select procedural rendering. A valid
file is always composed natively and never stretched or cropped.

## Reproduction

```sh
python3 tools/audit_hud_contract.py \
  --interface-drs "/path/to/Data/interfac.drs" \
  --executable "/path/to/AoK HD.exe"
python3 tools/test_audit_hud_contract.py
```

Focused tests verify DRS/SLP bounds, exact frame metadata, exact relative
formulas/catalog classification, and fail-closed absolute layout promotion.
## Technology and civilization catalog

`F9` opens represented catalog without environment-only controls. `Q`/`E`
changes civilization; arrow keys move deterministic visible node focus,
Shift-Tab/Tab cycle entries, Home/End reach bounds, and WASD retains viewport
panning. Focused detail shows costs and prerequisites. Browser chrome uses
strict localization keys, and SDL smoke coverage renders both English and
representative long localized strings.

Layout, focus policy, node framing, and keyboard mapping are
reconstruction-native. Runtime labels explicitly report missing original icon,
framing, navigation, and font-metric evidence; no commercial screen-equivalence
claim is made.

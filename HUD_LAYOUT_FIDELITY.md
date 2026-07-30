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

SLP 51141 has one frame. It cannot be `game_b%d.slp` consumed by this call
chain. Prior promotion of SLP 51141 as HUD background is disproved.

## Reconstruction geometry

Current renderer window is 1280×720:

- world viewport: `(0, 0, 1280, 640)`;
- HUD band: `(0, 640, 1280, 80)`;
- legacy approximate SLP source: bottom 218 pixels of frame 0;
- legacy approximate destination: full 1280×80 HUD band.

Information panel `(5, 644, 708, 72)`, command panel
`(718, 644, 337, 72)`, portrait `(12, 671, 40, 40)`, resource icon positions,
four-column command buttons, minimap rectangle, clips, text baselines, and
hover/disabled colors are reconstruction policy.

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

These are `exact_relative`, not proof of an absolute world/HUD split,
resolution class, or semantic panel role. Current 1280×720 split and role
assignments remain `reconstruction_policy`.

`FUN_005c5e40` also proves generic button chrome frame 36 normally, frame 37
when pressed, and a pressed icon offset of 1,1. This does not prove that frames
36/37 encode every command-slot state. Hover and disabled frames, command
semantics, archive-backed placement, and page ordering remain unproved.

## Missing absolute layout contract

Authoritative local evidence does not yet provide:

- original loose `game_b1.slp` through civilization-specific variants;
- numeric frame dimensions needed to evaluate exact bottom-band height;
- portrait, resource, information, minimap, and command-panel rectangles;
- hover, disabled, command-semantic, icon-order, and hotkey button states;
- transparency and final panel draw order.

Therefore no exact absolute numeric layout is enabled. Exact relative formulas
may be used only with their observed stored operands. SLP 51141 must not be
labeled or promoted as `game_b%d.slp`; its bottom crop is structurally
contradicted by the eight-frame compositor. Missing loose background assets
retain procedural panels.

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

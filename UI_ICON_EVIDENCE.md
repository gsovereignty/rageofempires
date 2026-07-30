# UI icon evidence catalog

`generated/ui_icon_catalog.json` joins the validated VER 5.7 DAT to the
user-supplied `Data/interfac.drs` inventory for every represented unit,
building, technology, resource, and command.

The evidence layers remain separate:

1. DAT `button_icon`/technology `icon_id` values are numeric icon indices.
2. `interfac.drs` proves which SLP resource IDs exist and each SLP frame count.
3. An SLP resource/frame binding is recorded only when an independent DAT or
   openage source proves the relationship.

The current live evidence proves exact DAT icon indices for all 96 units and
27 buildings and for 133 of 158 technologies. The remaining 25 technologies
have no exposed icon index. Resources and commands have no icon field in the
validated DAT records. The archive independently proves, among other entries,
SLP 50721 has 69 frames, 50729 has 118, and 50730 has 134. Executable loads
prove all three sheet roles. Selector traces additionally prove identity
transforms for technology and ordinary-unit DAT icon fields; no meaning is
guessed from artwork or frame order.

## Provenance search

Supplied HD executable SHA-256
`e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
and its decompilation supply three exact loader bindings:

| Executable filename | DRS resource | Live frames | Proved role |
|---|---:|---:|---|
| `btncmd.shp` | 50721 (`0xc621`) | 69 | command/action sheet |
| `btntech.shp` | 50729 (`0xc629`) | 118 | technology sheet |
| `ico_unit.shp` | 50730 (`0xc62a`) | 134 | unit sheet |

The 50729 and 50730 calls are explicit
`FUN_0050c6b0(filename, resource_id, 0)` loads. A second executable setup path
loads all three by the same filename/resource pairs. This upgrades sheet roles
from tentative descriptions to `exact_executable_load`.

Exact dispatch:

- `FUN_005c6750` action `0x67` selects 50729. `FUN_00517560` reads the
  technology signed short at record `+0x2c`; `FUN_005c5e40` receives it
  unchanged as the frame.
- `FUN_005c7560` selects 50730 for ordinary units and passes record `+0x54`
  unchanged as the frame. Subtypes 2 and 10 instead select a
  civilization-indexed building sheet.
- `FUN_005c5e40` keeps the actual icon sheet/frame unchanged when pressed.
  Runtime artwork inspection proves frames 36 and 37 are action pictures,
  so they cannot serve as reusable button chrome.
- No disabled alternate icon frame or command-page ordering is proved.

Raw 50721 constants are also exact: action `0x7c` uses frame 12, `0x7d` uses
frame 13, and `0x65` uses frame 30 for subtype 2 or 31 otherwise. These raw
action codes are not assigned reconstruction `PanelCommand` names without a
proved semantic bridge.

`generated/ui_icon_catalog.json` preserves these facts in
`executable_dispatch_contract`.

The remaining sources do not prove item bindings:

- Openage's unit and research DAT readers name `icon_id`, but mark the field
  `SKIP`; no conversion path relates it to an SLP frame.
- Pinned openage commit
  `9a5a7ccbfc20c2de658fc746462cd4a69aa758ef` also names 50721
  `hudactions`, independently agreeing with the executable role.
- `doc/media/aoc-slp-list.md` descriptions for 50731, 50732, and 50760 do
  not prove DAT-index semantics.
- The independently reviewed DRS format specification proves resource
  ID/extension inventory structure only, not icon meanings.

The pure `aoe::ui_icons` contract exposes bounded technology and ordinary-unit
identity dispatch. It pins the validated VER 5.7 icon fields for the 15 unit
types currently trainable from the reconstruction command grid. Unsupported
units, buildings, actions, disabled variants, and page states fail closed.

Generate the report from external, user-owned files:

```sh
python3 tools/dat_metadata/generate_ui_icon_catalog.py \
  /tmp/aoe-metadata.json \
  /path/to/Data/interfac.drs
```

The runtime handoff API is in `aoe/ui_assets.hpp`:

- `inventory_ui_icon_sheets` exposes validated external SLP IDs/frame counts.
- `UiIconBinding` carries the evidence classification separately from the DAT
  index and optional SLP/frame.
- `decode_ui_icon` refuses anything except an explicitly `exact` SLP/frame
  binding and checks frame bounds before decoding.

SDL loads proved 50730 frames needed by train buttons and distinct 50721
action frames for represented commands. Procedural bevels provide normal,
pressed, selected, and disabled chrome without repeating action artwork.

No original archive, SLP, or decoded bitmap is committed. Runtime capture
requires the user's `AOE_ASSET_ROOT`; the selection-controls smoke validates
the archive-backed path without embedding assets.

# Building upgrade identity evidence

## Scope

`BUG-UI-001`: Guard Tower, Keep, Fortified Wall, and fortified gates must be
distinct simulation identities after their type-3 technology transformations.

## Original evidence

- `AoK-HD-patched.c` `FUN_005c7560` reads each selected object's live record
  subtype and icon field; selection presentation does not infer an upgraded
  portrait from a researched technology.
- VER 5.7 effect 187 (Fortified Wall technology 194) contains type-3 record
  replacements, including 117 to 155 for wall and 64 to 63 / 487 to 488 for
  gate orientations.
- Read-only DAT extraction gives Watch Tower 79: 1020 HP/icon 25, Guard Tower
  234: 1500 HP/icon 25, Keep 235: 2250 HP/icon 26, Stone Wall 117: 1800
  HP/icon 31, Fortified Wall 155: 3000 HP/icon 31, Stone Gate 64/487: 2750
  HP/icon 36, and fortified gate 63/488: 4000 HP/icon 36.

## Reconstruction contract

Technology completion now replaces `Building.kind`. Future construction and
Korean free tower technologies create the transformed kind directly. Building
rules own transformed HP, armor, attack, footprint, name, and icon. Renderer
asset lookup aliases transformed kinds only to their corresponding original
art families; technology state no longer chooses tower or wall tier.

Save version 114 persists all five new identities. Older saves infer the
identity once from their stored researched technologies during migration.
Replay technology commands exercise the same simulation transformation.

## Regression proof

- `aoe_core_tests`: existing/future wall, both gate orientations, Guard Tower,
  Keep, Korean free Guard Tower, save round trips, and serialized replay.
- `aoe_ui_icon_contract_tests`: exact DAT icon frames 25, 26, 31, and 36.
- `render_asset_coverage_tests`: transformed Keep and Fortified Wall identities
  retain canonical world-art bindings.
- `aoe_ui_icon_sdl_smoke`: dummy/software background captures select Guard
  Tower, Keep, and Fortified Wall. Each scenario is captured twice and must be
  byte-identical; different transformed selections must differ.

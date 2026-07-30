# Building damage-state runtime evidence

This note separates facts proved from the shipped data and executable from
remaining unknowns. It covers every building kind in
`generated/building_body_state_catalog.json`; no renderer or simulation policy
is inferred where the executable link is still missing.

## Pinned inputs

- `AoK HD.exe`
  SHA-256 `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
- `empires2_x1_p1.dat`
  SHA-256 `e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`
- `graphics.drs`
  SHA-256 `b0541bbf9dc45cdef85eb50563d5412026efc56306334b66a88b56557d11bfdf`
- Ghidra C export:
  `/Users/gareth/Downloads/AOE/decompiled/AoK-HD-patched.c`

## Proven runtime representation

The static-master loader `FUN_00575420` at VA `0x00575420` reads:

- damage-record count: master `+0x9b`, one byte;
- damage-record array: master `+0x9c`, elements of eight bytes;
- element `+0`: resolved graphic pointer, four bytes;
- element `+4`: serialized `damage_percent`, two bytes;
- element `+5`: serialized `flag`, one byte.

The last write overlaps and replaces the high byte of serialized
`damage_percent`. Runtime selection in `FUN_00589490` reads only element byte
`+4`. Therefore the effective threshold is exactly
`serialized_damage_percent & 0xff`.

This proves why Stone Wall records `537/562/587` (`0x0219/0x0232/0x024b`) with
flag 2 still use effective thresholds `25/50/75`. The duplicated high byte is
not part of the runtime comparison.

## Proven selector

`FUN_00589490` at VA `0x00589490` is the damage-graphic state updater.

- Current HP is object float `+0x30`.
- Maximum HP is master signed short `+0x2a`.
- Constant float at VA `0x00772b2c` is exactly `100.0f` (raw
  `00 00 c8 42`, executable file offset `0x372b2c`).
- It computes:

  `damage = uint8(100 - trunc(current_hp * 100.0f / max_hp))`

- It stores the previous computed damage byte at object `+0x34`.
- For each record in DAT order, it selects the last record whose effective
  threshold is strictly less than `damage`.

Implementation-ready selector:

```text
damage = u8(100 - trunc_toward_zero(current_hp * 100.0 / max_hp))
selected = none
for record in records_in_dat_order:
    if (record.damage_percent & 0xff) < damage:
        selected = record
```

Consequences:

- 25% computed damage does **not** select the 25 record; 26% does.
- 50% computed damage still selects the 25 record; 51% selects the 50 record.
- 75% computed damage still selects the 50 record; 76% selects the 75 record.
- Re-evaluation is skipped when the newly computed damage byte equals object
  `+0x34`.
- Max HP less than 1, or an empty record list, returns without selection.
- Computed damage over 99 bypasses selection and only updates cached damage.

The rounding behavior is local and does not depend on process initialization.
At `0x005894d6` the function saves the x87 control word, ORs it with `0x0c00`
(round toward zero), loads that temporary control word, executes `fistp`, then
restores the saved control word at `0x005894fb`. Current HP and max HP are
positive on the selectable path, so this is exactly floor.

Half-ratio fixtures:

- HP ratio 75.5% -> trunc 75 -> damage 25 -> no record.
- HP ratio 75.0% -> trunc 75 -> damage 25 -> no record.
- HP ratio 74.5% -> trunc 74 -> damage 26 -> 25 record.
- HP ratio 50.5% -> trunc 50 -> damage 50 -> 25 record.
- HP ratio 49.5% -> trunc 49 -> damage 51 -> 50 record.
- HP ratio 25.5% -> trunc 25 -> damage 75 -> 50 record.
- HP ratio 24.5% -> trunc 24 -> damage 76 -> 75 record.

## Proven composition and reversal

When the selected record changes, `FUN_00589490` first activates the new record,
then removes the old record.

- Flag 0: calls `FUN_004eaf00(graphic, 0x5a, 0, 0, 0)`. This is an attached
  graphic/effect path; it does not replace the standing graphic.
- Flag 1: chooses randomized offsets, then uses the same `FUN_004eaf00`
  attachment path.
- Flag 2: calls object virtual slot `+0x40` with the record graphic. This is the
  replacement-graphic path.
- Removing an old flag 0/1 record calls `FUN_004eafc0(old_graphic)`.
- Leaving flag 2 for no record or a non-flag-2 record restores master standing
  graphic `+0x18` through virtual slot `+0x40`.

Thus repair reversal is proved inside the selector: decreasing computed damage
selects a lower record, removes the old overlay, or restores the standing
graphic after a flag-2 replacement.

Death transition `FUN_0059a7e0` at VA `0x0059a7e0` explicitly iterates all
damage records and calls `FUN_004eafc0` before setting HP to zero and continuing
death handling. Damage attachments therefore do not intentionally survive the
death transition.

## Animation and layer facts

The complete graphic roots, recursive delta layers, offsets, SLP IDs,
frame counts, angle counts, palette IDs, frame rates, and replay delays for all
damage records are already enumerated per civilization family in
`generated/building_body_state_catalog.json`.

The runtime starts flag-0/1 damage graphics on the state transition by calling
the attachment routine. `FUN_004eaf00` creates a fresh animation instance via
`FUN_00510830` at `0x00510830`.

- Non-animated graphics (`graphic flags +0x70 & 1 == 0`) use the compact
  instance from `FUN_004eb520`.
- Animated graphics use `FUN_004eb870` at `0x004eb870`. A fresh instance starts
  at frame zero and with its elapsed accumulator zero unless graphic flag
  `+0x70 & 4` requests a randomized start. The randomized path chooses both a
  starting frame and corresponding elapsed position.
- The initial per-frame timer comes from graphic float `+0x68` (catalog
  `frame_rate`), clamped to a small positive minimum.
- `FUN_004ebb90` at `0x004ebb90` accumulates elapsed time, advances frames, and
  wraps after `frame_count` (`graphic +0x5e`). Graphic float `+0x6c` is the
  replay-delay/terminal-delay input used when the last frame is crossed.

Flag 2 calls virtual graphic setter slot `+0x40`; for the static-object vtables
this resolves to `FUN_005794b0` at `0x005794b0`, which delegates to
`FUN_0058d110`. When the graphic differs, that function removes the old
animation instance and attaches the new graphic, creating a fresh instance
through the same `FUN_004eaf00` path. Therefore a flag-2 transition resets the
animation using the same frame-zero/random-start and cadence rules. Calling the
setter with the already-current graphic is a no-op and does not reset its
clock.

## Coverage

The catalog contains exactly 27 represented `BuildingKind` values and 104
civilization families. Every kind has damage records. Across them, the only
record forms are:

- effective 25/50/75, flag 0;
- serialized 537/562/587 -> effective 25/50/75, flag 2.

The 27 kinds are:

`town_center`, `barracks`, `archery_range`, `house`, `mill`, `lumber_camp`,
`mining_camp`, `farm`, `stable`, `blacksmith`, `castle`, `university`,
`siege_workshop`, `palisade_wall`, `watch_tower`, `stone_wall`,
`palisade_gate_x`, `palisade_gate_y`, `stone_gate_x`, `stone_gate_y`,
`monastery`, `market`, `dock`, `bombard_tower`, `fish_trap`, `outpost`,
`wonder`.

## Isolated missing links

- `FUN_00589490` occupies virtual slot `+0x28` in ten static-object-derived
  vtables (including the building vtables). `FUN_00589230` invokes that slot
  from its state-2 update path.
- Object `+0x20` is the containing/parent static-object link, not a construction
  marker. `FUN_0058a320` adds a child to the parent's object list and writes the
  parent to child `+0x20`; `FUN_0058a360` removes the child and clears `+0x20`.
  Constructors `FUN_0058b460` and `FUN_0058dc70` initialize it to zero.
  `FUN_00589230` runs the local state-2/damage update only for top-level objects
  (`+0x20 == 0`); contained children inherit/track the parent state instead.
- The base static-object constructor initializes lifecycle state byte `+0x48`
  to 2 and top-level link `+0x20` to zero. Consequently a top-level building
  takes the damage-selector path while it is under construction; construction
  is not a suppression condition.
- Construction graphic `master +0x1b8` is independently attached/removed by
  building construction handlers including `FUN_0059a630`. It uses the
  attachment list while damage flag 0/1 uses its own attachment entry.
  Completion removes/changes the construction attachment; it does not reset the
  cached damage byte or remove the selected damage attachment. Damage changes
  during building/repair are handled on the next state-2 update by the same
  selector and repair-reversal logic.
- The data proves fire/smoke graphic composition and delta layering. It does
  not by itself prove a semantic label for every raster layer; use catalog
  graphic/SLP identities, not guessed visual names.

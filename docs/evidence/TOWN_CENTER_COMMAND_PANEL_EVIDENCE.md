# Town Center command-panel evidence

## Source

The locally supplied VER 5.7
`game_data/Data/empires2_x1_p1.dat` was decoded with the pinned
`tools/dat_metadata` extractor documented in
`tools/dat_metadata/README.md`.

The decoded records expose command-button positions directly:

| Record | DAT ID | DAT field | Value |
|---|---:|---|---:|
| Villager | 83 | `creation.create_at_unit` | 109 |
| Villager | 83 | `creation.create_button` | 1 |
| Loom | 22 | `location` / `button_id` | 109 / 6 |
| Wheelbarrow | 213 | `location` / `button_id` | 109 / 7 |
| Hand Cart | 249 | `location` / `button_id` | 109 / 7 |
| Town Watch | 8 | `location` / `button_id` | 109 / 8 |
| Town Patrol | 280 | `location` / `button_id` | 109 / 8 |
| Feudal Age | 101 | `location` / `button_id` | 109 / 11 |
| Castle Age | 102 | `location` / `button_id` | 109 / 11 |
| Imperial Age | 103 | `location` / `button_id` | 109 / 11 |

Unit ID 109 is the Town Center. Upgrade pairs reuse one position because only
the currently available member is displayed. Age technologies likewise reuse
position 11.

## Classification

These positions are exact DAT metadata. By themselves they prove placement of
production, research, and age commands, but not runtime-only commands. The
separate screenshot evidence below proves additional visible slots.

The reconstructed command model and SDL renderer now preserve sparse slots.
Town Center production, research, and age-up commands appear directly at these
positions rather than behind production/research proxy buttons. Age-up uses
the existing replayable `AdvanceAgeCommand`; Feudal, Castle, and Imperial
targets select exact technology-sheet frames 30, 31, and 32.

## Runtime screenshot cross-check

The Steam guide
<https://steamcommunity.com/sharedfiles/filedetails/?id=871578718>
contains five full-resolution original-HD Town Center selections (guide images
14, 30, 52, 106, and 112). Across ages and civilizations they consistently
show:

| Grid slot | `btncmd` frame | Command |
|---:|---:|---|
| 0 | DAT unit icon | create Villager |
| 4 | 45 | runtime action; reconstructed as ungarrison |
| 5–7 | DAT technology icon | available Town Center technology |
| 10 | DAT technology icon | advance age |
| 14 | 49 | Town Bell |

Frames 45 and 49 were matched against the complete locally decoded 69-frame
`btncmd` sheet. The visible artwork does not independently prove frame 45's
semantic label, so `ungarrison` remains the reconstruction's best-fit behavior
rather than an exact semantic claim. The screenshots prove that the previous
rally arrow and skull Delete substitutes were visually wrong in these slots.
## Decompiled activation evidence

Read-only `AoK-HD-patched.c` command-panel construction at lines
326805–326840 identifies Town Center DAT ID 109, grid slot 14, action `0xa3`,
help string 41111, and `btncmd` frame 49 while inactive. Object state byte
`+0x161` changes that same command to frame 61 while Town Bell is active.
English string 41111 says nearby work stops, villagers garrison in Town Center,
and ringing again sends them back to work. Separate recall help 41015 names
"Send Villagers Back to Work." Decompiled audio initialization at lines
202492–202502 loads exact command sounds `townbell.wav` and `townrcal.wav`.
`FUN_005a6530` at lines 279876–280107 performs call behavior: bounds are
selected Town Center coordinates ±25; shelter candidates include Town Centers,
building class 52, and Castle ID 82; villagers are repeatedly matched to nearest
shelter while decrementing its free capacity. `FUN_005a68b0` immediately after
it clears alarm state and recalls those occupants.

Reconstruction models this as authoritative `TownBellCommand`: completed,
owned Town Center validation; decompiled 25-coordinate search box; only living
same-owner villagers; nearest eligible Town Center/tower/castle assignment with
capacity reservations; work-return destination preservation; tagged recall that
does not release unrelated manual occupants; blocked-exit retention; and active
frame 61/status feedback. Command, active shelter state, and affected-villager
return state persist through replay, lockstep, and native save version 126.

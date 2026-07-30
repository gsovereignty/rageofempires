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
Town Bell activation semantics are not yet reconstructed; its bell identity,
icon, and slot are visually explicit.

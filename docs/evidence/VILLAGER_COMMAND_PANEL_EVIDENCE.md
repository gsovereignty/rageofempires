# Villager command-panel visual evidence

## Source

An original *Age of Empires II: The Age of Kings* runtime screenshot is
published by ModDB:

`https://www.moddb.com/games/age-of-empires-ii-the-age-of-kings/images/screenshot125`

The page identifies an 800×600 screenshot dated 2011-06-02. Its selected
Villager panel visibly preserves the classic 5×3 command grid.

## Observed layout

The screenshot was compared against installed `btncmd.shp` resource 50721.
Zero-based reconstructed grid slots and matching frames are:

| Grid slot | `btncmd` frame | Command |
|---:|---:|---|
| 0 | 30 | economic buildings |
| 1 | 31 | military buildings |
| 2 | 13 | repair |
| 3 | 59 | delete |
| 4 | 2 | garrison |
| 9 | 3 | stop |

Slots 5–8 and 10–14 are visibly empty. Frame matches use distinctive artwork:
two build-category hammer icons, repair crane, skull, arrow entering a
building, and raised hand.

## Reconstruction contract

- A Villager-only selection uses these exact sparse slots.
- Delete dispatches the existing replayable `DeleteEntityCommand`.
- Mixed selections retain deterministic reconstructed aggregation because this
  screenshot proves only the Villager-only command set.
- Construction submenu contents are outside this screenshot's evidence.

# Scout command-panel visual evidence

## Source

The 2018 3DM article
[`《帝国时代2：高清版》UI界面变化一览`](https://www.3dmgame.com/gl/3745529.html)
publishes paired classic/HD interface captures. Section “六、刚进入游戏”
shows an idle, selected Scout Cavalry in the classic 580×435 HUD.

This repository does not copy the screenshot. Observations below derive from
it and are checked against decoded `btncmd` SLP 50721 artwork.

## Observed idle Scout Cavalry grid

The classic capture has nine populated cells in packed 5×3 order:

| Grid index | `btncmd` frame | Reconstructed command |
|---:|---:|---|
| 0 | 6 | aggressive stance |
| 1 | 1 | attack |
| 2 | 8 | stand ground |
| 3 | 59 | no-attack stance |
| 4 | 2 | garrison |
| 5 | 9 | defensive stance |
| 6 | 10 | guard |
| 7 | 51 | follow |
| 8 | 50 | patrol |

Idle Scout Cavalry has no Stop tile. Capture directly proves artwork and cell
order, but not internal action codes, hotkeys, pressed/disabled chrome,
tooltip text, or behavior after target selection.

Runtime uses reconstruction `attack_move` for observed attack cell because
simulation exposes attack-move targeting rather than separate explicit
attack-target mode. Follow currently shares Guard target execution; distinct
icon, label, and position are preserved while exact non-protective follow
behavior remains future work.

## Regression coverage

- `command_panel_tests` pins idle Scout order and Stop absence.
- `aoe_ui_icon_sdl_smoke` captures villager, Scout Cavalry, and Town Center
  panels independently and rejects identical output.
- Manual SDL comparison uses `AOE_COMMAND_PANEL=scout`.

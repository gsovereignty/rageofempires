# Woad Raider fidelity slice

Packet 1 implements the Celtic Woad Raider family from the supplied
`empires2_x1_p1.dat` and graphics/audio archives. No commercial-format
compatibility claim is implied.

## Authoritative DAT records

- Unit 232: 65 HP, 8 melee attack, 2-tick reload, 1.2 movement, 3 LOS,
  65 food + 25 gold, 10-tick Castle training.
- Unit 534: 80 HP, 13 melee attack, 2-tick reload, 1.2 movement, 5 LOS,
  same training cost/time.
- Both have 0 melee armor, 1 pierce armor, and class bonuses of 2/3 against
  buildings (class 21) and Eagle Warriors (class 29).
- Technology 370 requires Imperial Age and availability technology 277. It
  costs 1000 food + 800 gold, takes 45 ticks, and effect 368 upgrades unit
  232 to 534.
- Civilization owner is DAT civilization 13, Celts.

## Visible and audio bindings

DAT graphic records resolve as follows:

| State | Graphic | SLP | Frames |
|---|---:|---:|---:|
| attack | 1369 | 1592 | 12 |
| death | 1372 | 1595 | 10 |
| idle | 1375 | 1598 | 8 |
| walk | 1379 | 1602 | 12 |

Training icon field 47 binds to ordinary-unit sheet 50730 frame 47 through
the existing exact dispatch contract. DAT sounds bind selected 420, accepted
command 422, movement 421, and training 337.

## Reconstruction coverage

Rules, Celtic availability, Castle production, elite research/upgrading,
Eagle/building bonus damage, AI production, SDL controls/rendering, icon and
audio selection, scenario mapping, save/replay durability, and generated
civilization-matrix coverage are represented. Focused simulation coverage
uses the same Castle unique-family round trip as existing families.

# Standard unit rules and asset map

## Scope and evidence

This map records DAT IDs 751/752, 279/542, 550/588, and 331/42 from the
validated VER 5.7 `empires2_x1_p1.dat`. Rules come from the metadata extractor;
SLP presence comes from legacy DRS lookup. Numeric attack and armor classes are
preserved because naming them without an independently validated class table
would weaken the evidence.

`button_icon` is the in-game command-panel index. All eight records have no
separate portrait index in this DAT. Optional reconstruction assets currently
contain none of the referenced audio resources; numbers below are original DAT
resource IDs, not bundled files.

## Availability and upgrades

| Unit | DAT ID | Gate/upgrade tech | Where, slot | Cost | Time | Available civilizations |
|---|---:|---|---|---|---:|---|
| Eagle Warrior | 751 | 433, Castle Age prerequisite | Barracks 12, slot 4 | 20 food, 50 gold | 35 s | Aztecs, Mayan |
| Elite Eagle Warrior | 752 | 434; Imperial + 433; effect 445 upgrades 751 | Barracks 12, slot 4 | unit cost above; tech 800 food, 500 gold | 20 s; tech 40 s | Aztecs, Mayan |
| Scorpion | 279 | 94, Castle Age; effect 169 enables 279 | Siege Workshop 49, slot 3 | 75 wood, 75 gold | 30 s | all 18 |
| Heavy Scorpion | 542 | 239; Imperial + 94; effect 228 upgrades 279 and projectiles | Siege Workshop 49, slot 3 | unit cost above; tech 1,000 food, 1,100 wood | 30 s; tech 50 s | French, Goths, Germans, Japanese, Chinese, Persians, Turks, Vikings, Mongols, Celts, Mayan |
| Onager | 550 | 257, Imperial; effect 247 upgrades 280 | Siege Workshop 49, slot 2 | 160 wood, 135 gold | 46 s; tech 75 s | British, French, Goths, Germans, Japanese, Chinese, Byzantine, Persians, Saracens, Vikings, Mongols, Celts, Spanish, Aztecs, Mayan, Koreans |
| Siege Onager | 588 | 320; Imperial + 257; effect 320 upgrades 280/550 | Siege Workshop 49, slot 2 | unit cost above; tech 1,450 food, 1,000 gold | 46 s; tech 150 s | Germans, Saracens, Mongols, Celts, Aztecs, Koreans |
| Packed Trebuchet | 331 | 256, Imperial; effect 97 enables 331 | Castle 82, slot 2 | 200 wood, 200 gold, 1 population | 50 s | all 18 |
| Trebuchet | 42 | paired state of 331; same gate | Castle 82; record slot 0 | same | 50 s in record | all 18 |

Tech-panel details: Elite Eagle Warrior is Barracks button 9/icon 115; Scorpion
availability is Siege Workshop button 8/icon 68; Heavy Scorpion is button
8/icon 38; Onager is button 7/icon 57; Siege Onager is button 7/icon 96.
Trebuchet tech 256 is hidden/locationless and has zero research time.

## Unit statistics

| Unit | HP | Speed | LOS | Attack | Range/min | Reload | Accuracy | Frame delay | Blast radius/level | Icon |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|
| Eagle Warrior | 50 | 1.1 | 6 | 4 | melee | 2 | — | — | 0/0 | 109 |
| Elite Eagle Warrior | 60 | 1.3 | 6 | 9 | melee | 2 | — | — | 0/0 | 109 |
| Scorpion | 40 | 0.65 | 9 | 12 | 7/2 | 3.6 | 100 | 7 | 0/0 | 80 |
| Heavy Scorpion | 50 | 0.65 | 9 | 16 | 7/2 | 3.6 | 100 | 7 | 0/0 | 89 |
| Onager | 60 | 0.6 | 10 | 50 | 8/3 | 6 | 100 | — | 1.25/2 | 101 |
| Siege Onager | 70 | 0.6 | 10 | 75 | 8/3 | 6 | 100 | — | 1.5/1 | 102 |
| Packed Trebuchet | 150 | 0.8 | 18 | none | — | — | — | — | — | 29 |
| Trebuchet | 150 | 0 | 18 | 200 | 16/4 | 10 | 15 | 6 | 0/1 | 28 |

Raw weapon classes:

- Eagle 751: `25:8, 4:4, 8:0, 20:3`; Elite 752:
  `25:10, 4:9, 8:4, 16:2, 20:5`.
- Scorpion 279: `11:2, 5:6, 4:0, 3:12, 17:1`; Heavy 542:
  `11:4, 5:8, 4:0, 3:16, 17:2`.
- Onager 550: `11:45, 4:50, 20:12`; Siege 588:
  `11:60, 4:75, 20:12`.
- Packed Trebuchet has no weapon. Unpacked 42: `11:250, 3:200`.

Raw armor classes:

- Eagle 751: `29:0, 1:0, 4:0, 3:2`; Elite 752:
  `29:0, 1:0, 4:0, 3:4`.
- Scorpion 279: `4:0, 3:6, 20:0`; Heavy 542: `4:0, 3:7, 20:0`.
- Onager 550: `4:0, 3:7, 20:0`; Siege 588: `4:0, 3:8, 20:0`.
- Packed 331 has displayed melee/pierce armor 2/8. Unpacked 42 has 1/150
  plus zero-valued classes 17 and 20.

El Dorado tech 4 adds 40 HP to Eagle Warrior records. Onager has missile 656
plus volley missile 369, amount/max 8/10 and spread 1.25. Siege Onager uses
the same missile IDs, amount/max 8/10 and spread 1.5. Both use start-spread
adjustment 99.

## Trebuchet state behavior

Both Trebuchet records expose action work rate 4.5 and search radius 18.
Packed record is mobile and weaponless; unpacked record is immobile and
attacks. Their inherited task lists contain engine action types but no public
paired-unit ID, duration, or transform-sprite link. Therefore exact pack and
unpack seconds cannot be recovered from exposed fields and are not guessed
here.

Kataparuto tech 59 targets unpacked unit 42 only: attribute 13 multiplies work
rate by 4 and attribute 10 multiplies reload by 0.75. Resulting record values
are work rate 18 and reload 7.5. It does not change unit training time or cost,
and does not directly target packed unit 331.

Packed construction graphic 118 (SLP 236) and unpacked construction graphic
119 (SLP 237) each contain one frame over three angles. They are construction
links, not proven transform animations. Graphic 1714 (`TREBU_S1`, SLP 1245,
two frames) references child graphic 1181 (SLP 1246); root SLP 1245 is absent
from inspected DRS while child SLP 1246 is present. This is alternate-state art
evidence, but task data does not prove its timing role.

## Unit animation assets

Format is `graphic/SLP/frames`. Root graphics use palette `-1`, eight
directions, and mirror mode 6 unless noted.

| Unit | Attack | Death | Idle | Walk | Selection |
|---|---|---|---|---|---|
| Eagle and Elite | 5790/4826/10 | 5791/4827/10 | 5792/4828/10 | 5794/4830/15 | 5793/4829/5 |
| Scorpion | 1070/936/10 | 1073/939/10 | 1076/942/1 | 1080/946/10 | 1077/943/5 |
| Heavy Scorpion | 2771/2813/8 | 2774/2816/8 | 2777/2819/1 | 2781/2823/10 | 2778/2820/5 |
| Onager | 2813/3017/10 | 2816/3020/10 | 2819/3023/1 | 2822/3026/10 | 3790/4168/5 |
| Siege Onager | 3159/3553/10 | 3162/3556/10 | 3165/3559/1 | 3169/3563/10 | not mapped |
| Packed Trebuchet | — | 5434/4572/10 | 2281/2279/10 | 2281/2279/10 | 5435/4573/5 |
| Trebuchet | 1174/1237/22 | 1177/1241/12 | 1180/1244/1 | — | 1181/1246/5 |

All table SLP roots are present. Scorpion-family and Onager-family roots are
layered DAGs: attack/death/idle component SLPs are absent but composite root
SLPs are present. Their walk main layers are present (Scorpion 944, Heavy
2821, Onager 3024, Siege 3561), overlay layers are absent, and composite roots
remain present. Eagle and packed Trebuchet rows are direct SLPs. Unpacked
Trebuchet composite roots are present while component SLPs are absent.

## Projectiles and impacts

| Weapon | Unit ID | Flight graphic | Shadow | Impact |
|---|---:|---|---|---|
| Scorpion bolt | 367 | 3391/3812, 1 frame, 18 directions, mirror 13 | 3392/3813 | 1744/416, 10 frames |
| Heavy Scorpion bolt | 627 | 3388/3812, same bolt SLP | 3389/3813 | 1744/416 |
| Onager primary | 656 | 3396/4500, 1 frame | 3397/3818 | 1744/416 |
| Onager volley | 369 | 3385/3986, 10 frames | 3386/3807, 1 frame | DAT-linked impact |
| Trebuchet stone | 371 | 3394/3815, 10 frames | 3395/3816, 10 frames | 4203/4370, 20 frames |

All listed projectile, shadow, and impact SLPs are present. Projectile roots
use palette `-1`.

## Sound links

| Unit/family | Train/select | Attack | Death/idle |
|---|---|---|---|
| Eagle | train 337; select 420, civ resources Aztec 6750/6751/6752 and Mayan 6595/6596/6597 | graphic triggers 312 at delay 3 and 329 at delay 4 | death 294 |
| Scorpion/Heavy | train 337; select 490/resource 6435 | 384 at delay 6, resources 5447–5450 | death 293/resource 5367 |
| Onager | train 337; select 489/resource 6433 | 107 at delay 1, resources 5097/6153/6154 | death 293 |
| Siege Onager | train 337; select 489/resource 6433 | 466 at delay 1, resources 6158/6159/6160 | death 293 |
| Trebuchet states | train/select 291/resource 5366; unpacked command/move 291; packed command/move 484/resource 6424 | unpacked 290 at delay 1, resources 5425/5426/5368 | death 293; unpacked idle 292, resources 5427/5428/5369 |

Scorpion bolt graphic 3391 has sound 405, resources 5471–5474/5570.
Impact graphics 1744 and 4203 use sound 323, resources
5316/5317/5318/5459. Other projectile roots have no recorded graphic sound.

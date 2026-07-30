# Castle unique-unit asset and rule evidence

## Scope

Evidence joins pinned `VER 5.7` civilization, tech, effect, graphic, and sound
records to supplied `graphics.drs`. Unit records exist in all civilization
arrays, but availability techs below are explicitly civilization-locked.
Castle unit 82 trains every listed unit in button slot 1.

## Rules and availability

| Line | Civ lock / availability | Elite upgrade |
|---|---|---|
| Longbowman 8 to Elite 530 | British civilization 1; tech 263, Castle Age + tech 266 | tech 360, civilization 1, Imperial + tech 263; 850 food/850 gold, 60 seconds; effect 358 |
| Throwing Axeman 281 to Elite 531 | French civilization 2; tech 275, Castle Age + tech 266 | tech 363, civilization 2, Imperial + tech 275; 1000 food/850 gold, 45 seconds; effect 361 |
| Huskarl 41 to Elite 555 | Goth civilization 3; tech 446, Castle Age + tech 266 | tech 365, civilization 3; 1200 food/550 gold, 40 seconds; effect 363 |
| Teutonic Knight 25 to Elite 554 | Germans civilization 4; tech 276, Castle Age + tech 266 | tech 364, civilization 4, Imperial + tech 276; 1200 food/600 gold, 50 seconds; effect 362 |

Huskarl data has an unusual lock mismatch: availability tech 446 is named
`Huskarl (make avail)`, while elite tech 365 requires Imperial Age and tech
270, internally named `Berserker (make avail)`, rather than tech 446. This is
recorded as live data, not normalized.

| Unit | HP / speed / LOS | Attack / range / min / reload / accuracy | Cost / train | Button |
|---|---|---|---|---:|
| Longbowman 8 | 35 / 0.96 / 7 | 6 / 5 / 0 / 2 / 70 | 35 wood, 40 gold, 1 population / 19 | 41 |
| Elite Longbowman 530 | 40 / 0.96 / 8 | 7 / 6 / 0 / 2 / 80 | same / 19 | 41 |
| Throwing Axeman 281 | 50 / 0.90 / 5 | 7 / 3 / 0 / 2 / 100 | 55 food, 25 gold, 1 population / 17 | 46 |
| Elite Throwing Axeman 531 | 60 / 0.90 / 6 | 8 / 4 / 0 / 2 / 100 | same / 17 | 46 |
| Huskarl 41 | 60 / 1.05 / 3 | 10 / 0 / 0 / 2 / 100 | 80 food, 40 gold, 1 population / 16 | 50 |
| Elite Huskarl 555 | 70 / 1.05 / 5 | 12 / 0 / 0 / 2 / 100 | same / 16 | 50 |
| Teutonic Knight 25 | 70 / 0.65 / 3 | 12 / 0 / 0 / 2 / 100 | 85 food, 40 gold, 1 population / 12 | 45 |
| Elite Teutonic Knight 554 | 100 / 0.65 / 5 | 17 / 0 / 0 / 2 / 100 | same / 12 | 45 |

Raw class entries:

- Longbow weapon classes are 27:2, 21:0, 3:6/7, 17:0. Armor classes are
  4:0, 15:0, 3:0/1, 19:0.
- Throwing Axeman weapons are 29:1/2, 21:1/2, 4:7/8, 15:0. Armor is
  class 1:0, 4:0/1, 3:0, 19:0.
- Huskarl weapons are 29:2/3, 21:2/3, 4:10/12, 15:6/10. Armor is
  class 1:0, 4:0, 3:6/8, 19:0.
- Teutonic Knight weapons are 29:4, 21:4, 4:12/17. Armor is class 1:0,
  4:5/10, 3:2, 19:0.

Longbow uses missile 511; Throwing Axeman uses missile 515. Melee lines have
no missile. Creation records contain nominal volley amount/max 1 but no
volley missile; this is ordinary single-projectile behavior, not a multishot.

## Graphics and archive availability

Base and Elite units share faction art. Every root uses layer 20,
`player_color=-1`, eight directions, and mirroring mode 6. Each DAG is
`[layer-10 main,-1,layer-20 overlay]`, with zero offsets/display angle `-1`.
Every composite root is present; every listed component SLP is absent.

| Line/state | Root graphic / SLP | Frames | Missing component SLPs |
|---|---|---:|---|
| Longbow fight | 966 / 702 | 15 | 700,701 |
| Longbow dying | 969 / 705 | 15 | 703,704 |
| Longbow standing | 972 / 708 | 15 | 706,707 |
| Longbow walking | 976 / 713 | 15 | 711,712 |
| Throwing Axeman fight | 1122 / 1051 | 16 | 1049,1050 |
| Throwing Axeman dying | 1125 / 1054 | 13 | 1052,1053 |
| Throwing Axeman standing | 1128 / 1057 | 10 | 1055,1056 |
| Throwing Axeman walking | 1132 / 1061 | 15 | 1059,1060 |
| Huskarl fight | 823 / 4537 | 10 | 503,504 |
| Huskarl dying | 826 / 4538 | 10 | 505,506 |
| Huskarl standing | 829 / 4539 | 6 | 507,508 |
| Huskarl walking | 833 / 4541 | 10 | 509,510 |
| Teutonic Knight fight | 1161 / 1188 | 15 | 1186,1187 |
| Teutonic Knight dying | 1164 / 1191 | 15 | 1189,1190 |
| Teutonic Knight standing | 1167 / 1194 | 10 | 1192,1193 |
| Teutonic Knight walking | 1171 / 1198 | 10 | 1196,1197 |

Direct composite rendering is complete. Delta-only reconstruction is
unavailable for every line.

## Projectiles and sounds

Longbow missile 511 uses graphic 3378 `M_ARRO_R`, SLP 3799, layer 30,
11 frames/32 directions/mirroring 24, graph `[-1,3379]`. Shadow 3379 uses
SLP 3800, layer 10, one frame/72 directions/mirroring 54.

Throwing-axe missile 515 uses graphic 3380 `M_AXEX_R`, SLP 3801, layer 30,
10 frames/eight directions/mirroring 6, graph `[-1,3381]`. Shadow 3381 uses
SLP 3802, layer 10 with matching animation metadata. All missile SLPs are
present and have `player_color=-1`. Neither missile has an impact graphic.

All units use train sound 337/resource 5423 and selected sound 420.
Sound 420 selects civilization-specific voices: British resources
6240/6241/6242, French 6092/6093/6094, Goth 5641/5642/5643, and German
5900/5901/5902 at 33/34/33 percent.

All deaths use sound 294, resources 5309/5312/5313/5314 at 15 percent and
5310/5311 at 20 percent. Longbow fight triggers sound 312 at frame delay 5
and 314 at delay 7 for every direction. Huskarl triggers 312 at delay 3 and
329 at delay 4. Teutonic Knight triggers 329 at delay 7. Both missile
graphics use sound 405/resources 5471/5472/5473/5474/5570 at 20 percent.
Throwing Axeman root has no graphic sound trigger.

Current optional assets contain none of these audio resources; only IDs and
probabilities are available.

## Samurai, Chu Ko Nu, Cataphract, and War Elephant

| Line | Civilization lock / availability | Elite upgrade |
|---|---|---|
| Samurai 291 to Elite 560 | Japanese civilization 5; tech 262, Castle Age + tech 266 | tech 366, civilization 5, Imperial + tech 262; 950 food/875 gold, 60 seconds; effect 364 |
| Chu Ko Nu 73 to Elite 559 | Chinese civilization 6; tech 268, Castle Age + tech 266 | tech 362, civilization 6, Imperial + tech 268; 950 food/950 gold, 50 seconds; effect 360 |
| Cataphract 40 to Elite 553 | Byzantine civilization 7; tech 267, Castle Age + tech 266 | tech 361, civilization 7, Imperial + tech 267; 1600 food/800 gold, 50 seconds; effect 359 |
| War Elephant 239 to Elite 558 | Persian civilization 8; tech 274, Castle Age + tech 266 | tech 367, civilization 8, Imperial + tech 274; 1600 food/1200 gold, 75 seconds; effect 365 |

All train at Castle 82, button slot 1.

| Unit | HP / speed / LOS | Attack / range / reload / accuracy | Cost / train | Icon |
|---|---|---|---|---:|
| Samurai 291 | 60 / 1.00 / 4 | 8 / 0 / 1.9 / 100 | 60 food, 30 gold, 1 population / 9 | 44 |
| Elite Samurai 560 | 80 / 1.00 / 5 | 12 / 0 / 1.9 / 100 | same / 9 | 44 |
| Chu Ko Nu 73 | 45 / 0.96 / 6 | 8 / 4 / 3 / 85 | 40 wood, 35 gold, 1 population / 19 | 36 |
| Elite Chu Ko Nu 559 | 50 / 0.96 / 6 | 8 / 4 / 3 / 85 | same / 13 | 36 |
| Cataphract 40 | 110 / 1.35 / 4 | 9 / 0 / 1.8 / 100 | 70 food, 75 gold, 1 population / 20 | 35 |
| Elite Cataphract 553 | 150 / 1.35 / 5 | 12 / 0 / 1.7 / 100 | same / 20 | 35 |
| War Elephant 239 | 450 / 0.60 / 4 | 15 / 0 / 2 / 100 | 200 food, 75 gold, 1 population / 31 | 43 |
| Elite War Elephant 558 | 600 / 0.60 / 5 | 20 / 0 / 2 / 100 | same / 31 | 43 |

Raw class mechanics:

- Samurai weapon classes 29/21 are 2 base, 3 Elite; class 4 is 8/12 and
  unique-unit class 19 is 10/12. Armor is class 1:0, 4:1, 3:1, 19:0.
- Chu Ko Nu weapons are class 27:2, 21:0, 4:0, 8:0, 3:8, 17:0; all armor
  entries are zero. Both use primary and volley missile 510. Base volley
  amount/max is 3 with spread `(0,0)`; Elite is 5 with spread `(1,1)`.
  Start-spread adjustment is 1. Volley arrows must not each receive full
  listed primary damage without combat-rule evidence.
- Cataphract weapons are class 1 and 4 at 9/12, class 15:0. Armor is
  class 4:2, 8:12/16, 3:1, 19:0. Both have area-effect radius 0 and blast
  level 2: no intrinsic splash/trample radius is encoded in these unit
  records.
- War Elephant weapons are class 11:7/10, 4:15/20, 13:7/10. Armor is
  class 5:0, 4:1, 8:0, 3:2/3, 19:0. Base area-effect radius is 0; Elite
  radius is 0.5 with blast level 2. Thus splash/trample is Elite-only in
  these records.

### Graphics

Base and Elite share faction art. Every graphic has `player_color=-1`.
Roots use layer 20, eight directions, mirroring 6. DAG shape is
`[layer-10 main,-1,layer-20 overlay]`; all edges use `(0,0)` and display
angle `-1`. Every root is present; every component SLP below is absent.

| Line/state | Root graphic / SLP | Frames | Missing component SLPs |
|---|---|---:|---|
| Samurai fight | 1083 / 974 | 10 | 972,973 |
| Samurai dying | 1086 / 977 | 10 | 975,976 |
| Samurai standing | 1089 / 980 | 10 | 978,979 |
| Samurai walking | 2533 / 984 | 10 | 982,983 |
| Chu Ko Nu fight | 719 / 215 | 10 | 213,214 |
| Chu Ko Nu dying | 722 / 218 | 10 | 216,217 |
| Chu Ko Nu standing | 725 / 221 | 10 | 219,220 |
| Chu Ko Nu walking | 729 / 225 | 15 root; 10 components | 223,224 |
| Cataphract fight | 706 / 199 | 10 | 197,198 |
| Cataphract dying | 709 / 202 | 10 | 200,201 |
| Cataphract standing | 712 / 205 | 10 | 203,204 |
| Cataphract walking | 716 / 209 | 10 | 207,208 |
| War Elephant fight | 1018 / 795 | 7 | 793,794 |
| War Elephant dying | 1021 / 798 | 15 | 796,797 |
| War Elephant standing | 1024 / 801 | 7 | 799,800 |
| War Elephant walking | 1028 / 805 | 10 | 803,804 |

Direct composite rendering is complete; delta-only reconstruction is
unavailable.

Chu missile 510 uses the same complete arrow graph 3378/SLP 3799 and
3379/SLP 3800 documented above. It has no impact graphic. Other lines have no
missiles.

### Sounds

All use train sound 337. Samurai and Chu use selected sound 420 with Japanese
resources 5552/5553/5554 and Chinese 5710/5711/5712 respectively. Cataphract
selection sound 325 uses 5343/5344/5345 at 25/50/25 percent. War Elephant
selection sound 477 uses resource 6197.

- Samurai fight triggers sound 329 at delays 4 and 7; death sound 294.
- Chu fight triggers 314 at delay 5; missile sound 405; death 294.
- Cataphract fight triggers 329 at delay 7. Death triggers 410 at delay 1
  and 413 at delay 6.
- War Elephant fight has base sound 26/resource 6415 and angle sound 419 at
  delay 3. Death triggers 482/resource 6417 at delay 1 and
  481/resource 6416 at delay 4.

Current optional assets contain none of these audio resources.

## Jaguar Warrior, Plumed Archer, Conquistador, and Tarkan

| Line | Civilization lock / availability | Elite upgrade |
|---|---|---|
| Jaguar Warrior 725 to Elite 726 | Aztec civilization 15; tech 431, Castle Age; internal name `Jaguar Man` | tech 432, civilization 15, Imperial + tech 431; 1000 food/500 gold, 45 seconds; effect 444 |
| Plumed Archer 763 to Elite 765 | Mayan civilization 16; tech 26, Castle Age + tech 266 | tech 27, civilization 16, Imperial + tech 26; 500 food/1000 wood, 45 seconds; effect 469 |
| Conquistador 771 to Elite 773 | Spanish civilization 14; tech 58, Castle Age + tech 266 | tech 60, civilization 14, Imperial + tech 58; 1200 food/600 gold, 60 seconds; effect 492 |
| Tarkan 755 to Elite 757 | Hun civilization 17; tech 1, Castle Age | tech 2, civilization 17, Imperial + tech 1; 1000 food/500 gold, 45 seconds; effect 454 |

Availability techs have no direct command list; their `time2` links are
442, 468, 491, and 453 respectively. All four lines train at Castle 82,
button slot 1.

| Unit | Class | HP / armor / speed / LOS | Attack / range / min / reload / accuracy | Cost / train | Icon |
|---|---:|---|---|---|---:|
| Jaguar Warrior 725 | 6 | 50 / 1 melee, 0 pierce / 1.00 / 3 | 10 / 0 / 0 / 2 / 100 | 60 food, 30 gold, 1 population / 20 | 110 |
| Elite Jaguar Warrior 726 | 6 | 75 / 2 melee, 0 pierce / 1.00 / 5 | 12 / 0 / 0 / 2 / 100 | same / 20 | 110 |
| Plumed Archer 763 | 0 | 50 / 0 melee, 1 pierce / 1.20 / 6 | 5 / 4 / 0 / 1.9 / 80 | 46 wood, 46 gold, 1 population / 16 | 108 |
| Elite Plumed Archer 765 | 0 | 65 / 0 melee, 2 pierce / 1.20 / 7 | 5 / 5 / 0 / 1.9 / 90 | same / 16 | 108 |
| Conquistador 771 | 23 | 55 / 2 melee, 2 pierce / 1.30 / 8 | 16 / 6 / 0 / 2.9 / 65 | 60 food, 70 gold, 1 population / 24 | 106 |
| Elite Conquistador 773 | 23 | 70 / 2 melee, 2 pierce / 1.30 / 9 | 18 / 6 / 0 / 2.9 / 70 | same / 24 | 106 |
| Tarkan 755 | 12 | 90 / 1 melee, 2 pierce / 1.35 / 5 | 7 / 0 / 0 / 2.1 / 100 | 60 food, 60 gold, 1 population / 14 | 105 |
| Elite Tarkan 757 | 12 | 150 / 1 melee, 3 pierce / 1.35 / 7 | 11 / 0 / 0 / 2.1 / 100 | same / 14 | 105 |

No unit has a portrait-icon link. Raw class mechanics:

- Jaguar weapons are anti-infantry classes 29:2, 21:2, and 1:10 for both
  forms, plus melee class 4:10/12 and class 8:0. Armor entries are class
  1:0, class 4:1/2, class 3:0, class 19:0. Class-1 attack is the explicit
  anti-infantry bonus evidence.
- Plumed weapons are archer class 27:2, class 21:0, anti-infantry class
  1:1/2, pierce class 3:5, and gunpowder class 17:0. Armor is class 4:0,
  class 15:0, class 3:1/2, class 19:0. Speed 1.2 and the class-1 bonus
  encode its mobility/anti-infantry profile. Both use missile 511 and
  frame delay 5.
- Conquistador weapons are class 11:0/2, pierce class 3:16/18, and
  gunpowder class 17:4/6. Armor is class 4:2, class 15:0, class 8:0,
  class 3:2, and class 19:0. Both use gunshot missile 380 and frame delay
  4. Area radius is 0 and blast level 3; nominal volley amount/max is 1.
- Tarkan weapons are building classes 26:10, 13:12, and 22:8/10, class
  11:8/10, melee class 4:7/11, and class 15:0. Armor is class 4:1,
  class 8:0, class 3:2/3, and class 19:0. These three explicit building
  class entries preserve its structure bonus without collapsing targets
  into one invented category.

### Graphics and projectiles

Base and Elite share line art. All action roots are direct SLPs with no delta
children, layer 20, `player_color=-1`, eight directions, and mirroring 6.
Every listed root SLP is present.

| Line/state | Root graphic / SLP | Frames |
|---|---|---:|
| Jaguar fight | 6599 / 4858 | 10 |
| Jaguar dying | 6600 / 4859 | 10 |
| Jaguar standing | 6601 / 4860 | 10 |
| Jaguar walking | 6603 / 4862 | 15 |
| Plumed Archer fight | 6621 / 4871 | 15 |
| Plumed Archer dying | 6622 / 4872 | 10 |
| Plumed Archer standing | 6623 / 4873 | 10 |
| Plumed Archer walking | 6625 / 4875 | 15 |
| Conquistador fight | 5730 / 4716 | 14 |
| Conquistador dying | 5733 / 4719 | 14 |
| Conquistador standing | 5736 / 4722 | 14 |
| Conquistador walking | 5740 / 4726 | 14 |
| Tarkan fight | 6439 / 4916 | 14 |
| Tarkan dying | 6440 / 4917 | 14 |
| Tarkan standing | 6441 / 4918 | 14 |
| Tarkan walking | 6443 / 4920 | 10 |

Plumed missile 511 uses arrow graphic 3378/SLP 3799, layer 30,
`player_color=-1`, 11 frames/32 directions/mirroring 24. Its shadow child
3379/SLP 3800 is layer 10, one frame/72 directions/mirroring 54. It has no
impact graphic.

Conquistador missile 380 uses graphic 3396/SLP 4500 at layer 30 plus shadow
3397/SLP 3818 at layer 10; both are one frame/one direction/mirroring 0,
`player_color=-1`. Its dying/impact graphic is 5463/SLP 4370, layer 30,
20 frames/one direction/mirroring 0. Every projectile and impact SLP is
present. Jaguar and Tarkan have no missile.

### Sounds

All train with sound 337/resource 5423. Jaguar, Plumed Archer, and
Conquistador use selection sound 420: Aztec resources 6750/6751/6752 at
33/34/33 percent, Mayan 6595/6596/6597 at 34/33/33, and Spanish
6691/6692/6693 at 33/34/33. Tarkan uses selection sound 325, resources
5343/5344/5345 at 25/50/25.

- Jaguar fight triggers sound 312 at delay 3 and sound 329 at delay 4 for
  every direction. Dying uses sound 294.
- Plumed fight has no root trigger. Arrow graphic 3378 has base sound 405.
  Dying uses sound 294.
- Conquistador fight triggers gunshot sound 385 at delay 2 for every
  direction. Dying uses sound 294.
- Tarkan fight triggers sound 497 at delay 7. Dying triggers sound 410 at
  delay 1 and sound 413 at delay 6; it does not use sound 294.

Sound resources: 312 uses 5327/5461/5329/5330/5331; 329 uses
5358/5359/5360/5361/5362/5466/5467/5468; 405 uses
5471/5472/5473/5474/5570; 385 uses 5451/5452/5453; 497 uses
6450/6451/6452; 410 uses 5478/5479/5480; 413 uses 5490. Sound 294
resources are documented above. Current optional assets contain none of
these audio resources.

## Mameluke, Janissary, Berserk, and Mangudai

| Line | Civilization lock / availability | Elite upgrade |
|---|---|---|
| Mameluke 282 to Elite 556 | Saracen civilization 9; tech 269, Castle Age + tech 266 | tech 368, civilization 9, Imperial + tech 269; 600 food/500 gold, 50 seconds; effect 366 |
| Janissary 46 to Elite 557 | Turk civilization 10; tech 271, Castle Age + tech 266 | tech 369, civilization 10, Imperial + tech 271; 850 food/750 gold, 55 seconds; effect 367 |
| Berserk 692 to Elite 694 | Viking civilization 11; tech 399, Castle Age | tech 398, civilization 11, Imperial + tech 399; 1300 food/550 gold, 45 seconds; effect 397 |
| Mangudai 11 to Elite 561 | Mongol civilization 12; tech 273, Castle Age + tech 266 | tech 371, civilization 12, Imperial + tech 273; 1100 food/675 gold, 50 seconds; effect 369 |

Live internal names retain `Jannisary` for techs 271/369 and call Mangudai
availability tech 273 `Mobile Siege Unit (make avail)`. All train at Castle 82,
button slot 1.

| Unit | Class | HP / speed / LOS | Attack / range / reload / accuracy | Cost / train | Icon |
|---|---:|---|---|---|---:|
| Mameluke 282 | 12 | 65 / 1.40 / 5 | 7 / 3 / 2 / 100 | 55 food, 85 gold, 1 population / 23 | 37 |
| Elite Mameluke 556 | 12 | 80 / 1.40 / 5 | 10 / 3 / 2 / 100 | same / 23 | 37 |
| Janissary 46 | 44 | 35 / 0.96 / 10 | 17 / 8 / 3.45 / 50 | 60 food, 55 gold, 1 population / 21 | 39 |
| Elite Janissary 557 | 44 | 40 / 0.96 / 10 | 22 / 8 / 3.45 / 50 | same / 21 | 39 |
| Berserk 692 | 6 | 48 / 1.05 / 3 | 9 / 0 / 2 / 100 | 65 food, 25 gold, 1 population / 16 | 38 |
| Elite Berserk 694 | 6 | 60 / 1.05 / 5 | 14 / 0 / 2 / 100 | same / 16 | 38 |
| Mangudai 11 | 36 | 60 / 1.45 / 6 | 6 / 4 / 2.1 / 95 | 55 wood, 65 gold, 1 population / 26 | 42 |
| Elite Mangudai 561 | 36 | 60 / 1.45 / 6 | 8 / 4 / 2.1 / 95 | same / 26 | 42 |

Raw class and special-mechanic evidence:

- Mameluke weapons are class 11:0, class 4:7/10, and class 8:9/12. Armor
  is class 4:0/1, class 8:11, and classes 15/3/16/19:0. It uses missile
  736 despite unit class 12 and ranged attack, preserving its unusual
  cavalry/ranged-melee class behavior rather than treating it as an archer.
- Janissary weapons are class 11:0, class 3:17/22, and gunpowder class
  17:2/3. Armor is class 4:1/2 and classes 15/3/19:0. Missile 380 is the
  gunshot projectile. Base frame delay is 4; Elite delay is 0. Both have
  nominal single-shot volley settings. Both have area radius 0; blast level
  is 3 base and 0 Elite, recorded without inferring splash.
- Berserk weapons are class 29:2/3, class 21:2/3, class 4:9/14, and class
  8:0. Armor is class 1:0, class 4:0/2, class 3:1, and class 19:0.
  Berserkergang tech 49 costs 500 food/850 gold and takes 40 seconds. Its
  effect 467 is one type-6 command targeting resource 96 with `d=0.5`.
  This is exact regeneration-system evidence; tick units and resulting
  HP/second are not encoded by these records, so no rate is invented.
- Mangudai weapons are class 27:1, class 21:0, class 3:6/8, class 17:0,
  and siege class 20:3/5. Armor is class 28:0, class 4:0/1, and classes
  15/8/3/19:0. The explicit class-20 increase is the Elite siege-bonus
  upgrade. Both use missile 477.

### Graphics and projectiles

All unit roots use layer 20, `player_color=-1`, eight directions, and
mirroring 6. Every DAG is `[layer-10 main,-1,layer-20 overlay]` with zero
offsets/display angle `-1`. Root composite SLPs are present; component SLPs
are absent. Mameluke, Janissary, and Mangudai share art across upgrade.
Berserk and Elite Berserk use distinct art.

| Line/state | Root graphic / SLP | Frames | Missing component SLPs |
|---|---|---:|---|
| Mameluke fight | 771 / 351 | 10 | 349,350 |
| Mameluke dying | 774 / 354 | 10 | 352,353 |
| Mameluke standing | 777 / 357 | 6 | 355,356 |
| Mameluke walking | 781 / 361 | 10 | 359,360 |
| Janissary fight | 901 / 634 | 10 | 632,633 |
| Janissary dying | 904 / 637 | 10 | 635,636 |
| Janissary standing | 907 / 640 | 10 | 638,639 |
| Janissary walking | 911 / 644 | 10 | 642,643 |
| Berserk fight | 4235 / 4386 | 10 | 4384,4385 |
| Berserk dying | 4238 / 4389 | 10 | 4387,4388 |
| Berserk standing | 4241 / 4392 | 6 | 4390,4391 |
| Berserk walking | 4245 / 4396 | 12 | 4394,4395 |
| Elite Berserk fight | 4222 / 4373 | 10 | 4371,4372 |
| Elite Berserk dying | 4225 / 4376 | 10 | 4374,4375 |
| Elite Berserk standing | 4228 / 4379 | 6 | 4377,4378 |
| Elite Berserk walking | 4232 / 4383 | 12 | 4381,4382 |
| Mangudai fight | 1005 / 782 | 13 | 780,781 |
| Mangudai dying | 1008 / 785 | 10 | 783,784 |
| Mangudai standing | 1011 / 788 | 10 | 786,787 |
| Mangudai walking | 1015 / 792 | 10 | 790,791 |

Mameluke missile 736 uses graphic 5439/SLP 4583, layer 30, ten frames/eight
directions/mirroring 6, with no impact graphic. Janissary missile 380 uses
graphic 3396/SLP 4500, layer 30, one frame/one direction, plus shadow
3397/SLP 3818 at layer 10. Its dying/impact graphic is 5463/SLP 4370,
20 frames/one direction. Mangudai missile 477 uses complete arrow graph
3378/SLP 3799 and 3379/SLP 3800 documented above, with no impact graphic.
Every listed projectile SLP is present.

### Sounds

All use train sound 337/resource 5423 and death sound 294. Selection sound
420 resolves by civilization: Saracen 5676/5677/5678, Turk
6296/6295/6294, Viking 6278/6279/6280, and Mongol 5988/5989/5990 at
33/33/34 percent except Mongol at 34/33/33. Unit records have no portrait
icon; shared button-icon indices are 37, 39, 38, and 42 respectively.

- Mameluke fight has base sound 486, resources 6426/6427/6428 at
  34/33/33 percent.
- Janissary fight triggers sound 385 at delay 2 in every direction,
  resources 5451/5452/5453 at 34/33/33 percent.
- Both Berserk fight roots trigger sound 312 at delay 3 and sound 329 at
  delay 4. Sound 312 resources are 5327/5461/5329/5330/5331; sound 329
  resources are 5358/5359/5360/5361/5362/5466/5467/5468. Elite standing
  triggers sound 333 at delay 3 and Elite walking at delay 5; base
  standing/walking have no trigger. Sound 333 resources are
  5370/5371/5372/5373.
- Mangudai fight triggers sound 314 at delay 7. Its arrow graphic has base
  sound 405. Sound 314 resources are 5286/5287/5288/5289/5570; sound 405
  resources are 5471/5472/5473/5474/5570.

Current optional assets contain none of these audio resources.

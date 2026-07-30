# Naval unit asset and rule evidence

## Scope

This map joins the live `VER 5.7` civilization/effect records to
`LegacyDatFile` graphic metadata and the supplied `graphics.drs`. Family order
is E/F/M/W/X as defined in `BUILDING_SPRITE_MAP.md`. All 19 civilizations
agree within their architecture family.

Unless noted otherwise, every graphic and delta node below has
`player_color=-1`; every delta edge uses `(0,0)` and display angle `-1`.
“Present” means the exact SLP resource exists in the supplied archive.

## Unit and rule chain

| Unit | Age/role | HP | Speed | LOS | Attack / range / reload | Cost | Train |
|---:|---|---:|---:|---:|---|---|---:|
| 539 Galley | Feudal base | 120 | 1.43 | 7 | 6 / 5 / 3 | 90 wood, 30 gold, 1 population | 60 |
| 21 War Galley | Castle replacement | 135 | 1.43 | 8 | 7 / 6 / 3 | 90 wood, 30 gold, 1 population | 36 |
| 442 Galleon | Imperial replacement | 165 | 1.43 | 9 | 8 / 7 / 3 | 90 wood, 30 gold, 1 population | 36 |
| 545 Transport Ship | noncombat transport | 100 | 1.45 | 5 | none; displayed armor 4 | 125 wood, 1 population | 46 |

Costs are DAT resource IDs 1 wood, 3 gold, and flag-0 ID 4 population.
All four create at Dock unit 45. Galley-line create-button slot is 4;
Transport slot is 2. Galley-line accuracy is 100 and displayed armor is 0.
The live effect commands prove `539 -> 21` in effect 155 and both
`539 -> 442` and `21 -> 442` in effect 170. Tech catalog entries are 34
`War Galley` and 35 `Galleon`.

Button-picture indices are Dock 13, Galley 87, War Galley 25, Galleon 60, and
Transport Ship 95. These values are interface-atlas indices, not DRS resource
IDs. No table in the current parser proves their atlas SLP/frame mapping, so
icon archive availability remains unresolved rather than treating these
numbers as SLP IDs.

## Root graphics

Roots use layer 20 and `player_color=-1`. Active-state root SLPs are shared
IDs `2219,2220,2260,2263,5156` for E/F/M/W/X and are all absent. Their full
recursive delta DAGs are present, so renderer must compose them.

| Unit/state | E/F/M/W/X root graphics | Frames / directions / mirror |
|---|---|---|
| Galley fight | `4045,4046,4047,4048,7025` | 1 / 16 / 12 |
| Galley standing | `4049,4050,4051,4052,7026` | 1 / 16 / 12 |
| Galley walking | `4053,4054,4055,4056,7027` | 1 / 16 / 12 |
| War Galley fight | `4006,4007,4008,4009,6770` | 10 / 16 / 12 |
| War Galley standing | `4010,4011,4012,4013,6771` | 10 / 16 / 12 |
| War Galley walking | `4014,4015,4016,4017,6772` | 10 / 16 / 12 |
| Galleon fight | `3882,3883,3884,3885,7102` | 10 / 16 / 12 |
| Galleon standing | `3886,3887,3888,3889,7103` | 10 / 16 / 12 |
| Galleon walking | `3890,3891,3892,3893,7104` | 10 / 16 / 12 |
| Transport fight field | `4086,4087,4088,4089,7161` | 1 / 16 / 12 |
| Transport standing | `4090,4091,4092,4093,7162` | 1 / 16 / 12 |
| Transport walking | `4094,4095,4096,4097,7163` | 1 / 16 / 12 |

Transport has no weapon or missile. Its non-null `fight_sprite` is recorded
as a DAT field link, not evidence of an attack capability.

## Complete active-state delta DAGs

Lists below are direct children in DAT order. Family-vector positions remain
E/F/M/W/X. Every referenced component SLP is present.

### Galley

Shared node 4057 `SGAL_1H`, SLP 4300, layer 20, has child 2766
`SGALY_W0`, SLP 4515, layer 10. Family sail nodes are
`3934,3935,3936,3937,6685`, SLPs `4224,4225,4226,4227,4935`, layer 20.
Action body 2757, standing body 2763, and walking body 2766 all use SLP 4515
and layer 10.

| State | Direct child graph |
|---|---|
| Fight | `[4057,family-sail,2757]` |
| Standing | `[4057,family-sail,2763]` |
| Walking | `[4057,family-sail,2766]` |

### War Galley

Shared node 4018 `GALY_1H`, SLP 4256, layer 20, has child 818
`GALLY_W0`, SLP 4508, layer 10. Family component vectors:

- C2: `4062–4065,7030`, SLPs `4305–4308,5094`.
- moving sail: `4058–4061,7029`, SLPs `4301–4304,5093`.
- standing sail: `5454–5457,7028`, SLPs `4598–4601,5092`.
- C8: `4082–4085,7035`, SLPs `4325–4328,5099`.

All family components are layer 20. Bodies 808 fight, 814 standing, and 818
walking use SLP 4508 and layer 10.

| State | Direct child graph |
|---|---|
| Fight | `[4018,C2,moving-sail,C8,808]` |
| Standing | `[4018,C2,standing-sail,C8,814]` |
| Walking | `[4018,C2,moving-sail,C8,818]` |

### Galleon

Shared node 3894 `WARG_1H`, SLP 4199, layer 20, has child 1895
`WARGA_A0`, SLP 4516, layer 10. Family layer-20 component vectors:

- C2 `4062–4065,7030`, SLPs `4305–4308,5094`;
- C3 `4066–4069,7031`, SLPs `4309–4312,5095`;
- C4 `4070–4073,7032`, SLPs `4313–4316,5096`;
- moving sail `4058–4061,7029`, SLPs `4301–4304,5093`;
- standing sail `5454–5457,7028`, SLPs `4598–4601,5092`;
- C6 `4074–4077,7033`, SLPs `4317–4320,5097`;
- C7 `4078–4081,7034`, SLPs `4321–4324,5098`;
- C8 `4082–4085,7035`, SLPs `4325–4328,5099`.

Bodies 1895 fight, 1901 standing, and 1905 walking use SLP 4516, layer 10.

| State | Direct child graph |
|---|---|
| Fight | `[3894,C2,C3,C4,moving-sail,C6,C7,C8,1895]` |
| Standing | `[3894,C2,C3,C4,standing-sail,C6,C7,C8,1901]` |
| Walking | `[3894,C2,C3,C4,moving-sail,C6,C7,C8,1905]` |

### Transport Ship

Shared node 4098 `XPRT_1H`, SLP 4331, layer 20, has child 2784
`XPORT_A0`, SLP 4517, layer 10. It reuses Galleon family components C2, C3,
C7, and C8 listed above. Bodies 2784 fight, 2790 standing, and 2793 walking
use SLP 4517, layer 10.

| State | Direct child graph |
|---|---|
| Fight field | `[4098,C2,C3,C7,C8,2784]` |
| Standing | `[4098,C2,C3,C7,C8,2790]` |
| Walking | `[4098,C2,C3,C7,C8,2793]` |

## Death graphics

Death roots are directly renderable. Their layer-10 main nodes are present;
their layer-20 overlays are absent. Delta-only reconstruction is therefore
incomplete but unnecessary when using root SLP.

| Unit | Root graphic / SLP | Frames / directions / mirror | DAG | Missing overlay SLP |
|---|---|---|---|---:|
| Galley | 2762 / 2116 | 6 / 1 / 0 | `[2760,-1,2761]` | 2963 |
| War Galley | 813 / 495 | 6 / 1 / 0 | `[811,-1,812]` | 2939 |
| Galleon | 1900 / 1834 | 6 / 1 / 0 | `[1898,-1,1899]` | 2971 |
| Transport Ship | 2789 / 2116 | 6 / 1 / 0 | `[2787,-1,2788]` | 2983 |

Main SLPs are 4515, 4508, 4516, and 4517 respectively. All death nodes use
`player_color=-1`.

## Projectiles

| Source | Missile unit | Root graphic / SLP | Frames / directions / mirror | Delta | Archive |
|---|---:|---|---|---|---|
| Galley | 540 | 3378 `M_ARRO_R` / 3799 | 11 / 32 / 24 | `[-1,3379]`, SLP 3800 | root and delta present |
| War Galley | 372 | 3391 `M_LBOL_R` / 3812 | 1 / 18 / 13 | `[-1,3392]`, SLP 3813 | root and delta present |
| Galleon | 373 | 3388 `M_HBOL_R` / 3812 | 1 / 18 / 13 | `[-1,3389]`, SLP 3813 | root and delta present |

Projectile roots use layer 20. Delta nodes use layer 10. All have
`player_color=-1`. Missile units 372 and 373 link dying graphic 1744
`EXPL1_NN`, SLP 416, layer 20, 10 frames, one direction, mirroring mode 0;
it is present. Missile 540 has no dying graphic.

## Classic fire and demolition lines

This `VER 5.7` dataset has no unit 1103 Fire Galley and no unit 1104
Demolition Raft. It contains the classic two-level lines:

- Castle-age Demolition Ship 527 to Imperial Heavy Demolition Ship 528,
  proved by effect 233 and tech 244 `Heavy Demolition`;
- Castle-age Fire Ship 529 to Imperial Fast Fire Ship 532, proved by effect
  235 and tech 246 `Fast Fire Ship`.

Tech 242 is `Make Demolition ship available`. Tech 243 is internally named
`Make Fire Galley Avail`, but it does not imply a Fire Galley unit: no such
unit record exists in any civilization in this file.

| Unit | HP / speed / LOS | Displayed attack / range / reload | Raw weapon classes | Armor classes | Cost / train / Dock slot |
|---|---|---|---|---|---|
| 527 Demolition Ship | 50 / 1.60 / 6 | 110 / 0 / 5 | class 11: 220; class 4: 110 | class 16: 3; class 4: 0; class 3: 3 | 70 wood, 50 gold, 1 population / 31 / 21 |
| 528 Heavy Demolition Ship | 60 / 1.60 / 6 | 140 / 0 / 5 | class 11: 280; class 4: 140 | class 16: 5; class 4: 0; class 3: 5 | 70 wood, 50 gold, 1 population / 31 / 21 |
| 529 Fire Ship | 100 / 1.35 / 5 | 2 / 2.49 / 0.25 | class 11: 2; 16: 3; 2: 2; 4: 1; 3: 2 | class 16: 5; class 4: 0; class 3: 6 | 75 wood, 45 gold, 1 population / 36 / 22 |
| 532 Fast Fire Ship | 120 / 1.43 / 6 | 3 / 2.49 / 0.25 | class 11: 3; 16: 4; 2: 3; 4: 1; 3: 3 | class 16: 7; class 4: 0; class 3: 8 | 75 wood, 45 gold, 1 population / 36 / 22 |

Both demolition ships have accuracy 100 and no missile unit. Both fire ships
have DAT accuracy 0 and use missile unit 676.

### State roots and availability

Family order is E/F/M/W/X. Roots use layer 20, 16 directions, mirroring mode
12, and `player_color=-1`. All active roots reference absent shared root SLPs
`2219,2220,2260,2263,5156`, except Demolition Ship's shared fight root 4173
has no SLP at all. Every recursive active-state component SLP is present.

| Unit/state | E/F/M/W/X graphic roots | Frames |
|---|---|---:|
| Demolition Ship fight | 4173 shared | 1 |
| Demolition Ship standing | `4036,4037,4038,4039,6974` | 10 |
| Demolition Ship walking | `4040,4041,4042,4043,6975` | 10 |
| Heavy Demolition fight | `4152,4153,4154,4155,6738` | 1 |
| Heavy Demolition standing | `3985,3986,3987,3988,6739` | 10 |
| Heavy Demolition walking | `3989,3990,3991,3992,6740` | 10 |
| Fire Ship fight | `4175,4176,4177,4178,6767` | 1 |
| Fire Ship standing | `4179,4180,4181,4182,6768` | 1 |
| Fire Ship walking | `4001,4002,4003,4004,6769` | 10 |
| Fast Fire Ship fight | `4019,4020,4021,4022,6901` | 1 |
| Fast Fire Ship standing | `4023,4024,4025,4026,6902` | 1 |
| Fast Fire Ship walking | `4027,4028,4029,4030,6903` | 10 |

All edges below use `(0,0)` and display angle `-1`.

- Demolition Ship: shared node 4044/SLP 4299 has child body 2752/SLP
  4514. Fight is `[4044,-1,2743]`; standing is
  `[4044,family-sail,2749]`; walking is `[4044,family-sail,2752]`.
  Family sails are `3934–3937,6685`, SLPs `4224–4227,4935`.
- Heavy Demolition Ship: shared 3967/SLP 4253 has child 4215/SLP 4368.
  Fight/standing/walking are
  `[3967,family-B2,family-sail,family-B6,body]`. B2 vectors are
  `3938–3941,6686` / SLP `4228–4231,4936`; sails are the preceding
  Demolition vector; B6 vectors are `3950–3953,6689` / SLP
  `4240–4243,4939`. Bodies are 2703/2709/2712, SLP 4502.
- Fire Ship: shared 4005/SLP 4255 has child 2724/SLP 4503.
  Every state is `[4005,C2,C4,C6,C8,body]`. Family vectors C2, C4, C6, C8
  are those defined for warships above. Bodies are 2715/2721/2724, SLP 4503.
- Fast Fire Ship: shared 4031/SLP 4268 has child 2736/SLP 4509.
  Every state is `[4031,C2,C3,C4,C6,C7,C8,body]`. Family vectors are those
  defined above. Bodies are 2727/2733/2736, SLP 4509.

Layer-10 bodies sit below layer-20 hull/sail pieces. These graphs are complete
in `graphics.drs`; direct active-root rendering is unavailable.

### Death, fire missile, and explosion

| Unit | Death graphic / SLP | Frames / directions / mirror | DAG and availability |
|---|---|---|---|
| Demolition Ship | 4174 / 4347 | 7 / 16 / 12 | `[4173,-1,2746]`; all recursive dependencies present |
| Heavy Demolition Ship | 4150 / 4338 | 7 / 16 / 12 | `[-1,-1,2706]`; dependency present |
| Fire Ship | 2720 / 2753 | 6 / 1 / 0 | `[2718,-1,2719]`; root/main present, overlay SLP 2923 absent |
| Fast Fire Ship | 2732 / 2778 | 6 / 1 / 0 | `[2730,-1,2731,2730]`; root/main present, overlay SLP 2947 absent |

Fire missile unit 676 uses standing/walking graphic 3822 `M_FIRE_F`, SLP
4193, layer 30, one frame/direction, mirroring 0. Its dying graphic is 5463
`EXPLF_NN`, SLP 4370, layer 30, 20 frames, one direction, mirroring 0.
Both are present and have no deltas or forced player color.

### Sounds

All four ships use train sound 338 and selected sound 339. Demolition action
graphics use sound 433; Fire Ship action graphics use sound 429. Death
graphics use sound 379.

| Sound | Referenced resources and probabilities |
|---:|---|
| 338 | 5416 at 100% |
| 339 | 5417 at 100% |
| 429 | 5527, 5528, 5529, 5530 at 25% each |
| 433 | 5622 at 20%; 5623 at 80% |
| 379 | 5412, 5498, 5499, 5500 at 25% each |

The pinned parser exposes empty legacy filename fields for these entries, so
only sound/resource IDs are claimed. Current optional asset set contains no
sound DRS and none of these referenced files; audio playback is unavailable.

## Cannon Galleon line

Imperial Cannon Galleon unit 420 upgrades to Elite Cannon Galleon unit 691.
Effect 374 contains `420 -> 691`; tech 376 is internally named
`Elite Cannon Galley`. Both train at Dock 45 in button slot 23.

| Unit | HP / speed / LOS | Attack / range / minimum / reload / accuracy | Raw weapon classes | Cost / train |
|---|---|---|---|---|
| 420 Cannon Galleon | 120 / 1.10 / 15 | 35 / 13 / 3 / 10 / 50 | class 11: 200; 1: 15; 4: 35; 15: 15; 8: 15; 20: 40 | 200 wood, 150 gold, 1 population / 46 |
| 691 Elite Cannon Galleon | 150 / 1.10 / 17 | 45 / 15 / 3 / 10 / 50 | class 11: 275; 1: 15; 4: 45; 15: 15; 8: 15; 20: 40 | 200 wood, 150 gold, 1 population / 46 |

Both have armor-class entries 16:0, 4:0, and class 3 at 6 base/8 Elite.
Tech 376 costs 525 wood and 500 gold, takes 30 seconds, and requires Imperial
Age plus Cannon Galleon availability (generic tech 37 or Spanish tech 57).
Generic availability tech 37 requires Imperial Age, Chemistry, and tech 285;
its effect 173 enables unit 420. Tech 57 is restricted to civilization 14 and
effect 487 attempts to enable unit 768.

This file contains no unit 768 or 770 record in any civilization. Effect 374
also contains `768 -> 770`, so the Spanish alternate branch is dangling in
this dataset. Civilization restriction is therefore distinct from generic
engine capability: generic tech 37 itself has no civilization ID, but this
extractor does not yet decode each civilization's tech-tree disable list.
Actual per-civilization availability beyond the explicit civilization-14
dangling tech is not claimed.

### Graphics and composition

Units 420 and 691 share all graphics. Family order is E/F/M/W/X:

| State | Roots | Frames / directions / mirror |
|---|---|---|
| Fight | `3954,3955,3956,3957,6709` | 10 / 16 / 12 |
| Standing | `3958,3959,3960,3961,6710` | 10 / 16 / 12 |
| Walking | `3962,3963,3964,3965,6711` | 10 / 16 / 12 |

Roots are layer 20 with `player_color=-1`. Root SLPs are the absent family set
`2219,2220,2260,2263,5156`; all recursive component SLPs are present.
Every edge is `(0,0)` with display angle `-1`.

Shared hull node is 3966 `CANG_1H`, SLP 4244, layer 20. Direct graphs are:

- fight: `[3966,C2,C3,moving-sail,C7,C8,1752]`;
- standing: `[3966,C2,C3,standing-sail,C7,C8,1758]`;
- walking: `[3966,C2,C3,moving-sail,C7,C8,1762]`.

C2/C3/C7/C8 and sail vectors are defined in the Galleon section above.
Bodies 1752/1758/1762 use SLP 4336 and layer 10. Delta reconstruction is
complete; direct active-root rendering is unavailable.

Death graphic 1757 `CANGA_DN`, SLP 2116, layer 20, has six frames, one
direction, mirroring 0, and graph `[1755,-1,1756]`. Root and main SLP 4336
are present; overlay SLP 4333 is absent.

Generic missile unit 374 and Spanish-referenced missile unit 767 both use
graphic 3382 `M_BALL_R`, SLP 3803, layer 30, one frame/direction, graph
`[-1,3383]`; delta SLP 3804 is present. Both link impact graphic 1744
`EXPL1_NN`, SLP 416, layer 30, 10 frames/one direction. All projectile art is
present and has `player_color=-1`.

Cannon attack and projectile graphic sound is 411: resources 5486/5487/5488
at 33/34/33 percent. Impact sound 323 is resources 5316/5317/5318 at 30
percent each and 5459 at 10 percent. Death uses sound 379, train sound 338,
and selection sound 339. Current optional assets contain none of these audio
resources.

## Dock ship technologies

Costs and times below come from exact pinned tech records. Effect commands
target unit classes, not a hard-coded list of currently enabled units.

| Tech | ID | Cost | Time | Requirements | Effect |
|---|---:|---|---:|---|---|
| Shipwright | 373 | 1000 food, 300 gold | 60 | Imperial Age | classes 21/22/2/20: cost multiplier 0.80 and train-time multiplier 0.65 |
| Careening | 374 | 250 food, 150 gold | 50 | Castle Age | transport class 20 capacity +5; classes 21/2/20/22 armor assignment 769 |
| Dry Dock | 375 | 600 food, 400 gold | 60 | Imperial Age and Careening | transport class 20 capacity +10 and speed ×1.15; classes 21/2/22 speed ×1.15 |

In live civilization records these classes contain:

- class 2 Trade Cog: unit 17;
- class 20 Transport Ship: unit 545;
- class 21 Fishing Ship: unit 13;
- class 22 warships: units
  `15,21,250,420,436,438,442,527,528,529,532,533,539,691,706,831,832,844`.

That is the precise generic effect target set present in the file. It includes
disabled, replacement, and scenario records; class membership alone does not
prove a civilization may train each unit. All three tech records have no
fixed civilization ID, but civilization tech-tree restrictions remain a
separate availability layer not decoded by the current metadata tool.

## Viking Longboat line

Correct unit chain is Longboat 250 to Elite Longboat 533. Unit 530 is an
unrelated land unit. Effect 370 proves `250 -> 533`.

Availability tech 272 `Longboat (make avail)` is locked to civilization 11
Vikings and requires Castle Age plus tech 266. Elite tech 372 is also locked
to civilization 11, requires Imperial Age, tech 266, and tech 272, costs
750 food/475 gold, takes 60 seconds, and drives effect 370.

| Unit | HP / speed / LOS | Attack / range / minimum / reload / accuracy | Weapon classes | Cost / train / Dock slot |
|---|---|---|---|---|
| 250 Longboat | 130 / 1.54 / 8 | 7 / 6 / 0 / 3 / 100 | class 11:7; 16:9; 3:7; 17:4 | 100 wood, 50 gold, 1 population / 25 / 24 |
| 533 Elite Longboat | 160 / 1.54 / 9 | 8 / 7 / 0 / 3 / 100 | class 11:8; 16:11; 3:8; 17:4 | 100 wood, 50 gold, 1 population / 25 / 24 |

Both use primary missile 512 and volley missile 512. DAT specifies volley
amount 4, maximum 4, start-spread adjustment 1. Longboat spread is `(2,2)`;
Elite spread is `(1,1)`. This is one primary attack plus configured volley
behavior; it is not evidence that every projectile applies full listed
weapon damage independently.

Both units share faction-specific graphics:

| State | Root / SLP | Layer | Frames / directions / mirror | DAG | Archive |
|---|---|---:|---|---|---|
| Fight | 953 `LNGBT_AN` / 689 | 20 | 1 / 16 / 12 | `[951/4510,-1,952/688]` | root/main present; overlay absent |
| Standing | 959 `LNGBT_FN` / 695 | 20 | 1 / 16 / 12 | `[957/4512,-1,958/694]` | root/main present; overlay absent |
| Walking | 963 `LNGBT_WN` / 699 | 20 | 1 / 16 / 12 | `[961/4513,-1,962/698]` | root/main present; overlay absent |
| Dying | 956 `LNGBT_DN` / 692 | 20 | 5 / 1 / 0 | `[954/4511,-1,955/691]` | root present; both components absent |

Main nodes use layer 10; overlays layer 20. All nodes have
`player_color=-1`; edges use `(0,0)` and display angle `-1`.

Missile 512 uses graphic 3378 `M_ARRO_R`, SLP 3799, layer 30,
11 frames/32 directions/mirroring 24 and graph `[-1,3379]`. Delta 3379 uses
SLP 3800, layer 10, one frame/72 directions/mirroring 54. Root and delta are
present. Projectile sound 405 references resources
5471/5472/5473/5474/5570 at 20 percent each. Ship death uses sound 379;
training 338; selection 339. No referenced audio file is supplied.

## Korean Turtle Ship line

Turtle Ship 831 upgrades to Elite Turtle Ship 832 through effect 501.
Availability tech 447 `Turtle Ship (make avail)` is locked to civilization 18
Koreans and requires Castle Age plus tech 266. Elite tech 448 is locked to
civilization 18, requires Imperial Age, tech 266, and tech 447, costs
1000 food/800 gold, takes 65 seconds, and drives effect 501.

| Unit | HP / speed / LOS | Attack / range / minimum / reload / accuracy | Armor classes | Cost / train / Dock slot |
|---|---|---|---|---|
| 831 Turtle Ship | 200 / 0.90 / 8 | 50 / 6 / 0 / 6 / 100 | class 16:8; 2:0; 4:6; 3:5 | 200 wood, 200 gold, 1 population / 50 / 24 |
| 832 Elite Turtle Ship | 300 / 0.90 / 8 | 50 / 6 / 0 / 6 / 100 | class 16:11; 2:1; 4:8; 3:6 | 200 wood, 200 gold, 1 population / 50 / 24 |

Both carry only weapon class 4 at amount 50, use primary missile 767, and
have zero volley amount, zero maximum volley attacks, and no volley missile.
They share Korean-specific graphics:

| State | Root / SLP | Layer | Frames / directions / mirror | DAG | Archive |
|---|---|---:|---|---|---|
| Fight | 7257 `TURTL_AN` / 5218 | 20 | 1 / 16 / 12 | `[7177/5168,-1]` | complete |
| Standing | 7258 `TURTL_FN` / 5219 | 20 | 1 / 16 / 12 | `[7186/5176,-1]` | complete |
| Walking | 7259 `TURTL_WN` / 5220 | 20 | 1 / 16 / 12 | `[7192/5182,-1]` | complete |
| Dying | 7185 `TURTL_DN` / 5175 | 20 | 6 / 1 / 0 | `[7183/5174,-1]` | root present; main absent |

Main nodes use layer 10. All nodes have `player_color=-1`; edges use `(0,0)`
and display angle `-1`.

Missile 767 uses graphic 3382 `M_BALL_R`, SLP 3803, layer 30, one
frame/direction, graph `[-1,3383]`; delta SLP 3804 is present. Impact graphic
1744 `EXPL1_NN`, SLP 416, layer 30, has 10 frames/one direction. All
projectile art is present. Fight and missile sound 411 references
5486/5487/5488 at 33/34/33 percent; impact sound 323 references
5316/5317/5318 at 30 percent each and 5459 at 10 percent. Death is sound 379,
train 338, selection 339. Current optional assets contain none of these audio
resources.

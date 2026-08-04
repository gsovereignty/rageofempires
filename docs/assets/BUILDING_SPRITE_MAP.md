# Building sprite evidence map

## Scope and evidence standard

This report records only mappings proved against this repository's legally
supplied `setup.exe` data. It does not include extracted art.

Sources used:

- `app/Data/empires2_x1_p1.dat`, raw-deflate payload version `VER 5.7`.
- `app/Data/graphics.drs`.
- GenieUtils `Graphic`/`GraphicDelta` structure definitions.
- openage `graphic.py` and DRS/SLP format documentation.
- The reconstruction's strict DRS/SLP reader.

The reconstruction's `LegacyDatFile` parses the validated stable prefix of
`VER 5.7` directly. This prefix contains all graphic records but stops before
the HD-expanded terrain block. The live file proves these checkpoints:

- 22 terrain restrictions and 41 used terrains;
- 15 player colors and 506 sounds;
- 7,367 graphic slots; and
- terrain-block start at decompressed offset 917,004;
- 42 stored AoC terrain records (41 marked used) and 16 terrain borders; and
- random-map section start at offset 960,744 (`count=3`, pointer flag `4`).

The parser treats graphic-pointer values as presence flags and reads present
records sequentially. This is required: the values are not file offsets.
The random-map metadata after offset 960,744 is not yet implemented by this
small C++ reader. An independent live parse with the historical AoC 11.97
layout validates 514 effects and 19 civilizations, but this reader stops
before those records rather than embedding extracted values.
Record-level results below were accepted only where:

1. fixed graphic fields, graphic ID, and variable record length agreed;
2. successive records remained aligned;
3. every delta target described a matching record;
4. the referenced SLP was independently checked in `graphics.drs`; and
5. its SLP frame count agreed with DAT metadata.

The graphic catalog parses completely. Object-to-standing-graphic links are
obtained with the pinned `tools/dat_metadata` extractor and accepted only when
the live civilization records and age-replacement effects agree. Remaining
rows stay unresolved instead of receiving a family selected from its name.

## Proven mapping: Archery Range, north-European architecture

All offsets are pixel offsets. Draw lower `layer` first. Each listed record has
one frame, one direction, mirroring mode `0`.

| Age | Variant | Composite graphic | Composite SLP | Layer | Delta stack |
|---|---:|---:|---:|---:|---|
| Feudal | E | 9 `ARRG2NNE` | 21 | 20 | graphic 1 at `(0,0)`, absent `-1`, graphic 5 at `(0,0)` |
| Feudal | F | 10 `ARRG2NNF` | 22 | 20 | graphic 2 at `(0,0)`, absent `-1`, graphic 6 at `(0,0)` |
| Feudal | M | 11 `ARRG2NNM` | 23 | 20 | graphic 3 at `(0,0)`, absent `-1`, graphic 7 at `(0,0)` |
| Feudal | W | 12 `ARRG2NNW` | 24 | 20 | graphic 4 at `(0,0)`, absent `-1`, graphic 8 at `(0,0)` |
| Castle+ | E | 21 `ARRG3NNE` | 33 | 20 | graphic 13 at `(0,0)`, absent `-1`, graphic 17 at `(0,0)` |
| Castle+ | F | 22 `ARRG3NNF` | 34 | 20 | graphic 14 at `(0,0)`, absent `-1`, graphic 18 at `(0,0)` |
| Castle+ | M | 23 `ARRG3NNM` | 35 | 20 | graphic 15 at `(0,0)`, absent `-1`, graphic 19 at `(0,0)` |
| Castle+ | W | 24 `ARRG3NNW` | 36 | 20 | graphic 16 at `(0,0)`, absent `-1`, graphic 20 at `(0,0)` |

Component records:

- Feudal main: graphics 1–4, SLP 13–16, layer 5.
- Feudal overlay: graphics 5–8, SLP 17–20, layer 20.
- Castle+ main: graphics 13–16, SLP 25–28, layer 5.
- Castle+ overlay: graphics 17–20, SLP 29–32, layer 20.

### Supplied archive availability

`graphics.drs` contains composite SLPs 21–24 and 33–36. Each is classic SLP
2.0 with exactly one frame. It does not contain component SLPs 13–20 or 25–32.
Therefore this supplied HD archive supports direct composite rendering, but
not reconstruction from the DAT delta components.

## Proven live civilization mappings

An independent sequential AoC 11.97 parse of the same `VER 5.7` payload
validated 514 effects and 19 civilizations. Civilization index 1 supplies:

| Object | Graphic role | Graphic | SLP | Frames / directions |
|---|---|---:|---:|---:|
| Market 84 `MRKT` | standing | 2268 `MRKT2NNW` | 2278 | 1 / 1 |
| Market 84 `MRKT` | dying | 40 `BEXP4_NN` | 75 | 10 / 1 |
| Market 84 `MRKT` | construction | 121 `CNST4_NN` | 239 | 1 / 3 |
| Trade Cart 128 `TCART` | standing | 1141 `TCARE_FN` | 1122 | 10 / 8 |
| Trade Cart 128 `TCART` | dying | 1138 `TCARE_DN` | 1119 | 15 / 8 |
| Trade Cart 128 `TCART` | walking | 1681 `TCARE_WN` | 4486 | 10 / 8 |

All six SLPs exist in the supplied `graphics.drs`. Market standing graphic
2268 is layer 20 with delta stack 2260, absent `-1`, 2264. Unit records are
per-civilization; these links must not be generalized to every architecture.
SLP 2278 is the present one-frame complete root. Rendering selects fixed frame
0 and does not recursively redraw its baked delta stack; see
`../evidence/MARKET_STANDING_RUNTIME_EVIDENCE.md`.

## House and Town Center age mappings

Age upgrades replace the base objects with unit records 463/464/465 for House
70 and 71/141/142 for Town Center 109. Architecture families are:

- E: Goths 3, Germans 4, Vikings 11, Huns 17.
- F: Japanese 5, Chinese 6, Mongols 12, Koreans 18.
- M: Byzantine 7, Persians 8, Saracens 9, Turks 10.
- W: Gaia 0, British 1, French 2, Celts 13, Spanish 14.
- X: Aztecs 15, Mayan 16.

Every row below is layer 20, direction count 1, mirroring mode 0, and has
zero-offset deltas with display angle `-1`. Every listed composite SLP exists
in the supplied `graphics.drs`.

| Object / age | E graphic / SLP / deltas | F graphic / SLP / deltas | M graphic / SLP / deltas | W graphic / SLP / deltas | Frames |
|---|---|---|---|---|---:|
| House Dark, unit 70 | 2197 / 2223 / `2195,-1,2196` | same | same | same | 3 |
| House Feudal, unit 463 | 2206 / 2232 / `2198,-1,2202` | 2207 / 2233 / `2199,-1,2203` | 2208 / 2234 / `2200,-1,2204` | 2209 / 2235 / `2201,-1,2205` | 3 |
| House Castle+, units 464/465 | 2220 / 2244 / `2212,-1,2216` | 2221 / 2245 / `2213,-1,2217` | 2222 / 2246 / `2214,-1,2218` | 2223 / 2247 / `2215,-1,2219` | 3 |
| TC Dark, unit 109 | 3241 / 3596 / `433,434,-1,5470` | same | same | same | 1 |
| TC Feudal, unit 71 | 3250 / 3605 / `436,440,-1,5479` | 3251 / 3606 / `437,441,-1,5480` | 3252 / 3607 / `438,442,-1,5481` | 3253 / 3608 / `439,443,-1,5482` | 1 |
| TC Castle, unit 141 | 3262 / 3617 / `448,452,-1,5491` | 3263 / 3618 / `449,453,-1,5492` | 3264 / 3619 / `450,454,-1,5493` | 3265 / 3620 / `451,455,-1,5494` | 1 |
| TC Imperial, unit 142 | 3038 / 3473 / `460,464,-1,5503` | 3039 / 3474 / `461,465,-1,5504` | 3040 / 3475 / `462,466,-1,5505` | 3041 / 3476 / `463,467,-1,5506` | 1 |

Mesoamerican X-family replacements:

| Object / age | Graphic / SLP / deltas | Frames |
|---|---|---:|
| House Dark, unit 70 | 2197 / 2223 / `2195,-1,2196` | 3 |
| House Feudal, unit 463 | 6909 / 5038 / `6907,-1,6908` | 3 |
| House Castle+, units 464/465 | 6916 / 5041 / `6914,-1,6915` | 3 |
| TC Dark, unit 109 | 3241 / 3596 / `433,434,-1,5470` | 1 |
| TC Feudal, unit 71 | 6986 / 5069 / `6982,6983,-1,6989` | 1 |
| TC Castle, unit 141 | 7002 / 5078 / `6998,6999,-1,7005` | 1 |
| TC Imperial, unit 142 | 7018 / 5087 / `7014,7015,-1,7021` | 1 |

Shared lifecycle graphics:

| Object | Dying graphic / SLP | Construction graphic / SLP | Metadata |
|---|---|---|---|
| House | 38 `BEXP2_NN` / 73 | 119 `CNST2_NN` / 237 | dying 10 frames/1 direction; construction 1 frame/3 directions |
| Town Center | 40 `BEXP4_NN` / 75 | 121 `CNST4_NN` / 239 | dying 10 frames/1 direction; construction 1 frame/3 directions |

These graphics have DAT `player_color=-1`: no palette is forced by the
graphic record. Actual player-color pixels remain controlled by SLP commands.

## Recursive Town Center composition

Town Center roots are acyclic four-entry delta lists:

1. layer-5 main body;
2. layer-10 middle/player-color detail;
3. absent `-1` sentinel; and
4. layer-20 annex/overlay.

Every edge is anchored at `(0,0)` with display angle `-1`; every node has
`player_color=-1`. Render lower layers before higher layers and preserve delta
order within a layer. The composite roots listed above are all present in
`graphics.drs`, so they are the preferred complete images.

Renderer invariant: draw a present complete root SLP once. DAT deltas remain
composition evidence and are not expanded over that root. Delta traversal is
used only for an explicitly classified SLP-less/container root. All children
then come from that selected root, with accumulated offsets, lower layers
first, and stable DAT order within equal layers.

The dark-age DAG is:

| Graphic | SLP | Layer | Archive |
|---:|---:|---:|---|
| 3241 `RTWC1N4G` composite | 3596 | 20 | present |
| 433 `RTWC1N0G` main | 889 | 5 | present |
| 434 `RTWC1N1G` middle | 890 | 10 | **absent** |
| 5470 `RTWC1N7G` overlay | 4612 | 20 | present |

SLP 890 is specifically the layer-10 middle delta. It is not required when
rendering composite SLP 3596, which is present and complete. It is required
for delta-only reconstruction; that reconstruction cannot be completed from
this archive. The same HD packaging omits several layer-10 TC delta SLPs
(896, 897, 899, 908–911), while retaining their composite roots.

## Barracks, Mill, Lumber Camp, and Mining Camp

Unit replacement chains:

- Barracks: 12 Dark, 498 Feudal, 132 Castle, 20 Imperial. Castle and Imperial
  share a standing root.
- Mill: 68 Dark, 129 Feudal, 130 Castle, 131 Imperial. Castle and Imperial
  share a standing root.
- Lumber Camp: 562/563/564/565 by age; the standing root is age-invariant.
- Mining Camp: 584/585/586/587 by age; the standing root is age-invariant.

Root graphics below use family order E/F/M/W. X is the Mesoamerican family.
All root deltas use `(0,0)`, display angle `-1`, and are listed in DAT order.

| Object / age | E | F | M | W | X |
|---|---|---|---|---|---|
| Barracks Dark | 2575 / SLP2683 | same | same | same | same |
| Barracks Feudal | 90 / 130 / `82,-1,86` | 91 / 131 / `83,-1,87` | 92 / 132 / `84,-1,88` | 93 / 133 / `85,-1,89` | 6698 / 4944 / `6696,-1,6697` |
| Barracks Castle+ | 102 / 142 / `94,-1,98` | 103 / 143 / `95,-1,99` | 104 / 144 / `96,-1,100` | 105 / 145 / `97,-1,101` | 6706 / 4947 / `6704,-1,6705` |
| Mill Dark | 3124 / 3482 / `3123,3125,-1` | same | same | same | same |
| Mill Feudal | 365 / 734 / `361,369,-1` | 366 / 735 / `362,370,-1` | 367 / 736 / `363,371,-1` | 368 / 737 / `364,372,-1` | 6923 / 5047 / `6922,6924,-1` |
| Mill Castle+ | 377 / 746 / `2384,373,381,-1` | 378 / 747 / `2385,374,382,-1` | 379 / 748 / `2386,375,383,-1` | 380 / 749 / `2387,376,384,-1` | 6930 / 5050 / `6932,6929,6931,-1` |
| Lumber Camp all ages | 3115 / 3500 / `3111,3119` | 3116 / 3501 / `3112,3120` | 3117 / 3502 / `3113,3121` | 3118 / 3503 / `3114,3122` | 7049 / 5105 / `7048,7050` |
| Mining Camp all ages | 3130 / 3488 / `3126,3134` | 3131 / 3489 / `3127,3135` | 3132 / 3490 / `3128,3136` | 3133 / 3491 / `3129,3137` | 6939 / 5054 / `6938,6940` |

Barracks and Mill root SLPs in this table are present in `graphics.drs`.
Lumber/Mining root SLPs and their layer-5 main-body SLPs are absent; only the
layer-20 overlays (3504–3507, 5106, 3492–3495, 5055) are present. Therefore
the supplied archive cannot render complete Lumber or Mining Camps from
either their composite root or delta DAG.

## Dock, Fishing Ship, and fish resources

Every civilization maps Dock unit 45 and Fishing Ship unit 13 to the same
graphics. Fish-resource objects exist only in Gaia civilization 0. The
extractor proves each fish record with both `track_as_resource=true` and
`resource_group=2`; the graphic catalog independently names each linked
graphic `FISH*`.

Dock unit 45 links:

| Role | Graphic / SLP | Layer | Frames / directions / mirror | Delta graph | Archive |
|---|---|---:|---|---|---|
| Standing | 215 `DOCK1N1G` / no SLP (`-1`) | 20 | 60 / 1 / 0 | `214,216,4411` | root intentionally unavailable |
| Construction | 4248 `CNSTD_NN` / 4397 | 10 | 1 / 3 / 0 | none | present |
| Dying | 5452 `DEXP3_NN` / 4597 | 30 | 10 / 1 / 0 | `-1,-1` | present |

Dock standing graph is complete in the archive:

| Graphic | SLP | Layer | Frames / directions | Archive |
|---:|---:|---:|---:|---|
| 214 `DOCK1N0G` main | 374 | 5 | 1 / 1 | present |
| 216 `DOCK1NNG` static overlay | 376 | 20 | 1 / 1 | present |
| 4411 `DOCKXN1G` animated overlay | 4518 | 20 | 60 / 1 | present |

All three edges use `(0,0)` and display angle `-1`. All Dock nodes have
`player_color=-1` and mirroring mode 0. Because graphic 215 has no SLP,
render it from this three-node layer graph.

Fishing Ship unit 13 links the DAT `fight_sprite`, dying, standing, and
walking fields. `fight_sprite` is reported by field name here: assigning
gameplay meaning to it is outside this evidence.

| DAT role | Root graphic / SLP | Root frames / directions / mirror | Main graphic / SLP | Overlay graphic / SLP | Archive result |
|---|---|---|---|---|---|
| Fight | 797 `FSHSP_AN` / 438 | 10 / 8 / 6 | 795 / 4504 | 796 / 2929 | root and components absent |
| Dying | 800 `FSHSP_DN` / 441 | 5 / 1 / 0 | 798 / 4505 | 799 / 2931 | root present; components absent |
| Standing | 803 `FSHSP_FN` / 444 | 1 / 8 / 6 | 801 / 4506 | 802 / 2933 | root present; components absent |
| Walking | 807 `FSHSP_WN` / 449 | 1 / 8 / 6 | 805 / 4507 | 806 / 2935 | root present; components absent |

Each Fishing Ship root is layer 20 with graph `[layer-10 main,-1,layer-20
overlay]`. Every edge uses `(0,0)` and display angle `-1`; every node has
`player_color=-1`. Component frames/directions/mirror are 10/8/6 for fight,
dying, and walking, except standing components are 5/8/6. Dying root alone is
5/1/0. Supplied archive supports direct standing, walking, and dying roots.
It cannot render the fight graphic or reconstruct any ship graph from deltas.

Gaia fish-resource standing graphics:

| Unit | Graphic / SLP | Layer | Frames / directions / mirror | Archive |
|---:|---|---:|---|---|
| 53 | 1700 `FISHX_NN` / 420 | 11 | 30 / 2 / 1 | present |
| 69 | 3138 `FISHS_NN` / 3549 | 11 | 34 / 1 / 0 | present |
| 455 | 2186 `FISH1_NN` / 1910 | 11 | 49 / 2 / 1 | present |
| 456 | 2187 `FISH2_NN` / 1911 | 11 | 49 / 2 / 1 | present |
| 457 | 2188 `FISH3_NN` / 1912 | 11 | 49 / 2 / 1 | present |
| 458 | 2189 `FISH4_NN` / 1913 | 11 | 49 / 2 / 1 | present |
| 459 | 2190 `FISH5_NN` / no SLP (`-1`) | 11 | 30 / 2 / 1 | unavailable |

All fish graphics have `player_color=-1` and no delta graph. Unit 459 has
neither direct SLP nor deltas, so no supplied art path exists for it.

## Archery Range, Stable, Castle, and Siege Workshop

The unit/effect replacement chains prove:

- Archery Range: unit 87 Feudal, units 10/14 Castle+.
- Stable: unit 101 Feudal, units 86/153 Castle+.
- Castle: units 82/33 share one standing root.
- Siege Workshop: unit 49 has one age-invariant standing root.

Family order is E/F/M/W/X. Every node in these graphs has
`player_color=-1`, mirroring mode 0, one frame, and one direction. All edges
use `(0,0)` and display angle `-1`.

| Object / age | Root graphics | Root SLPs | Direct delta graphics |
|---|---|---|---|
| Archery Range Feudal | `9,10,11,12,6658` | `21,22,23,24,4926` | main `1,2,3,4,6656`; absent `-1`; overlay `5,6,7,8,6657` |
| Archery Range Castle+ | `21,22,23,24,6665` | `33,34,35,36,4929` | main `13,14,15,16,6663`; absent `-1`; overlay `17,18,19,20,6664` |
| Stable Feudal | `510,511,512,513,7061` | `1006,1007,1008,1009,5110` | main `502,503,504,505,7059`; absent `-1`; overlay `506,507,508,509,7060` |
| Stable Castle+ | `522,523,524,525,7068` | `1018,1019,1020,1021,5113` | main `514,515,516,517,7066`; absent `-1`; overlay `518,519,520,521,7067` |
| Castle | `171,172,173,174,6747` | `302,303,304,305,4956` | main `163,164,165,166,6745`; absent `-1`; overlay `167,168,169,170,6746` |
| Siege Workshop | `486,487,488,489,7042` | `955,956,957,958,5103` | main `478,479,480,481,7040`; absent `-1`; overlay `482,483,484,485,7041` |

Roots and main nodes use layers 20 and 5 respectively; overlays use layer 20.
Archive availability is exact:

- every root SLP in the table is present;
- every E/F/M/W Stable component SLP (`998–1005`, `1010–1017`) and both X
  Stable component pairs (`5108–5109`, `5111–5112`) are absent;
- Castle main SLPs `294–297` and `4954` are present, while overlays
  `298–301` and `4955` are absent;
- E/F/M/W Siege Workshop component SLPs `947–954` are absent; X main SLP
  `5101` is present and overlay `5102` is absent; and
- Archery Range E/F/M/W availability is detailed above. X component SLPs
  `4924–4925` and `4927–4928` are absent.

Thus direct root rendering is complete for all these mappings. Delta-only
reconstruction is incomplete except for no listed family.

## Blacksmith

The replacement chain is units 103/105 for Dark/Feudal and units 18/19 for
Castle/Imperial. Family order is E/F/M/W/X.

| Age | Animated root graphics | Root SLPs | Main graphics | Nested composite graphics |
|---|---|---|---|---|
| Dark/Feudal | `50,51,52,53,6673` | `2219,2220,2260,2263,5156` | `46,47,48,49,6672` | `54,55,56,57,6674` |
| Castle+ | `62,63,64,65,6680` | `98,99,100,101,4933` | `58,59,60,61,6679` | `66,67,68,69,6681` |

Animated roots are layer 21, 23 frames, one direction. Main nodes are layer 5
with one frame. Nested composites are layer 20 with one frame. Shared smoke
graphic 5314 `BLACSMK`, SLP 4520, is layer 20 with 23 frames. All use
`player_color=-1`, mirroring mode 0, and zero-offset/display-angle `-1` edges.

Dark/Feudal root graph per family is
`root -> [main, nested composite, smoke]`; nested composite is
`[main,-1,smoke]`. Castle+ has the same outer list, but its nested composite
is `[main,-1,animated root]`, forming a DAT cycle. A recursive renderer must
detect that cycle.

Archive availability:

- Dark/Feudal animated root SLPs and main SLPs (`82–85`, `4930`) are absent.
  Nested composite SLPs `90–93` and `4931` plus smoke SLP 4520 are present.
- Castle+ animated root SLPs `98–101`, `4933`, nested composite SLPs
  `102–105`, `4934`, and smoke SLP 4520 are present. Main SLPs `94–97` and
  `4932` are absent.

Therefore supplied archive can directly render Castle+ animated roots.
Dark/Feudal complete animated roots are unavailable; static nested composites
remain available.

## University

Unit 209 is the Castle-age object; effect replacement unit 210 supplies the
Imperial standing graphic. Family order is E/F/M/W/X.

| Age | Animated root graphics / SLPs | Main graphics / SLPs | Nested composite graphics / SLPs |
|---|---|---|---|
| Castle | `566,567,568,569,7084` / `1360,1361,1362,1363,5118` | `562,563,564,565,7083` / `1356,1357,1358,1359,5117` | `570,571,572,573,7085` / `3832,3833,3834,3835,5119` |
| Imperial | `578,579,580,581,7090` / `1372,1373,1374,1375,5121` | `574,575,576,577,7089` / `1368,1369,1370,1371,5120` | `582,583,584,585,7091` / `3836,3837,3838,3839,5122` |

Animated roots are layer 20 with 9 frames; all other nodes have one frame.
Main nodes use layer 5 and nested composites layer 20. All nodes have one
direction, `player_color=-1`, and mirroring mode 0. Edges use `(0,0)` and
display angle `-1`.

Castle root list is `[main,-1,nested]`; Imperial is
`[main,nested,-1]`. Each nested composite points back to its animated root and
main (`[-1,root,main]`), so both graphs contain a DAT cycle.

All animated-root and nested-composite SLPs are present. All main SLPs
(`1356–1359`, `1368–1371`, `5117`, `5120`) are absent. Direct animated-root
rendering is complete; delta-only reconstruction is not.

## Watch Tower upgrade chain

Effect commands replace base unit 79 with target 234 or 235. This evidence
fixes links without assigning names to target IDs beyond the live DAT.
Family order is E/F/M/W/X.

| Unit | Root graphics / SLPs | Main graphics / SLPs |
|---|---|---|
| 79 | `4199,4200,4201,4202,7116` / `2652,2653,2654,2655,5134` | `4191,4192,4193,4194,7114` / `4348,4349,4350,4351,5132` |
| 234 | `2529,2530,2531,2532,7123` / `2664,2665,2666,2667,5137` | `2521,2522,2523,2524,7121` / `2656,2657,2658,2659,5135` |
| 235 | `2404,2405,2406,2407,7131` / `2538,2539,2540,2541,5140` | `2400,2401,2402,2403,7129` / `2534,2535,2536,2537,5138` |

Each root is layer 20 with list `[main,-1]`; main is layer 5. Every node has
one frame/direction, `player_color=-1`, and mirroring mode 0. Edges use
`(0,0)` and display angle `-1`. Every root and main SLP above is present, so
both direct and delta rendering are complete.

## Stone walls

Unit 117 is replaced by unit 155 in effects 138/187. Effect 185 also names
target 231, but no unit 231 record exists in the live civilization arrays;
no graphic mapping is claimed for it. Family order is E/F/M/W/X.

| Unit | Root graphics / SLPs | Main graphics / SLPs | Overlay graphics / SLPs |
|---|---|---|---|
| 117 | `2021,2022,2023,2024,7096` / `2098,2099,2100,2101,5124` | `2013,2014,2015,2016,7094` / `2090,2091,2092,2093,5123` | `2017,2018,2019,2020,7095` / `2219,2220,2260,2263,5156` |
| 155 | `2033,2034,2035,2036,7100` / `2110,2111,2112,2113,5126` | `2025,2026,2027,2028,7098` / `2102,2103,2104,2105,5125` | `2029,2030,2031,2032,7099` / `2219,2220,2260,2263,5156` |

Roots and overlays use layer 20; mains use layer 5. Each has one frame, five
directions, `player_color=-1`, and mirroring mode 0. Edges use `(0,0)` and
display angle `-1`. Unit 117 roots have `[-1,main,-1,overlay]`; unit 155 roots
have `[main,-1,overlay]`.

All root and main SLPs are present. All overlay SLPs are absent. Direct root
rendering is complete; delta-only reconstruction is not.

## Bombard Tower

DAT building 236 is a 1×1 Bombard Tower. Its normal graphic 2415 maps to
composite SLP 2549 (`WCTW4NNGW`), construction graphic 118 maps to SLP 236
(`CNST1_NN`), and death graphic 38 maps to SLP 73 (`BEXP2_NN`). All three are
present in `graphics.drs`.

Snow graphic 6294 maps to SLP 2263 (`WCTW4SW`), but SLP 2263 is absent from
the supplied `graphics.drs`. Snowy terrain therefore uses normal SLP 2549 as
the proved fallback; no snow art is inferred or synthesized.

Missile 506 uses graphic 3382/SLP 3803 (`M_BALL_R`) with sound 411/WAV
5486–5488. Impact graphic 1744/SLP 416 (`EXPL1_NN`) and tower death use sound
323/WAV 5316–5318/5459. Selection and construction sound 23 uses WAV 5276.
All listed projectile, impact, and WAV resources are present.

## Requested buildings not yet proved

| Building / object ID | Status |
|---|---|
| Town Center / 109 plus age replacements | Resolved for all 19 civilizations; see age mappings |
| House / 70 plus age replacements | Resolved for all 19 civilizations; see age mappings |
| Barracks / 12 | Resolved for all families/ages; root SLPs present |
| Mill / 68 | Resolved for all families/ages; root SLPs present |
| Lumber Camp / 562 | Mapping resolved; complete art absent from supplied DRS |
| Mining Camp / 584 | Mapping resolved; complete art absent from supplied DRS |
| Farm / 50 | Graphic catalog available; post-terrain object link unresolved |
| Stable / 101 plus age replacements | Resolved for all families/ages; root SLPs present |
| Blacksmith / 103 plus age replacements | Resolved; Castle+ animated roots present, Dark/Feudal roots absent |
| Castle / 82 | Resolved for all families; root SLPs present |
| University / 209 plus Imperial replacement | Resolved; animated roots present |
| Siege Workshop / 49 | Resolved for all families; root SLPs present |
| Stone walls / 117 and proved replacement 155 | Resolved; root SLPs present |
| Gates / multiple orientation-specific object IDs | Graphic catalog available; post-terrain object link unresolved |
| Towers / 79, replacements 234/235, Bombard Tower 236 | Resolved; roots present, Bombard snow SLP absent with proved normal fallback |
| Monastery / 104 | Graphic catalog available; post-terrain object link unresolved |

## Renderer guidance

- Do not infer SLP ID from unit/object ID.
- Do not assume DAT delta component SLPs exist in the HD DRS.
- Prefer a proved composite SLP when the archive contains it.
- Treat architectural suffixes (`E`, `F`, `M`, `W`) as variants, not frames.
- Keep existing procedural building art for every unresolved row.

## Reproduction notes

Selective archive extraction (outside repository):

```sh
innoextract --output-dir /tmp/aoe-assets \
  --include app/Data/empires2_x1_p1.dat \
  --include app/Data/graphics.drs \
  setup.exe
```

No generated image or game asset belongs in source control.

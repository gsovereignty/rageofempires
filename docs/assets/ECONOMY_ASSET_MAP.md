# Economy technology and resource evidence

## Scope and boundary

[`generated/economy_dat_metadata.json`](../../generated/economy_dat_metadata.json)
records ten live VER 5.7 economy technologies, raw effects, civilization
boundaries, worker/resource records, terrain restrictions, and terrain
summaries. DAT proves numeric records. Gather cadence, floating-point rounding,
drop-off/retarget ordering, farm reseeding, terrain semantics, and random-map
placement require original-runtime validation.

## Technology records

Resource IDs are 0 food, 1 wood, and 3 gold. `Effect` is raw tech `time2`.

| Technology | DAT/effect | Prerequisites | Location | Cost | Time | Button/icon | Raw effect |
|---|---|---|---|---|---:|---|---|
| Horse Collar | 14/14 | Feudal 101 | Mill 68 | 75 food, 75 wood | 20 s | 1/2 | resource 36 +75; workers 214/259 work rate ×1 |
| Heavy Plow | 13/13 | Castle 102, Horse Collar 14 | Mill 68 | 125 food, 125 wood | 40 s | 1/1 | resource 36 +125; workers 214/259 work rate ×1 and attribute 14 +1 |
| Crop Rotation | 12/12 | Imperial 103, Heavy Plow 13 | Mill 68 | 250 food, 250 wood | 70 s | 1/0 | resource 36 +175 |
| Bow Saw | 203/196 | Castle 102, Double-Bit Axe 202 | Lumber Camp 562 | 150 food, 100 wood | 50 s | 1/71 | workers 218/123 work rate ×1.2; attribute 14 +0 |
| Two-Man Saw | 221/210 | Imperial 103, Bow Saw 203 | Lumber Camp 562 | 300 food, 200 wood | 100 s | 1/81 | workers 123/218 work rate ×1.1 |
| Gold Mining | 55/55 | Feudal 101 | Mining Camp 584 | 100 food, 75 wood | 30 s | 1/15 | workers 581/579 work rate ×1.15 |
| Gold Shaft Mining | 182/178 | Castle 102, Gold Mining 55 | Mining Camp 584 | 200 food, 150 wood | 75 s | 1/62 | workers 579/581 work rate ×1.15 |
| Stone Mining | 278/278 | Feudal 101 | Mining Camp 584 | 100 food, 75 wood | 30 s | 2/87 | workers 124/220 work rate ×1.15 |
| Stone Shaft Mining | 279/279 | Castle 102, Stone Mining 278 | Mining Camp 584 | 200 food, 150 wood | 75 s | 2/88 | workers 220/124 work rate ×1.15 |
| Hand Cart | 249/238 | Castle 102, Wheelbarrow 213 | Town Center 109 | 300 food, 200 wood | 55 s | 7/42 | class 4 attribute 14 ×1.5 and speed ×1.1 |

Effect selectors and attribute 14/resource 36 meanings stay numeric. Runtime
labels are not inferred beyond existing validated carry/speed and farm
behavior.

## Represented deterministic contracts

- Double-Bit Axe, Bow Saw, and Two-Man Saw multiply sequentially:
  `1.20 × 1.20 × 1.10 = 1.584`. Fixed-point gather remainder uses denominator
  10,000 and retains exact composed ratio.
- Gold Mining plus Gold Shaft Mining, and Stone Mining plus Stone Shaft
  Mining, each compose `1.15 × 1.15 = 1.3225`.
- Wheelbarrow multiplies base villager carry 10 by 1.25 and floors to 12.
  Hand Cart then multiplies 12 by 1.5 and floors to 18.
- Heavy Plow's attribute-14 +1 applies only while a villager actively gathers
  food from a Farm. Berry, hunt, herdable, fish, and return-trip carry remain
  unchanged.
- Wheelbarrow and Hand Cart speed multipliers compose
  `1.10 × 1.10 = 1.21`; movement uses a fixed-point numerator 121 over
  denominator 100.

These contracts reproduce decoded multiplier order. Original-engine floor
timing, mid-gather research remainder conversion, collision/path-end movement,
and job-switch cadence still require original-runtime validation.

## Civilization boundaries

Heavy Plow, Bow Saw, Gold Mining, Stone Mining, and Hand Cart are available to
all 18 civilizations. Live disable exclusions:

- Crop Rotation: British, Japanese, Chinese, Saracens, Turks, Mongols, Celts,
  Spanish, Huns, and Koreans.
- Two-Man Saw: French, Mongols, Celts, and Aztecs.
- Gold Shaft Mining: Goths, Germans, Japanese, Spanish, and Mayan.
- Stone Shaft Mining: British, French, Japanese, Saracens, Turks, Vikings,
  and Huns.

## Worker and resource records

Base worker action work rates include berry forager 120 at 0.31, lumberjacks
123/218 at 0.39, stone miners 124/220 at 0.36, gold miners 579/581 at 0.38,
and farmers 214/259 at 0.53. Generated tasks retain target IDs/classes,
resource attributes, work/carry sprites, work range, auto-search, and
three-second search waits.

Resource records expose:

- Berry bush 59: 125 food, resource group 1, standing graphic 2420.
- Gold mine 66: 800 gold, group 4, graphic 2421.
- Stone mine 102: 350 stone, group 3, graphic 1592.
- Shore fish 69: 200 resource-17 units, group 2, graphic 3138.
- Deep-fish records 450/451: 350 each, group 2, graphics 2181/2182.
- Fish record 458: 225, group 2, graphic 2189.

Animal food records expose Sheep 594 at 100 food with resource decay 0.25,
Deer 65 at 140 with decay 0.25, and Boar 48 at 340 with decay 0.4. Runtime
keeps a killed animal as a finite carcass, applies those rates with an exact
fixed-point remainder at five simulation ticks per represented second, and
removes it only at zero food. Multiple gatherers draw sequentially from the
same remaining amount, so gathering plus decay cannot create food. The
remainder is save-persistent. Sheep enter the same killed-carcass state when
gathering begins; this live DAT does not evidence a sheep no-decay exception.

Farm 50 exposes work rate 0.4, attribute type 0 amount 15, terrain restriction
4, and graphics/tasks. Heavy Plow, Crop Rotation, and Horse Collar modify
global resource 36 by 125/175/75. DAT does not justify treating farm attribute
15 as total food; exact base capacity, reseeding, exhaustion ordering, and
civilization adjustments remain separate runtime contracts.

## Farm-specific live evidence

Generated `farm_evidence` makes farm fields independently testable:

- Farm 50 costs 60 wood, takes 15 seconds to create, has 480 HP, uses action
  work rate `0.4000000059604645`, and is created at unit 118 through button 6.
- Female and male farmer records 214/259 both use work rate
  `0.5299999713897705` and base carry 10. Task 0 targets Farm 50 with action
  type 5, attribute tuple `(16, 190, 0, -1)`, zero work range, auto-search
  enabled, and a three-second search wait. Female work/carry graphics are
  1953/1952 (SLPs 1876/1875); male graphics are 1600/1599
  (SLPs 1512/1515).
- Farm standing graphic 255 (`FARM0NNG`, SLP 419) composes graphic 253
  (`FARM0N0G`, SLP 417), one empty delta slot, and graphic 254
  (`FARM0N1G`, SLP 418). DAT provides no construction graphic, dying graphic,
  damage sound, or death sound. Selected sound 416 resolves to resource 5496.
  Therefore depletion must not invent a death animation or sound from these
  records.
- Capacity resource 36 receives +75 from Horse Collar, +125 from Heavy Plow,
  and +175 from Crop Rotation. Heavy Plow separately adds 1 to attribute 14
  on farmer records 214/259. Neither Heavy Plow nor Crop Rotation increases
  farming work rate: their explicit work-rate multipliers are ×1 or absent.

## AoC queue contract versus later auto-reseed

The Conquerors manual defines a finite, prepaid Farm queue at the Mill:
each click adds one Farm and requires its wood cost immediately; any Mill may
add or remove queue entries; queued entries survive destruction of all Mills;
one entry is consumed when an exhausted Farm is replanted; queued Farms use
Farm technologies researched before replanting. See the
[The Conquerors expansion manual](https://manuals.plus/m/dfcc44241b836c6ff27a5847cc3143380251208c49d8efc417d8ba5624469bb0.pdf).

This is not Definitive Edition's persistent auto-reseed toggle. That later
behavior buys replacements when needed and can continue indefinitely while
wood remains. Safe AoC implementation contract:

1. Queue command atomically checks and deducts 60 wood, then increments a
   player-wide prepaid count. Cancel removes one entry; refund behavior needs
   original-runtime confirmation before implementation.
2. Farm exhaustion leaves carried food intact, marks farm exhausted, and then
   consumes at most one prepaid entry. No entry means villager becomes idle or
   follows normal retarget rules; no automatic wood purchase occurs.
3. Consumed entry replants same Farm site using capacity effective at reseed
   time: baseline runtime contract 175, then 250/375/550 after Horse Collar,
   Heavy Plow, and Crop Rotation. Research adds its resource-36 increment to
   active non-exhausted Farms without reviving exhausted Farms.
4. Heavy Plow raises only active Farm-food carry from 10 to 11 before other
   validated carry modifiers. It does not raise gather rate. Crop Rotation only
   raises Farm capacity.
5. Queue count belongs to player, not Mill object, and must serialize/replay.
   Construction timing, notification suppression, exact exhaustion tick
   ordering, and villager retarget ordering remain runtime-validation items;
   DAT alone does not settle them.

The original AoC manual establishes a player-wide prepaid Farm queue at any
Mill. Each accepted click immediately costs 60 wood (36 for the represented
Teuton 40% Farm discount), survives loss of that Mill, and consumes one entry
when a Farm exhausts. Replanting uses the owner's capacity at exhaustion, so a
Farm queued before a later farming technology benefits from that technology.
Insufficient wood is an atomic no-op. This is the original bounded queue, not
the later Definitive Edition persistent auto-reseed toggle.
The bounded queue holds at most 15 entries as an overflow-safe runtime
ceiling; a full queue rejects a click without charging wood. The exact
original queue ceiling remains an engine-validation boundary.
Legacy replay versions retain their historical selected-exhausted-Farm
immediate reseed command instead of being reinterpreted as a Mill queue click.

Workers use terrain restriction 7; berries, gold, and stone use restriction 8;
shore fish use 19; deep fish use 13. Generated records preserve full
per-terrain passability vectors. Terrain summaries preserve enabled state,
SLP/sound, passable/impassable alternates, and generated objects. Numeric
tables alone do not prove movement/build policy or random-map density.

## Deep-sea versus shore-fish boundary

Pinned live DAT evidence proves four distinct fish identities:

- shore fish 69: 200 food, restriction 19, class 33, graphic 3138;
- deep fish 450/451: 350 food each, restriction 13, class 5, graphics
  2181/2182;
- fish 458: 225 food, restriction 19, class 5, graphic 2189.

Current `Terrain::fish` cannot encode those identities. It defaults to 200
food, is sailable and non-walkable, depletes to Water, and is handled only by
the Fishing Ship loop at one represented food per tick, 15-food carry, and
Dock drop-off. This combines shore-fish stock with deep-water movement; it is
not evidence of original behavior. Villager terrain gathering cannot route
onto that same non-walkable tile.

No pinned generated record currently captures Fishing Ship 13 action tasks,
work rate, or carry field. Generated metadata explicitly reserves gather
cadence/floating-point rounding, drop-off/retarget/depletion ordering, and
terrain movement semantics for original-runtime evidence. Therefore no exact
rate, movement-domain split, or depletion base terrain can be selected.
Adding `deep_fish`/`shore_fish` terrain values now would move guesses into a
versioned format. Scenario, save, and random-map formats remain unchanged
until Fishing Ship task evidence and original executable ordering are pinned.

## Hunt, herdable, and carcass evidence

Generated `hunt_evidence` records three live VER 5.7 animals:

| Animal/unit | Food attribute | Raw `attribute_rot` | HP | Standing graphic/SLP | Dying graphic/SLP |
|---|---:|---:|---:|---|---|
| Wild Boar 48 | 340 | `0.4000000059604645` | 75 | 2455/2557 | 2454/2556 |
| Deer 65 | 140 | `0.25` | 5 | 764/342 | 761/339 |
| Sheep 594 | 100 | `0.25` | 7 | 3178/3629 | 3175/3626 |

All three expose food as attribute type 0. All three have a dying graphic but
no unit-level damage or death sound. Sound comes from graphic events instead:
Deer and Sheep dying graphic 761/3175 trigger sound 413 at frame delay 11;
Boar dying graphic 2454 triggers sounds 451 and 452 at delays 2 and 7.
Boar attack, idle, and run graphics also reference sounds 450, 449, and 453;
Sheep idle references 456. Generated evidence preserves complete per-angle
event lists and sound-resource records.

Hunters 122/216 use work rate `0.4099999964237213`, carry 35, and action type
110 tasks for Boar 48 and Deer 65. Task attributes are `(15, 190, 0, -1)`,
work range is 0.1, auto-search is enabled, and search wait is three seconds.
Shepherds 590/592 use work rate `0.33000001311302185`, carry 10, and action
type 110 against unit class 58 with the same range/search settings.

Raw civilization effects:

- British hidden technology 383/effect 381 multiplies Shepherd 590/592 work
  rate by 1.25.
- Mongol hidden technology 389/effect 388 multiplies Hunter 122/216 work rate
  by 1.5.
- Goth hidden technology 402/effect 414 adds attack class 24 amount 5 and
  attribute 14 amount 15 to Hunter 122/216.
- Mayan civilization effect 449 applies raw 0.83 work-rate multipliers across
  gatherer variants, including Hunters and Shepherds, alongside resource
  duration commands. This encoding must not be simplified into “slower
  gathering” without reproducing its paired resource accounting.

### Behavioral boundary and safe contract

Original-era documentation states that animal food becomes gatherable after
death and carcasses continue losing food even when no Villager gathers.
Runtime observations identify Boar decay as 0.4 food per game-second and
Deer/Sheep decay as 0.25; those values match the live raw `attribute_rot`
fields. See the contemporary
[AoC strategy guide](https://gamefaqs.gamespot.com/pc/914421-age-of-empires-ii-the-conquerors-expansion/faqs/38481)
for carcass rot behavior. Exact fixed-step rounding is still runtime evidence,
not encoded by the float alone.

Safe implementation contract:

1. Living Boar, Deer, and Sheep do not decay. On valid death, retain remaining
   food and start independent carcass decay immediately.
2. Decay runs whether zero or many Villagers gather: Boar 0.4 food/s,
   Deer/Sheep 0.25 food/s. Gathering and decay both reduce the same remaining
   food, clamped at zero. Remove carcass only at zero after resolving same-tick
   gathers deterministically.
3. Original AoC edible-carcass eligibility requires a Villager killing blow.
   Military/building kills spoil the food. Do not import later Definitive
   Edition behavior allowing broader killer types.
4. Base gather rates come from worker mode, not animal decay:
   Hunters 0.41 and Shepherds 0.33. Apply British ×1.25 only to Shepherds,
   Mongol ×1.5 only to Hunters, and Goth hunt attack/carry only to Hunters.
5. Do not infer sound from null unit `death_sound`. Play dying-graphic event
   sounds at their recorded frame delays. Preserve dying animation before
   static carcass presentation.
6. Use fixed-point accumulators for fractional gather and decay. DAT proves
   rates, targets, graphics, and events; it does not prove rounding phase,
   death-tick ordering, carcass ownership transfer, or retarget timing.

## Represented civilization economy effects

The reconstruction retains exact fixed-point rate multipliers where the
affected task already exists: Mongol Hunters ×1.5, Turk gold miners ×1.15,
and Korean stone miners ×1.20. These compose with researched gathering
technologies. Persian Town Centers and Docks use ×1.10/×1.15/×1.20 work rates
in Feudal/Castle/Imperial Age. Vikings receive Wheelbarrow in Feudal Age and
Hand Cart in Castle Age. Aztec Monks gain five maximum hit points for each
represented researched Monastery technology.

Generated random maps apply civilization start packages before validation:
Chinese receive three extra Villagers, minus 50 wood, and minus 200 food;
Mayans receive one extra Villager and minus 50 food. Authored Scenario loading
and later civilization selection never apply these packages. Mayan resource
duration uses separate credited yield and stock depletion: every 115 credited
units consume 100 real stock units. A per-player fixed-point remainder prevents
worker/source rotation exploits and stays isolated during shared-node
competition. The same path covers finite terrain resources, Farms, animal
food, the reconstruction's undifferentiated fish tile, and Fish Traps;
carcass decay still removes real stock
independently. Current Save v109 preserves all player remainders; Save v106
introduced this persistence. The raw 0.83 DAT
commands prove the paired duration effect; 100/115 is the exact documented
15%-longer contract rather than a literal gathering slowdown. No team bonus is
inferred.

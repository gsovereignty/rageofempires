# Trade and Fish Trap DAT asset map

## Evidence boundary

`generated/trade_dat_metadata.json` is generated from validated VER 5.7 DAT
metadata. It preserves raw entity, task, technology, effect, civilization,
graphic, and sound records. Resource IDs are 0 food, 1 wood, 3 gold, and 4
population.

DAT records do **not** expose the original runtime's distance-to-gold formula,
rounding, route interruption, allied endpoint rules, exact team-vision
propagation, Market fee calculation, tribute transfer contract, or Fish Trap
depletion/rebuild ordering. Those remain original-runtime validation targets.
Numeric resource fields and task action types below are evidence, not inferred
runtime formulas.

## Trade Cog and Fish Trap

Trade Cog 17 has 80 HP, speed 1.32, LOS/search radius 6, work rate 0.23,
terrain restriction 3, and armor classes 16:0, 4:0, and 3:6. Dock 45 trains it
in slot 3 for 100 wood, 50 gold, one population, and 36 source seconds. Its
tasks are auto-search action 109 with a three-second wait and action 111
targeting Dock 45 with work value 1.0. DAT supplies no distance payout formula.

Hidden technology 180 `Cog` requires Feudal Age 101; effect 153 enables unit
17. All 18 civilization trees retain this gate.

Fish Trap 199 has 50 HP, LOS 1, terrain restriction 13, armor classes 4:0 and
3:0, and costs 100 wood. Fishing Ship 13 builds it in slot 1 in 53 source
seconds. Tasks are default action 21 and action 121, both using attribute 17
and work value 1.0. Its separate attribute record is type 17, amount 15; this
record alone does not prove total food capacity.

Hidden technology 357 requires Feudal Age 101; effect 356 enables object 199.
All 18 civilization trees retain this gate.

Fishing Ship 13 costs 75 wood and one population and trains at Dock 45 in 40
source seconds. It has work rate 0.28. Gather tasks use work values 1.75 for
class 5, 1.0 for class 33, 1.75 for class 31, and 1.25 for Fish Trap 199.
Action 101 has work value 3.57. Market 84 and Dock 45 contain no DAT task that
defines trade payout. Trade Cart 128 has action 111 targeting Market 84,
attribute 9, work value 0, plus auto-search action 109 and action 131.

## Market technologies

All five research at Market 84:

- Coinage 23: Feudal Age 101; 150 food/50 gold; 50 seconds; slot 3, icon 7.
  Effect 23 applies type-1 resource command 46 with raw value 0.20.
- Banking 17: Castle Age 102 plus Coinage 23; 200 food/100 gold; 50 seconds;
  slot 3, icon 3. Effect 17 applies type-1 resource command 46 with value 0.
- Cartography 19: Feudal Age 101; 100 food/100 gold; 60 seconds; slot 2,
  icon 4. Effect 19 applies type-1 resource command 50 with `b=1`, value 1.
- Caravan 48: Castle Age 102 plus Cartography 19; 200 food/200 gold; 40
  seconds; slot 2, icon 113. Effect 482 multiplies movement speed and work rate
  by 1.5 for Trade Cog 17, Trade Cart 128, and duplicate cart record 204.
- Guilds 15: Imperial Age 103; 300 food/200 gold; 50 seconds; slot 4, icon 58.
  Effect 15 applies type-1 resource command 78 with raw value 0.15.

Coinage, Banking, Cartography, and Caravan are available to all 18
civilizations. Guilds is unavailable to French, Japanese, Chinese, Saracens,
Vikings, Mongols, and Aztecs.

Represented Caravan behavior applies its exact 1.5 movement multipliers as
fixed-point numerators 150 for Trade Cart and 198 for Trade Cog. DAT also
multiplies both units' 0.23 work rate by 1.5. The reconstruction models this as
a bounded endpoint turnaround reduction from three ticks to two; DAT does not
prove that state-machine interpretation.

Cartography gates allied unit/building vision in represented play. Effect 19's
resource-50 command establishes the technology toggle, but exact original
sharing, exploration, diplomacy-change, and persistence semantics remain
runtime validation.

Coinage and Banking drive a bounded tribute subsystem: an alive completed
sender Market and allied distinct recipient are required; the sender pays
requested wood, food, gold, or stone plus a 30%, 20%, or 0% fee for base,
Coinage, or Banking state. Resource-46 commands prove raw Coinage value 0.20
and Banking value 0, but the starting 30%, transfer checks, fee rounding, and
resource-46 interpretation are reconstruction behavior pending original
validation. This tribute fee is separate from Guilds and Market buy/sell fees.

The pinned executable's `tribute-to-player` callback at `0x456080..0x456132`
independently proves the front half of the contract. It validates the target
player and resource index 0..3, loads the sender's resource value, converts the
script's signed integer amount to `float` with `fildl` at `0x456116`, and
passes recipient, resource, amount, and player state to virtual method
`+0x154`. The supplied decompilation does not resolve that virtual target, so
it does not prove fractional fee settlement or rounding.

`percentage_fee_floor` now centralizes the represented tribute boundary with a
wide product: `floor(amount * percent / 100)` for positive inputs.
`market_price_after_fee` separately truncates the final adjusted price,
`floor(base * (100 +/- fee) / 100)`, matching the existing buy/sell contract.
Focused 99-resource fixtures pin the otherwise easy-to-hide truncation
(30%→29, Coinage 20%→19, Guilds buy 113 and sell 84 at base 99)
through direct helper, simulation, and replay coverage. DAT proves only the
raw Coinage `0.20`, Banking `0`, and Guilds `0.15` commands; the base 30% and
their runtime meanings remain reconstruction;
truncation remains reconstruction until virtual `+0x154` is independently
resolved.

## Represented route and price contracts

Trade Cart routes connect an owned completed living Market to a completed
living Market owned by a different allied player. Trade Cog uses the same
contract with Docks and sailable perimeter destinations. Enemy, neutral,
self-owned, wrong-kind, incomplete, dead, missing, or unreachable endpoints
are rejected.

At each endpoint the unit waits three simulation ticks, or two with Caravan,
then turns around. Gold is paid only on arrival back at the home endpoint.
Represented payout is:

```text
max(1, 2 * Manhattan distance between endpoint top-left tiles)
```

DAT proves neither this formula nor its top-left geometry, multiplier,
rounding, home-payment timing, or turnaround state machine. All are explicit
deterministic reconstruction policy. Caravan's DAT-proved 1.5 movement
multiplier also applies during travel; it changes trip duration, not payout.

Route state stores home and target IDs, direction, endpoint wait state, and
remaining work ticks. Save/load preserves those fields. Replay records only
the public route command; identical entity IDs and simulation state reproduce
the same route. Every update revalidates both endpoints and diplomacy, even
while the trader is moving. Invalid routes clear IDs, waiting state, path, and
movement immediately and award no gold.

Market exchange uses a reconstruction base price starting at 100. Buying adds
the player fee and raises shared base price by three; selling subtracts the
fee and lowers it by three, bounded to 20..999. Base fee is 30%; Guilds uses
its DAT raw 0.15 as 15%. The represented Saracen civilization bonus uses 5%.
These runtime formulas and price drift are not established by this trade DAT
extract.

No trade team-bonus effect is present in
`generated/trade_dat_metadata.json`, and this reconstruction has no separate
team-bonus roster or propagation model. No commercial team bonus is claimed
or guessed here.

Focused tests prove a 19-tile route pays 38 gold for both ordinary and Caravan
traders, Caravan finishes sooner, an enemy endpoint is rejected, diplomacy
change cancels a moving route before payout, and mid-trip save/load branches
finish with identical gold and direction. Existing tests cover Trade Cog
water routing, dynamic market prices, Guilds fees, tribute fees,
Coinage/Banking, and replay serialization.

## Graphics and sounds

Trade Cog attack 3971, idle 3975, and walk 3979 all map to SLP 2263; death
graphic 735 maps to SLP 2116. Selection sound 339 maps to WAV resource 5417,
training 338 to 5416, and command/movement 340 to resources 5413–5415.
Represented SDL playback uses SLP 2116 for Trade Cog death and sound 379 from
that death graphic, plus sound 338 when training completes.

Fish Trap standing graphic 3281 maps to SLP 3593 with six frames and one
angle. Construction graphic 5441 maps to SLP 4585 with one frame and three
angles. Represented SDL rendering uses both completed SLP 3593 and construction
SLP 4585. Selection sound 460 maps to WAV resource 6043.

These are DAT graphic-to-SLP and conceptual sound-to-WAV links. Presence and
decoding in supplied DRS archives require separate archive validation.

Dock `45` additionally records radius and construction radius `(1.5, 1.5)`,
center terrain requirements `(1, 4)`, perimeter tile requirements `(2, 35)`,
terrain restriction `6`, and selection shape `3`. Completed graphic `215` and
all three child graphics have one angle, so the DAT does not define a rotated
completed Dock facing.

## Regeneration

```sh
python3 tools/dat_metadata/generate_trade_metadata.py \
  /tmp/aoe-metadata.json
```

Live fixture comparison:

```sh
AOE_TEST_METADATA=/tmp/aoe-metadata.json \
  python3 -m unittest tools/dat_metadata/test_generate_trade_metadata.py
```

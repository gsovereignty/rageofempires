# Castle unique-technology evidence

## Exact research records

| Enum / DAT tech | Civilization | Prerequisite / age | Location / UI | Cost / time | Effect |
|---|---|---|---|---|---:|
| `yeomen` / 3 `British Yeoman` | British 1 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 750 wood, 450 gold / 60 seconds | 455 |
| `bearded_axe` / 83 `Frankish Bearded Axe` | French 2 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 400 food, 400 gold / 60 seconds | 291 |
| `anarchy` / 16 `Gothic Anarchy` | Goth 3 | tech 102, Castle | Castle 82, button 8, icon 107, no hotkey | 450 food, 250 gold / 40 seconds | 462 |
| `crenellations` / 11 `Teuton Crenellations` | German 4 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 600 food, 400 stone / 60 seconds | 461 |

Resource codes are 0 food, 1 wood, 2 stone, and 3 gold. Costs above are
research costs; none of the four effects changes a unit/building cost.
`icon_id=107` is the shared unique-technology button index. Tech records have
no per-tech sound or audio-resource field. Any research sound must come from
Castle/UI behavior outside these records.

## Effect command semantics

Effect type 4 modifies a unit selected by `a`, or every unit in class `b`
when `a=-1`. Parameter `c` is the unit attribute. Relevant attributes are:

| Attribute | Meaning |
|---:|---|
| 1 | line of sight |
| 9 | attack; `d` packs attack class and signed amount |
| 12 | maximum range |
| 23 | search radius |

Packed attack `d=770` decodes to class 3, amount +2
(`3 * 256 + 2`). Type 1 changes resource/control value `a` by `d`.
Type 2 enables unit `a` when `b=1`. Type 3 upgrades unit `a` to `b`.

### Yeomen: effect 455

| Type | Unit / class | Attribute | Amount | Result |
|---:|---|---:|---:|---|
| 4 | class 0 | 12 | +1 | maximum range +1 |
| 4 | class 0 | 1 | +1 | line of sight +1 |
| 4 | class 0 | 23 | +1 | search radius +1 |
| 4 | class 52 | 9 | packed 770 | class-3 attack +2 |

Thus live data gives class-0 archers +1 maximum range, LOS, and search radius.
It gives class-52 tower-family records +2 class-3 attack. Tower range is not
changed by Yeomen: no class-52 attribute-12 command exists. “Tower attack”
is exactly supported.

### Bearded Axe: effect 291

Six commands target Throwing Axeman 281 and Elite Throwing Axeman 531:

| Units | Attribute | Amount |
|---|---:|---:|
| 281 and 531 | 12 maximum range | +1 |
| 281 and 531 | 23 search radius | +1 |
| 281 and 531 | 1 line of sight | +1 |

Bearded Axe therefore gives both forms +1 effective maximum range, with
matching acquisition/vision increases. No attack or cost command exists.

### Anarchy: effects 462 and 463

Anarchy's direct effect 462 has zero commands. Its behavior is a two-tech
chain:

1. Research tech 16 `Gothic Anarchy`.
2. Hidden tech 18 `Barracks Huskarls`, civilization 3, requires tech 16,
   has zero time/no location, and invokes effect 463.
3. Effect 463 is type 2, `a=759`, `b=1`: enable unit 759.

Unit 759 is the Barracks Huskarl variant: trained at Barracks 12, button 4,
80 food/40 gold/one population, 16 seconds. It matches Castle Huskarl 41
combat data and art. Elite Huskarl effect 363 upgrades both Castle unit
41 to 555 and Barracks unit 759 to 761. Unit 761 remains at Barracks button
4 and matches Elite Huskarl 555.

This proves Anarchy enables Barracks production through hidden tech 18; an
empty direct effect does not mean no behavior.

### Crenellations: effect 461

| Type | Target | Attribute/control | Amount |
|---:|---|---|---:|
| 1 | resource/control 194 | direct modifier | encoded value 1 |
| 4 | Castle 82 | 12 maximum range | +3 |
| 4 | Castle 82 | 23 search radius | +3 |
| 4 | Castle 82 | 1 line of sight | +3 |

Crenellations unambiguously gives Castle 82 +3 maximum range, search radius,
and LOS. Resource/control 194 is the only encoded non-range toggle. It is
consistent with the special garrisoned-infantry firing behavior, but the
pinned parser provides no authoritative resource-name table; this document
keeps its exact numeric identity instead of inventing a label. No unit attack,
garrison capacity, or cost command exists. Research itself costs 600 food and
400 stone.

## Kataparuto, Rocketry, Logistica, and Mahouts

| Enum / DAT tech | Civilization | Prerequisite / age | Location / UI | Cost / time | Effect |
|---|---|---|---|---|---:|
| `kataparuto` / 59 `Japanese Kataparuto` | Japanese 5 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 750 wood, 400 gold / 60 seconds | 59 |
| `rocketry` / 52 `Chinese Rocketry` | Chinese 6 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 750 wood, 750 gold / 60 seconds | 483 |
| `logistica` / 61 `Byzantine Logistica` | Byzantine 7 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 1000 food, 600 gold / 50 seconds | 493 |
| `mahouts` / 7 `Persian Mahouts` | Persian 8 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 300 food, 300 gold / 50 seconds | 458 |

As above, tech records contain no per-tech sound link. All four use the shared
unique-technology icon index 107.

### Kataparuto: effect 59

| Type | Target | Attribute | Factor |
|---:|---|---:|---:|
| 5 | unpacked Trebuchet 42 | 13 work rate | x4 |
| 5 | unpacked Trebuchet 42 | 10 reload time | x0.75 |

Type 5 multiplies an attribute. The reload interval becomes 75 percent of its
old value, a 25 percent interval reduction. Work rate x4 is the encoded
pack/unpack-work acceleration. No command changes attack, range, creation
time, or cost. Packed Trebuchet 331 is not directly targeted, and its Castle
training record remains 200 wood/200 gold/50 seconds. Kataparuto therefore
does not accelerate Trebuchet training in these records.

Trebuchet 42/331 are represented as unpacked and packed `UnitKind` states.
Kataparuto reduces unpacked reload from 20 to 15 simulation ticks. DAT exposes
work rate x4 but no transform duration; core uses a documented deterministic
two-tick pack/unpack approximation, persisted and replayed without claiming
that duration as a DAT value.

### Rocketry: effect 483

| Type | Targets | Packed attack | Decoded result |
|---:|---|---:|---|
| 4 | Scorpion 279, Heavy Scorpion 542 | 772 | class 3 +4 |
| 4 | Chu Ko Nu 73, Elite Chu Ko Nu 559 | 770 | class 3 +2 |

Rocketry raises class-3 attack only. No maximum-range, search-radius, LOS, or
reload command exists. Both Chu Ko Nu and Scorpion lines are represented;
tech 239 upgrades Scorpion 279 to Heavy Scorpion 542.

### Logistica: effect 493

| Type | Targets | Attribute / packed value | Result |
|---:|---|---|---|
| 4 | Cataphract 40, Elite Cataphract 553 | 22 blast width, +0.5 | add 0.5 splash width/radius parameter |
| 4 | Cataphract 40, Elite Cataphract 553 | attack 262 | class 1 +6 |

Packed 262 decodes as `1 * 256 + 6`: +6 against attack/armor class 1,
the explicit infantry bonus. Attribute 22 adds 0.5 to both Cataphract
records. Their pre-tech unit records have area radius 0 and blast level 2;
Logistica supplies the missing 0.5 splash parameter. No effect command selects
a special attack graphic, projectile, or sound, so the ordinary Cataphract
attack art remains authoritative. Exact damage distribution within the radius
belongs to combat-engine blast rules, not this tech record.

Both Cataphract forms are represented by current `UnitKind`.

### Mahouts: effect 458

| Type | Targets | Attribute | Factor |
|---:|---|---:|---:|
| 5 | War Elephant 239, Elite War Elephant 558 | 5 movement speed | x1.3 |

Both records start at speed 0.6; direct multiplication yields approximately
0.78. Mahouts changes no HP, armor, attack, reload, training time, or cost.
Both Elephant forms are represented by current `UnitKind`.

## Zealotry, Artillery, Drill, and Berserkergang

| Enum / DAT tech | Civilization | Prerequisite / age | Location / UI | Cost / time | Effect |
|---|---|---|---|---|---:|
| `zealotry` / 9 `Saracen Zealotry` | Saracen 9 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 750 food, 800 gold / 50 seconds | 459 |
| `artillery` / 10 `Turkish Artillery` | Turk 10 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 500 gold, 450 stone / 40 seconds | 460 |
| `drill` / 6 `Mongol Siege Drill` | Mongol 12 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 500 wood, 450 gold / 60 seconds | 457 |
| `berserkergang` / 49 `Viking Berserkergang` | Viking 11 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 500 food, 850 gold / 40 seconds | 467 |

These records have no per-tech sound. Icon 107 remains the common Castle
unique-technology button.

### Zealotry: effect 459

Four type-4 commands add 30 to attribute 0, hit points:

| Targets | HP before | HP after |
|---|---:|---:|
| Mameluke 282 / Elite Mameluke 556 | 65 / 80 | 95 / 110 |
| Camel 329 / Elite Camel 330 | 100 / 120 | 130 / 150 |

No armor, attack, range, speed, training, or cost command exists. Mameluke and
Elite Mameluke are represented by current `UnitKind`; Camel 329/330 are not.

### Artillery: effect 460

Twelve type-4 commands target four records. Each receives +2 maximum range
(attribute 12), +2 LOS (attribute 1), and +2 search radius (attribute 23):

| Target | Base maximum range | After Artillery |
|---|---:|---:|
| Bombard Tower 236 | 8 | 10 |
| Bombard Cannon 36 | 12 | 14 |
| Cannon Galleon 420 | 13 | 15 |
| Elite Cannon Galleon 691 | 15 | 17 |

Thus Artillery changes range/acquisition/vision, not attack, reload, accuracy,
minimum range, training, or cost. Cannon Galleon and Elite Cannon Galleon are
represented by current `UnitKind`. Bombard Cannon and Bombard Tower are not
represented by current unit/building enums.

### Drill: effect 457

Two type-5 commands multiply movement speed by 1.5 for unit classes 13 and 55:

| Class | Live targets |
|---:|---|
| 13 | ram line, mangonel/onager line, Bombard Cannon |
| 55 | Scorpion and Heavy Scorpion |

Represented targets include Battering Ram 35, Mangonel/Onager/Siege Onager,
and Scorpion/Heavy Scorpion. Bombard Cannon and upgraded ram records remain
absent. Drill has no attack, reload, training-time, or cost command.

### Berserkergang: effect 467

The complete effect is one command:

| Type | Resource/control | Mode | Factor |
|---:|---:|---:|---:|
| 6 | 96 | 0 | 0.5 |

Type 6 is the resource-multiplier command. Live civilization resource slot 96
starts at 3.0 for Gaia and Vikings, so the command produces 1.5. This is a
timer reduction, not a direct HP addition: halving the regeneration interval
doubles regeneration frequency. The DAT does not contain a command against
Berserk 692/Elite 694 HP or work-rate attributes, and the pinned parser has no
public label for resource 96. Core should therefore preserve the exact
resource-96 multiplier/timer representation rather than hard-code an
unverified direct heal amount.

Berserk and Elite Berserk are represented by current `UnitKind`.

## Supremacy, Atheism, Shinkichon, and El Dorado

| Enum / DAT tech | Civilization | Prerequisite / age | Location / UI | Cost / time | Effect |
|---|---|---|---|---|---:|
| `supremacy` / 440 `Spanish Supremacy` | Spanish 14 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 400 food, 250 gold / 60 seconds | 495 |
| `atheism` / 21 `Hun Atheism` | Hun 17 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 500 food, 500 gold / 60 seconds | 464 |
| `shinkichon` / 445 `Korean catapults` | Korean 18 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 800 wood, 500 gold / 60 seconds | 506 |
| `el_dorado` / 4 `Mayan El Dorado` | Mayan 16 | tech 103, Imperial | Castle 82, button 8, icon 107, no hotkey | 750 food, 450 gold / 50 seconds | 456 |

`Shinkichon` is the public/UI technology name; DAT retains internal name
`Korean catapults`. None has a per-tech sound link.

### Supremacy: effect 495

All four type-4 commands target unit class 4, Villagers:

| Attribute | Packed value | Decoded result |
|---|---:|---|
| 9 attack | 1030 | class 4 +6 |
| 0 hit points | 40 | +40 HP |
| 8 armor | 1026 | armor class 4 +2 |
| 8 armor | 770 | armor class 3 +2 |

Live Villager records 83/293 begin at 25 HP, so Supremacy produces 65 HP.
It adds six to melee attack class 4 and two each to melee armor class 4 and
pierce armor class 3. Villager bonus classes, speed, work rate, range, and
cost remain unchanged. `UnitKind::villager` is represented.

### Atheism: effect 464

| Type | Resource/control | Mode | Encoded change |
|---:|---:|---:|---:|
| 1 | 196 | 1 | add 1000 |
| 1 | 197 | 0 | set 1 |

These are global victory-rule resources, not unit/building attributes.
Resource 196 receives the 1000-unit Wonder/Relic victory-timer extension;
resource 197 is enabled with value 1. Pinned `genie-rs` exposes neither
resource label, so numeric IDs and mode remain canonical. No command targets
a Wonder, relic object, unit, building, graphics, or sound. Core feedback
must therefore come from victory-timer/UI state, not a target animation.

Atheism affects no represented `UnitKind` or `BuildingKind` directly.

### Shinkichon: effect 506

Mangonel 280, Onager 550, and Siege Onager 588 each receive:

- maximum range (attribute 12) +1;
- LOS (attribute 1) +1;
- search radius (attribute 23) +1;
- attack attribute 9 packed value `-226`.

The signed 16-bit packed value `-226` has bytes `0xff1e`: signed class byte
`-1` (unsigned sentinel 255), amount +30. Unlike ordinary packed attacks,
this does not name a normal armor class. It is preserved as sentinel-class
attack metadata; interpreting it as +30 damage against buildings or another
ordinary class would be unsupported.

Ranges become 7 to 8 for Mangonel and 8 to 9 for both Onager forms.
All three Mangonel-line targets are represented.

### El Dorado: effect 456

Two type-4 commands add 40 hit points:

| Target | HP before | HP after |
|---|---:|---:|
| Eagle Warrior 751 | 50 | 90 |
| Elite Eagle Warrior 752 | 60 | 100 |

No attack, armor, speed, training, or cost command exists. Both Eagle forms
are represented and receive this HP increase.

## Extraction and validation

`tools/dat_metadata` emits each complete pinned-parser tech record as
`record`, names relevant effect attributes, and decodes packed attack class
and amount. Raw `a/b/c/d` values remain present for lossless consumers.

```sh
cargo run --quiet --manifest-path tools/dat_metadata/Cargo.toml -- \
  /path/to/empires2_x1_p1.dat > /tmp/aoe-metadata.json
cargo test --manifest-path tools/dat_metadata/Cargo.toml \
  --target-dir /tmp/aoe-dat-metadata-target
```

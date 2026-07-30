# Original versus reconstruction: 500 discrepancies

Date: 2026-07-30

## Scope and meaning

This bounded audit records exactly 500 evidenced differences between
the supplied 2013 runtime/data corpus and the reconstruction. A
discrepancy is any non-exact representation: confirmed bug, transformed
unit/scale, deliberate reconstruction policy, missing field, or open
fidelity gap. It is not automatically a defect and does not imply that
the decompiler recovered original source semantics.

Items 001-004 come from the movement audit against
`decompiled/AoK-HD-patched.c`, the matching supplied binary, and the
movement presentation contract. Items 005-500 are a deterministic
prefix of non-exact live VER 5.7 DAT comparisons in
`generated/core_rules_drift.json`. Exact rows are excluded.

Regenerate with:

```sh
python3 tools/generate_500_discrepancy_audit.py
```

## Runtime and binary discrepancies

| ID | Area | Reconstruction | Original evidence | Class | Note |
|---:|---|---|---|---|---|
| D001 | movement animation | moving art selected from pending move-order flag | floating-coordinate service path represents physical motion | fixed | 99d7782 gates moving art on current-tick displacement |
| D002 | blocked fractional movement | blocked ticks banked speed remainder | continuous speed evidence gives no basis for stored blocked bursts | fixed | 89e9f86 restores pre-tick accumulator on failed primary step |
| D003 | authoritative coordinates | integer tile positions | binary object coordinates are floating point | open | render interpolation only; sub-tile simulation remains absent |
| D004 | walking frame period | bounded 100 ms presentation period | per-graphic timing path exists but exact integrated cadence is unproved | open | exact graphic timing metadata not integrated |

## Live DAT discrepancies

| ID | Kind | Entity | Attribute | Reconstruction | VER 5.7 DAT | Class | Note |
|---:|---|---|---|---:|---:|---|---|
| D005 | unit | villager | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D006 | unit | villager | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D007 | unit | villager | training_ticks | 10 | 25 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D008 | unit | villager | speed | 100 | 0.800000011920929 | transformed | relative simulation percentage versus DAT tiles/second |
| D009 | unit | villager | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D010 | unit | villager | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D011 | unit | knight | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D012 | unit | knight | reload | 4 | 1.7999999523162842 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D013 | unit | knight | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D014 | unit | knight | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D015 | unit | knight | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D016 | unit | knight | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D017 | unit | archer | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D018 | unit | archer | training_ticks | 14 | 35 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D019 | unit | archer | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D020 | unit | archer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D021 | unit | archer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D022 | unit | scout_cavalry | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D023 | unit | scout_cavalry | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D024 | unit | scout_cavalry | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D025 | unit | scout_cavalry | speed | 100 | 1.2000000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D026 | unit | scout_cavalry | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D027 | unit | scout_cavalry | capacity | not in rules struct | 1 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D028 | unit | militia | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D029 | unit | militia | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D030 | unit | militia | training_ticks | 12 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D031 | unit | militia | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D032 | unit | militia | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D033 | unit | militia | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D034 | unit | spearman | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D035 | unit | spearman | reload | 5 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D036 | unit | spearman | training_ticks | 12 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D037 | unit | spearman | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D038 | unit | spearman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D039 | unit | spearman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D040 | unit | battering_ram | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D041 | unit | battering_ram | reload | 10 | 5.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D042 | unit | battering_ram | training_ticks | 18 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D043 | unit | battering_ram | speed | 100 | 0.5 | transformed | relative simulation percentage versus DAT tiles/second |
| D044 | unit | battering_ram | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D045 | unit | battering_ram | capacity | not in rules struct | 4 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D046 | unit | skirmisher | reload | 7 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D047 | unit | skirmisher | training_ticks | 9 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D048 | unit | skirmisher | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D049 | unit | skirmisher | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D050 | unit | skirmisher | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D051 | unit | mangonel | reload | 12 | 6.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D052 | unit | mangonel | training_ticks | 18 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D053 | unit | mangonel | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D054 | unit | mangonel | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D055 | unit | mangonel | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D056 | unit | man_at_arms | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D057 | unit | man_at_arms | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D058 | unit | man_at_arms | training_ticks | 12 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D059 | unit | man_at_arms | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D060 | unit | man_at_arms | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D061 | unit | man_at_arms | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D062 | unit | crossbowman | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D063 | unit | crossbowman | training_ticks | 11 | 27 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D064 | unit | crossbowman | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D065 | unit | crossbowman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D066 | unit | crossbowman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D067 | unit | pikeman | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D068 | unit | pikeman | reload | 5 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D069 | unit | pikeman | training_ticks | 12 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D070 | unit | pikeman | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D071 | unit | pikeman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D072 | unit | pikeman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D073 | unit | long_swordsman | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D074 | unit | long_swordsman | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D075 | unit | long_swordsman | training_ticks | 12 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D076 | unit | long_swordsman | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D077 | unit | long_swordsman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D078 | unit | long_swordsman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D079 | unit | cavalier | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D080 | unit | cavalier | reload | 4 | 1.7999999523162842 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D081 | unit | cavalier | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D082 | unit | cavalier | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D083 | unit | cavalier | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D084 | unit | cavalier | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D085 | unit | paladin | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D086 | unit | paladin | reload | 4 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D087 | unit | paladin | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D088 | unit | paladin | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D089 | unit | paladin | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D090 | unit | paladin | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D091 | unit | light_cavalry | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D092 | unit | light_cavalry | vision_range | 8 | 4.0 | intentionally_policy | rules record is the researched form including upgrade LOS effects |
| D093 | unit | light_cavalry | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D094 | unit | light_cavalry | training_ticks | 6 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D095 | unit | light_cavalry | speed | 100 | 1.5 | transformed | relative simulation percentage versus DAT tiles/second |
| D096 | unit | light_cavalry | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D097 | unit | light_cavalry | capacity | not in rules struct | 1 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D098 | unit | hussar | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D099 | unit | hussar | vision_range | 10 | 4.0 | intentionally_policy | rules record is the researched form including upgrade LOS effects |
| D100 | unit | hussar | reload | 4 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D101 | unit | hussar | training_ticks | 6 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D102 | unit | hussar | speed | 100 | 1.5 | transformed | relative simulation percentage versus DAT tiles/second |
| D103 | unit | hussar | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D104 | unit | hussar | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D105 | unit | two_handed_swordsman | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D106 | unit | two_handed_swordsman | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D107 | unit | two_handed_swordsman | training_ticks | 12 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D108 | unit | two_handed_swordsman | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D109 | unit | two_handed_swordsman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D110 | unit | two_handed_swordsman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D111 | unit | champion | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D112 | unit | champion | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D113 | unit | champion | training_ticks | 12 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D114 | unit | champion | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D115 | unit | champion | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D116 | unit | champion | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D117 | unit | arbalester | reload | 5 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D118 | unit | arbalester | training_ticks | 11 | 27 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D119 | unit | arbalester | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D120 | unit | arbalester | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D121 | unit | arbalester | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D122 | unit | elite_skirmisher | reload | 6 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D123 | unit | elite_skirmisher | training_ticks | 9 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D124 | unit | elite_skirmisher | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D125 | unit | elite_skirmisher | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D126 | unit | elite_skirmisher | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D127 | unit | sheep | food_cost | 0 | 50 | intentionally_policy | non-trainable map object; DAT creation cost is never charged |
| D128 | unit | sheep | reload | 5 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D129 | unit | sheep | training_ticks | 0 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D130 | unit | sheep | speed | 100 | 0.699999988079071 | transformed | relative simulation percentage versus DAT tiles/second |
| D131 | unit | sheep | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D132 | unit | sheep | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D133 | unit | deer | food_cost | 0 | 50 | intentionally_policy | non-trainable map object; DAT creation cost is never charged |
| D134 | unit | deer | reload | 5 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D135 | unit | deer | training_ticks | 0 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D136 | unit | deer | speed | 100 | 0.7369999885559082 | transformed | relative simulation percentage versus DAT tiles/second |
| D137 | unit | deer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D138 | unit | deer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D139 | unit | boar | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D140 | unit | boar | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D141 | unit | boar | training_ticks | 0 | 0 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D142 | unit | boar | speed | 100 | 0.800000011920929 | transformed | relative simulation percentage versus DAT tiles/second |
| D143 | unit | boar | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D144 | unit | boar | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D145 | unit | monk | reload | 20 | 1.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D146 | unit | monk | training_ticks | 12 | 51 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D147 | unit | monk | speed | 100 | 0.699999988079071 | transformed | relative simulation percentage versus DAT tiles/second |
| D148 | unit | monk | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D149 | unit | monk | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D150 | unit | relic | food_cost | 0 | 50 | intentionally_policy | non-trainable map object; DAT creation cost is never charged |
| D151 | unit | relic | reload | 0 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D152 | unit | relic | training_ticks | 0 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D153 | unit | relic | speed | 100 | 0.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D154 | unit | relic | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D155 | unit | relic | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D156 | unit | trade_cart | reload | 0 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D157 | unit | trade_cart | training_ticks | 12 | 51 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D158 | unit | trade_cart | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D159 | unit | trade_cart | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D160 | unit | trade_cart | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D161 | unit | fishing_ship | reload | 0 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D162 | unit | fishing_ship | training_ticks | 12 | 40 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D163 | unit | fishing_ship | speed | 100 | 1.2599999904632568 | transformed | relative simulation percentage versus DAT tiles/second |
| D164 | unit | fishing_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D165 | unit | fishing_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D166 | unit | galley | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D167 | unit | galley | training_ticks | 12 | 60 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D168 | unit | galley | speed | 100 | 1.4299999475479126 | transformed | relative simulation percentage versus DAT tiles/second |
| D169 | unit | galley | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D170 | unit | galley | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D171 | unit | war_galley | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D172 | unit | war_galley | training_ticks | 12 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D173 | unit | war_galley | speed | 100 | 1.4299999475479126 | transformed | relative simulation percentage versus DAT tiles/second |
| D174 | unit | war_galley | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D175 | unit | war_galley | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D176 | unit | galleon | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D177 | unit | galleon | training_ticks | 12 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D178 | unit | galleon | speed | 100 | 1.4299999475479126 | transformed | relative simulation percentage versus DAT tiles/second |
| D179 | unit | galleon | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D180 | unit | galleon | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D181 | unit | transport_ship | reload | 0 | 0.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D182 | unit | transport_ship | training_ticks | 12 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D183 | unit | transport_ship | speed | 100 | 1.4500000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D184 | unit | transport_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D185 | unit | transport_ship | capacity | not in rules struct | 5 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D186 | unit | fire_ship | attack_range | 2 | 2.490000009536743 | transformed | integer grid reach bounds DAT 2.49-tile flame range |
| D187 | unit | fire_ship | reload | 1 | 0.25 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D188 | unit | fire_ship | training_ticks | 12 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D189 | unit | fire_ship | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D190 | unit | fire_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D191 | unit | fire_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D192 | unit | fast_fire_ship | attack_range | 2 | 2.490000009536743 | transformed | integer grid reach bounds DAT 2.49-tile flame range |
| D193 | unit | fast_fire_ship | reload | 1 | 0.25 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D194 | unit | fast_fire_ship | training_ticks | 12 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D195 | unit | fast_fire_ship | speed | 100 | 1.4299999475479126 | transformed | relative simulation percentage versus DAT tiles/second |
| D196 | unit | fast_fire_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D197 | unit | fast_fire_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D198 | unit | demolition_ship | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D199 | unit | demolition_ship | reload | 1 | 5.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D200 | unit | demolition_ship | training_ticks | 10 | 31 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D201 | unit | demolition_ship | speed | 100 | 1.600000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D202 | unit | demolition_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D203 | unit | demolition_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D204 | unit | heavy_demolition_ship | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D205 | unit | heavy_demolition_ship | reload | 1 | 5.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D206 | unit | heavy_demolition_ship | training_ticks | 10 | 31 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D207 | unit | heavy_demolition_ship | speed | 100 | 1.600000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D208 | unit | heavy_demolition_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D209 | unit | heavy_demolition_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D210 | unit | cannon_galleon | reload | 10 | 10.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D211 | unit | cannon_galleon | training_ticks | 15 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D212 | unit | cannon_galleon | speed | 100 | 1.100000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D213 | unit | cannon_galleon | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D214 | unit | cannon_galleon | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D215 | unit | elite_cannon_galleon | reload | 10 | 10.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D216 | unit | elite_cannon_galleon | training_ticks | 15 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D217 | unit | elite_cannon_galleon | speed | 100 | 1.100000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D218 | unit | elite_cannon_galleon | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D219 | unit | elite_cannon_galleon | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D220 | unit | longboat | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D221 | unit | longboat | training_ticks | 8 | 25 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D222 | unit | longboat | speed | 100 | 1.5399999618530273 | transformed | relative simulation percentage versus DAT tiles/second |
| D223 | unit | longboat | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D224 | unit | longboat | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D225 | unit | elite_longboat | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D226 | unit | elite_longboat | training_ticks | 8 | 25 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D227 | unit | elite_longboat | speed | 100 | 1.5399999618530273 | transformed | relative simulation percentage versus DAT tiles/second |
| D228 | unit | elite_longboat | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D229 | unit | elite_longboat | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D230 | unit | turtle_ship | reload | 6 | 6.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D231 | unit | turtle_ship | training_ticks | 17 | 50 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D232 | unit | turtle_ship | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D233 | unit | turtle_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D234 | unit | turtle_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D235 | unit | elite_turtle_ship | reload | 6 | 6.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D236 | unit | elite_turtle_ship | training_ticks | 17 | 50 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D237 | unit | elite_turtle_ship | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D238 | unit | elite_turtle_ship | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D239 | unit | elite_turtle_ship | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D240 | unit | longbowman | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D241 | unit | longbowman | training_ticks | 6 | 19 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D242 | unit | longbowman | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D243 | unit | longbowman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D244 | unit | longbowman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D245 | unit | elite_longbowman | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D246 | unit | elite_longbowman | training_ticks | 6 | 19 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D247 | unit | elite_longbowman | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D248 | unit | elite_longbowman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D249 | unit | elite_longbowman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D250 | unit | throwing_axeman | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D251 | unit | throwing_axeman | training_ticks | 6 | 17 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D252 | unit | throwing_axeman | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D253 | unit | throwing_axeman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D254 | unit | throwing_axeman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D255 | unit | elite_throwing_axeman | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D256 | unit | elite_throwing_axeman | training_ticks | 6 | 17 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D257 | unit | elite_throwing_axeman | speed | 100 | 0.8999999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D258 | unit | elite_throwing_axeman | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D259 | unit | elite_throwing_axeman | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D260 | unit | huskarl | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D261 | unit | huskarl | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D262 | unit | huskarl | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D263 | unit | huskarl | speed | 100 | 1.0499999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D264 | unit | huskarl | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D265 | unit | huskarl | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D266 | unit | elite_huskarl | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D267 | unit | elite_huskarl | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D268 | unit | elite_huskarl | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D269 | unit | elite_huskarl | speed | 100 | 1.0499999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D270 | unit | elite_huskarl | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D271 | unit | elite_huskarl | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D272 | unit | teutonic_knight | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D273 | unit | teutonic_knight | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D274 | unit | teutonic_knight | training_ticks | 4 | 12 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D275 | unit | teutonic_knight | speed | 100 | 0.6499999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D276 | unit | teutonic_knight | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D277 | unit | teutonic_knight | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D278 | unit | elite_teutonic_knight | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D279 | unit | elite_teutonic_knight | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D280 | unit | elite_teutonic_knight | training_ticks | 4 | 12 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D281 | unit | elite_teutonic_knight | speed | 100 | 0.6499999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D282 | unit | elite_teutonic_knight | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D283 | unit | elite_teutonic_knight | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D284 | unit | samurai | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D285 | unit | samurai | reload | 2 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D286 | unit | samurai | training_ticks | 3 | 9 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D287 | unit | samurai | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D288 | unit | samurai | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D289 | unit | samurai | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D290 | unit | elite_samurai | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D291 | unit | elite_samurai | reload | 2 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D292 | unit | elite_samurai | training_ticks | 3 | 9 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D293 | unit | elite_samurai | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D294 | unit | elite_samurai | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D295 | unit | elite_samurai | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D296 | unit | chu_ko_nu | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D297 | unit | chu_ko_nu | training_ticks | 6 | 19 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D298 | unit | chu_ko_nu | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D299 | unit | chu_ko_nu | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D300 | unit | chu_ko_nu | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D301 | unit | elite_chu_ko_nu | reload | 3 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D302 | unit | elite_chu_ko_nu | training_ticks | 4 | 13 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D303 | unit | elite_chu_ko_nu | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D304 | unit | elite_chu_ko_nu | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D305 | unit | elite_chu_ko_nu | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D306 | unit | cataphract | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D307 | unit | cataphract | reload | 2 | 1.7999999523162842 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D308 | unit | cataphract | training_ticks | 7 | 20 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D309 | unit | cataphract | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D310 | unit | cataphract | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D311 | unit | cataphract | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D312 | unit | elite_cataphract | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D313 | unit | elite_cataphract | reload | 2 | 1.7000000476837158 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D314 | unit | elite_cataphract | training_ticks | 7 | 20 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D315 | unit | elite_cataphract | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D316 | unit | elite_cataphract | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D317 | unit | elite_cataphract | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D318 | unit | war_elephant | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D319 | unit | war_elephant | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D320 | unit | war_elephant | training_ticks | 10 | 31 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D321 | unit | war_elephant | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D322 | unit | war_elephant | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D323 | unit | war_elephant | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D324 | unit | elite_war_elephant | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D325 | unit | elite_war_elephant | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D326 | unit | elite_war_elephant | training_ticks | 10 | 31 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D327 | unit | elite_war_elephant | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D328 | unit | elite_war_elephant | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D329 | unit | elite_war_elephant | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D330 | unit | mameluke | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D331 | unit | mameluke | training_ticks | 8 | 23 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D332 | unit | mameluke | speed | 100 | 1.399999976158142 | transformed | relative simulation percentage versus DAT tiles/second |
| D333 | unit | mameluke | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D334 | unit | mameluke | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D335 | unit | elite_mameluke | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D336 | unit | elite_mameluke | training_ticks | 8 | 23 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D337 | unit | elite_mameluke | speed | 100 | 1.399999976158142 | transformed | relative simulation percentage versus DAT tiles/second |
| D338 | unit | elite_mameluke | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D339 | unit | elite_mameluke | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D340 | unit | janissary | reload | 3 | 3.450000047683716 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D341 | unit | janissary | training_ticks | 7 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D342 | unit | janissary | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D343 | unit | janissary | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D344 | unit | janissary | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D345 | unit | elite_janissary | reload | 3 | 3.450000047683716 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D346 | unit | elite_janissary | training_ticks | 7 | 21 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D347 | unit | elite_janissary | speed | 100 | 0.9599999785423279 | transformed | relative simulation percentage versus DAT tiles/second |
| D348 | unit | elite_janissary | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D349 | unit | elite_janissary | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D350 | unit | berserk | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D351 | unit | berserk | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D352 | unit | berserk | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D353 | unit | berserk | speed | 100 | 1.0499999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D354 | unit | berserk | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D355 | unit | berserk | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D356 | unit | elite_berserk | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D357 | unit | elite_berserk | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D358 | unit | elite_berserk | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D359 | unit | elite_berserk | speed | 100 | 1.0499999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D360 | unit | elite_berserk | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D361 | unit | elite_berserk | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D362 | unit | mangudai | reload | 2 | 2.0999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D363 | unit | mangudai | training_ticks | 9 | 26 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D364 | unit | mangudai | speed | 100 | 1.4500000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D365 | unit | mangudai | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D366 | unit | mangudai | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D367 | unit | elite_mangudai | reload | 2 | 2.0999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D368 | unit | elite_mangudai | training_ticks | 9 | 26 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D369 | unit | elite_mangudai | speed | 100 | 1.4500000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D370 | unit | elite_mangudai | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D371 | unit | elite_mangudai | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D372 | unit | jaguar_warrior | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D373 | unit | jaguar_warrior | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D374 | unit | jaguar_warrior | training_ticks | 7 | 20 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D375 | unit | jaguar_warrior | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D376 | unit | jaguar_warrior | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D377 | unit | jaguar_warrior | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D378 | unit | elite_jaguar_warrior | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D379 | unit | elite_jaguar_warrior | reload | 2 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D380 | unit | elite_jaguar_warrior | training_ticks | 7 | 20 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D381 | unit | elite_jaguar_warrior | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D382 | unit | elite_jaguar_warrior | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D383 | unit | elite_jaguar_warrior | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D384 | unit | plumed_archer | reload | 2 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D385 | unit | plumed_archer | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D386 | unit | plumed_archer | speed | 100 | 1.2000000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D387 | unit | plumed_archer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D388 | unit | plumed_archer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D389 | unit | elite_plumed_archer | reload | 2 | 1.899999976158142 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D390 | unit | elite_plumed_archer | training_ticks | 5 | 16 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D391 | unit | elite_plumed_archer | speed | 100 | 1.2000000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D392 | unit | elite_plumed_archer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D393 | unit | elite_plumed_archer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D394 | unit | conquistador | reload | 3 | 2.9000000953674316 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D395 | unit | conquistador | training_ticks | 8 | 24 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D396 | unit | conquistador | speed | 100 | 1.2999999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D397 | unit | conquistador | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D398 | unit | conquistador | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D399 | unit | elite_conquistador | reload | 3 | 2.9000000953674316 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D400 | unit | elite_conquistador | training_ticks | 8 | 24 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D401 | unit | elite_conquistador | speed | 100 | 1.2999999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D402 | unit | elite_conquistador | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D403 | unit | elite_conquistador | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D404 | unit | tarkan | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D405 | unit | tarkan | reload | 2 | 2.0999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D406 | unit | tarkan | training_ticks | 5 | 14 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D407 | unit | tarkan | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D408 | unit | tarkan | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D409 | unit | tarkan | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D410 | unit | elite_tarkan | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D411 | unit | elite_tarkan | reload | 2 | 2.0999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D412 | unit | elite_tarkan | training_ticks | 5 | 14 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D413 | unit | elite_tarkan | speed | 100 | 1.350000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D414 | unit | elite_tarkan | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D415 | unit | elite_tarkan | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D416 | unit | eagle_warrior | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D417 | unit | eagle_warrior | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D418 | unit | eagle_warrior | training_ticks | 14 | 35 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D419 | unit | eagle_warrior | speed | 100 | 1.100000023841858 | transformed | relative simulation percentage versus DAT tiles/second |
| D420 | unit | eagle_warrior | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D421 | unit | eagle_warrior | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D422 | unit | elite_eagle_warrior | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D423 | unit | elite_eagle_warrior | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D424 | unit | elite_eagle_warrior | training_ticks | 8 | 20 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D425 | unit | elite_eagle_warrior | speed | 100 | 1.2999999523162842 | transformed | relative simulation percentage versus DAT tiles/second |
| D426 | unit | elite_eagle_warrior | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D427 | unit | elite_eagle_warrior | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D428 | unit | scorpion | reload | 7 | 3.5999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D429 | unit | scorpion | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D430 | unit | scorpion | speed | 100 | 0.6499999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D431 | unit | scorpion | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D432 | unit | scorpion | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D433 | unit | heavy_scorpion | reload | 7 | 3.5999999046325684 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D434 | unit | heavy_scorpion | training_ticks | 12 | 30 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D435 | unit | heavy_scorpion | speed | 100 | 0.6499999761581421 | transformed | relative simulation percentage versus DAT tiles/second |
| D436 | unit | heavy_scorpion | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D437 | unit | heavy_scorpion | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D438 | unit | onager | reload | 12 | 6.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D439 | unit | onager | training_ticks | 18 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D440 | unit | onager | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D441 | unit | onager | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D442 | unit | onager | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D443 | unit | siege_onager | reload | 12 | 6.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D444 | unit | siege_onager | training_ticks | 18 | 46 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D445 | unit | siege_onager | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D446 | unit | siege_onager | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D447 | unit | siege_onager | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D448 | unit | packed_trebuchet | attack_range | 1 | 16.0 | intentionally_policy | packed state is non-attacking; DAT inherits unpacked weapon fields |
| D449 | unit | packed_trebuchet | minimum_attack_range | 0 | 4.0 | intentionally_policy | packed state is non-attacking; DAT inherits unpacked weapon fields |
| D450 | unit | packed_trebuchet | reload | 20 | 10.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D451 | unit | packed_trebuchet | training_ticks | 20 | 50 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D452 | unit | packed_trebuchet | speed | 100 | 0.800000011920929 | transformed | relative simulation percentage versus DAT tiles/second |
| D453 | unit | packed_trebuchet | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D454 | unit | packed_trebuchet | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D455 | unit | trebuchet | reload | 20 | 10.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D456 | unit | trebuchet | training_ticks | 20 | 50 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D457 | unit | trebuchet | speed | 100 | 0.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D458 | unit | trebuchet | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D459 | unit | trebuchet | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D460 | unit | cavalry_archer | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D461 | unit | cavalry_archer | training_ticks | 14 | 34 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D462 | unit | cavalry_archer | speed | 100 | 1.399999976158142 | transformed | relative simulation percentage versus DAT tiles/second |
| D463 | unit | cavalry_archer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D464 | unit | cavalry_archer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D465 | unit | heavy_cavalry_archer | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D466 | unit | heavy_cavalry_archer | training_ticks | 11 | 27 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D467 | unit | heavy_cavalry_archer | speed | 100 | 1.399999976158142 | transformed | relative simulation percentage versus DAT tiles/second |
| D468 | unit | heavy_cavalry_archer | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D469 | unit | heavy_cavalry_archer | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D470 | unit | camel_rider | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D471 | unit | camel_rider | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D472 | unit | camel_rider | training_ticks | 9 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D473 | unit | camel_rider | speed | 100 | 1.4500000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D474 | unit | camel_rider | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D475 | unit | camel_rider | capacity | not in rules struct | 1 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D476 | unit | heavy_camel | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D477 | unit | heavy_camel | reload | 4 | 2.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D478 | unit | heavy_camel | training_ticks | 9 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D479 | unit | heavy_camel | speed | 100 | 1.4500000476837158 | transformed | relative simulation percentage versus DAT tiles/second |
| D480 | unit | heavy_camel | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D481 | unit | heavy_camel | capacity | not in rules struct | 1 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D482 | unit | capped_ram | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D483 | unit | capped_ram | reload | 10 | 5.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D484 | unit | capped_ram | training_ticks | 18 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D485 | unit | capped_ram | speed | 100 | 0.5 | transformed | relative simulation percentage versus DAT tiles/second |
| D486 | unit | capped_ram | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D487 | unit | capped_ram | capacity | not in rules struct | 4 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D488 | unit | siege_ram | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D489 | unit | siege_ram | reload | 10 | 5.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D490 | unit | siege_ram | training_ticks | 18 | 36 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D491 | unit | siege_ram | speed | 100 | 0.6000000238418579 | transformed | relative simulation percentage versus DAT tiles/second |
| D492 | unit | siege_ram | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D493 | unit | siege_ram | capacity | not in rules struct | 6 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D494 | unit | halberdier | attack_range | 1 | 0.0 | transformed | engine contact reach is one grid tile; DAT contact range is zero |
| D495 | unit | halberdier | reload | 6 | 3.0 | transformed | simulation ticks versus DAT seconds; no inverse guessed |
| D496 | unit | halberdier | training_ticks | 12 | 22 | transformed | bounded simulation ticks versus DAT seconds; no inverse guessed |
| D497 | unit | halberdier | speed | 100 | 1.0 | transformed | relative simulation percentage versus DAT tiles/second |
| D498 | unit | halberdier | population | not in rules struct | 0 | intentionally_policy | population accounting is outside UnitRules |
| D499 | unit | halberdier | capacity | not in rules struct | 0 | intentionally_policy | capacity is represented by simulation policy, not this rules struct |
| D500 | unit | hand_cannoneer | reload | 7 | 3.450000047683716 | transformed | simulation ticks versus DAT seconds; no inverse guessed |

## Count gate

- Runtime/binary discrepancies: 4
- Live DAT discrepancies: 496
- Total: 500

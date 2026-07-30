# Teuton conversion-resistance evidence

## Result

Original HD conversion arithmetic, Teuton team-effect payload, and resource
effect modes are pinned. Team initialization applies each distinct eligible
civilization bonus once per recipient. Two eligible Teuton players therefore
still produce one Teuton team-effect application per recipient.

Executable is `AoK HD.exe`, SHA-256
`e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`.
Conversion class RTTI is `.?AVTRIBE_Action_Convert@@`; type descriptor is
`0x81627c`, complete-object locator `0x80534c`, vtable `0x772acc`, and main
conversion action function `0x413a80`.

## DAT team effect

Civilization index 4, DAT name `Germans`, has `bonus_effect: Some(404)`.
Effect 404 contains exactly:

- type 1, resource 77, mode 1, value 2;
- type 1, resource 178, mode 0, value 1;
- type 1, resource 179, mode 0, value 2.

The type-1 dispatcher at `0x4f73e0` sends resource ID, amount, and the low byte
of mode to `0x5839c0`. Mode zero selects the `0x5a4d70`/`0x582860` path, which
assigns the resource float. Any nonzero mode selects the
`0x5a4e90`/`0x582940` path, which adds the resource float. Thus one application
to zero-valued resources produces exactly:

- resource 77 = 2;
- resource 178 = 1;
- resource 179 = 2.

Player construction at `0x5856a0` stores the civilization effect at player
offset `0x168` and team effect at `0x16c`. The constructor applies the
civilization effect at `0x585c2d..0x585c48` through effect-manager virtual
slot `+8`, recovered as `0x51c990`.

An exhaustive decoded-instruction search found no later read of player field
`+0x16c`; team initialization instead reads civilization records directly.
At `0x54a740..0x54a891`, each recipient gets its civilization effect, then a
zeroed byte bitmap indexed by civilization ID is built. Recipient civilization
is marked. Other players qualify through player virtual slot `+0x1c`, recovered
as `0x581dc0`; it returns true exactly when the diplomacy byte for that player
is `3`. Qualifying players' civilization IDs are marked. Initialization then
iterates civilization IDs, reads bonus-effect ID at civilization-record
offset `+0x2c`, and applies each marked nonnegative effect once through effect
manager at `0x54a856..0x54a868`.

Bitmap makes duplicate rule exact: same-civilization eligible players do not
stack. Each recipient gets every distinct eligible civilization's team bonus
once, including its own civilization. Thus any eligible team containing one
or more Teutons applies effect 404 once per recipient.

The bidirectional player serialization routine at `0x582aa0` transfers the
resource count at `+0xa4`, then exactly that many contiguous float32 resource
values through the array pointer at `+0xa8`. Resource 77/178/179 values are
therefore part of saved player state. Whether a later load-finalization path
reapplies team effects is still unproved.

## Conversion check

At `0x413e2c`, one CRT random value is drawn after conversion event emission.
Executable scales it as integer `floor(rand * 100 / 32767)`, producing
0 through 100.

Resistance accumulator starts at zero:

- add 3 when target class is 2, 20, 21, 22, or 53, or target record flag
  `+0x97` is 2 or 10, unless converter class is `0x35`;
- add 8 for unit IDs 448, 546, 441, 751, or 752;
- add target-player resource 77 when nonzero.

If accumulator is positive, `0x413f19` multiplies scaled integer roll by that
float and `0x72421c` converts result back to integer using active x87 rounding
environment. No resistance clamp exists.

For ordinary targets, base chance is converter unit-record field `+0x14a`;
base minimum/maximum come from conversion task fields `+0x1c/+0x20`. Target
classes 3 and `0x34` instead use converter resources 182/180/181 as base
chance/minimum/maximum. Final minimum adds converter resource 176 and target
resource 178. Final maximum adds converter resource 177 and target resource
179.

If elapsed time is below minimum, threshold becomes -1000. At or above maximum,
threshold becomes 1000. Otherwise base chance remains. Conversion succeeds
when multiplied integer roll is less than or equal to threshold at `0x413feb`.

Implementation-shape pseudocode:

```text
roll = floor(rand() * 100 / 32767)
resistance = class_resistance(target, converter) +
             special_id_resistance(target) +
             target.player.resource[77]
if resistance > 0:
    roll = x87_float_to_int(roll * resistance)

if target.class in {3, 0x34}:
    chance = converter.player.resource[182]
    minimum = converter.player.resource[180]
    maximum = converter.player.resource[181]
else:
    chance = converter.unit_type.field_0x14a
    minimum = conversion_task.field_0x1c
    maximum = conversion_task.field_0x20

minimum += converter.player.resource[176] + target.player.resource[178]
maximum += converter.player.resource[177] + target.player.resource[179]
threshold = -1000 if elapsed < minimum else
            1000 if elapsed >= maximum else chance
success = roll <= threshold
```

`evaluate_conversion_check` in `game_rules` now implements exact bounded
arithmetic once caller supplies recovered inputs: CRT roll scaling, positive
resistance multiplication, active rounding-mode conversion, minimum/maximum
boundaries, and inclusive threshold comparison. Simulation still lacks proved
global CRT RNG sequencing and complete DAT class/ID/resource transport, so its
existing conversion scheduler is not relabeled exact.

Generated evidence lives in
[`generated/teuton_conversion_evidence.json`](../../generated/teuton_conversion_evidence.json).

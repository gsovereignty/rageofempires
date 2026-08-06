# Unit and building animation fidelity

## Exhaustive evidence catalog

`generated/animation_evidence.json` joins the validated VER 5.7 DAT, live
`graphics.drs`, and every represented gameplay kind: 96 units and 27
buildings. Seven graphic roles are recorded for each kind:

- standing and alternate standing;
- walking and running;
- attack and dying;
- construction.

For every referenced graphic, the catalog preserves the exact DAT graphic ID,
SLP ID, frames per angle, angle count, frame duration, replay delay, sequence
type, mirroring mode, layer, and delta records. The live SLP header supplies
the physical frame count independently.

| Evidence classification | Units | Buildings | Total |
|---|---:|---:|---:|
| exact DAT plus matching live SLP layout | 330 | 70 | 400 |
| ambiguous missing/mismatched live layout | 45 | 5 | 50 |
| absent DAT role or SLP identity | 283 | 114 | 397 |

`exact` requires the live physical frame count to match either the complete
DAT frame/angle product or the mirrored stored-angle product. For example,
Villager standing graphic 1284 declares 15 frames, eight angles, and mirror
mode 6; live SLP 1479 contains 75 frames, exactly five stored angles.

`ambiguous` is fail-closed. It includes absent supplied SLP payloads and
physical counts that match neither complete nor mirrored DAT layouts.
Examples include Sheep attack SLP 3623 absent from the supplied archive and
Archer standing SLP 8 containing 52 physical frames against DAT 10 frames,
eight angles, mirror mode 6.

## Cadence and action synchronization

The DAT `frame_rate` field is cataloged as seconds per frame under the pinned
openage DAT schema. `replay_delay`, `sequence_type`, and `mirror_flag` remain
separate exact fields; they are not collapsed into a reconstruction timer.

Ninety-one represented records expose an attack graphic and therefore exact
raw `frame_delay` and `reload_time` fields. Thirty have no attack graphic.
Twenty-five of the 91 attack records carry a nonzero frame delay.

The supplied HD decompilation now closes the scheduler path. `FUN_0056c500`
passes the authoritative world delta into the animated-object update.
`FUN_004eb870` initializes the graphic frame count, duration (with a 0.001
second minimum), sequence flags, and optional randomized initial frame.
`FUN_004ebb90` advances only when sequence bit 0 is set, applies replay delay,
and clamps a sequence with bit 3 on its final frame. The moving-object override
`FUN_0057b620` sets current movement speed before calling that same base path.

`RGE_Action_Attack::FUN_00407910` separately reads the master-object DAT attack
frame delay and does not launch or apply the attack until the active graphic
reaches that frame. A zero delay releases immediately; `-1` selects the active
animation path rather than supplying a positive frame number.

The reconstruction's authoritative world update is 0.2 seconds. Exact DAT
frame durations are now evaluated in that same elapsed-time domain, while SDL
interpolation supplies only the bounded partial interval between completed
world updates. Nonzero attack delays are rounded up to the first five-Hz update
that reaches the DAT frame. The 25 represented nonzero-delay records are bound
to their exact DAT IDs; zero-delay records retain immediate release.

## Runtime integration boundary

The pure `aoe::animation` contract now consumes the exact catalog subset. It
validates full versus mirrored physical layouts and exposes exact graphic,
SLP, frames-per-angle, angle-count, duration, and mirror fields for:

- Villager idle, move, attack, and hunting gather;
- Militia idle, move, and attack;
- Archer move and attack;
- Knight idle, move, and attack.

Archer idle remains procedural because SLP 8 has 52 physical frames rather
than either valid 50-frame mirrored or 80-frame full layout. Villager
build/repair remains procedural because SLP 1493 has no catalog provenance.
Hunting SLP 1528 is now selected only for sheep, deer, and boar targets rather
than incorrectly representing wood, farm, or mining work.

Represented buildings have no DAT attack graphic. Their exact standing body
continues while projectiles render separately. Construction roots whose
sequence bit 0 is clear remain driven by authoritative construction progress,
because the original animator does not auto-advance those graphics. Animated
construction and damage composites use the recovered scheduler when their DAT
root is exact.

The API keeps unresolved logical-direction conversion external:

```text
AnimationEvidence {
  graphic_id, slp_id,
  frames_per_angle, dat_angle_count, stored_angle_count,
  frame_duration_seconds, replay_delay_seconds,
  sequence_type, mirroring_mode, deltas,
  layout_classification
}
```

Production rendering parses `sequence_type` from the DAT instead of skipping
its byte. Exact unit, commercial-object, naval, work, death, projectile,
building-damage, and animated-construction bindings consume DAT frame duration,
replay delay, sequence advancement, sequence-gated initial phase, and
final-frame hold. Ambiguous SLP-to-graphic timing identity fails closed to the existing
fallback and remains an art-binding issue tracked by `BUG-ANIMATION-002`.

Each unit persists its current animation state and authoritative state-start
tick. Attack actions also persist their start tick, bound target/action key,
and remaining release delay. New orders cancel an unreleased action without
consuming reload; target movement after a valid windup starts does not retarget
or cancel that bound release. This keeps save/load, replay, and lockstep hashes
on the same attack frame and prevents a delayed projectile from hitting a newly
selected target.

Likewise, physical five-angle mirrored storage is exact, but the original
logical-direction and `display_angle` selector path remains unproved. Current
SDL mirroring stays procedural and is not labeled original-exact.

## Reproduction and tests

```sh
python3 tools/dat_metadata/generate_animation_evidence.py \
  /tmp/aoe-core-rules-metadata.json \
  /path/to/Data/graphics.drs
```

Focused tests pin all 121 records and 847 role outcomes, parsed sequence flags,
frame duration, replay hold, non-advancing and final-frame sequences, world-time
interpolation, exact high-use bindings, and all 25 nonzero attack-delay records.
The production simulation regression proves delayed Archer release, save/load
phase preservation, cancellation, retarget binding, and release against a
moving target. Deterministic SDL capture smoke exercises the shipped renderer
path; the full repository gate covers save, replay, lockstep, gameplay, and SDL
smoke tests.

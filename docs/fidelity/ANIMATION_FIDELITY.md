# Unit and building animation fidelity

## Exhaustive evidence catalog

`generated/animation_evidence.json` joins the validated VER 5.7 DAT, live
`graphics.drs`, and every represented gameplay kind: 97 units and 27
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
| exact DAT plus matching live SLP layout | 342 | 70 | 412 |
| ambiguous missing/mismatched live layout | 45 | 5 | 50 |
| absent DAT role or SLP identity | 292 | 114 | 406 |

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

Ninety-four represented records expose an attack graphic and therefore exact
raw `frame_delay` and `reload_time` fields. Thirty have no attack graphic.
Twenty-five of the 94 attack records carry a nonzero frame delay.

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

## Logical angle and physical frame selection

The supplied executable closes the direction path too. Animated objects store
their logical angle as a byte at object offset `0x35`; `FUN_0058b1d0` is the
setter. Graphic/action changes preserve and reapply that byte instead of
deriving direction from the most recent position delta.

`FUN_0058da80` normalizes the requested world vector and reference vector
`(+1,0,+1)`, uses dot/`acos` plus the cross-product sign, divides by one logical
angle step, and rounds to the nearest angle. Graphics with fewer than nine
angles use eight logical directions. Thus logical zero is southeast `(+x,+y)`,
followed by south, southwest, west, northwest, north, northeast, and east.

`FUN_00510160` consumes the stored angle and graphic fields at offsets `0x5e`
(frames per angle), `0x60` (angle count), and `0x74` (mirroring mode). The
mirroring byte is the inclusive end of the directly stored logical range, not
a boolean. For eight angles/mode 6, logical angles 2 through 6 select physical
slots 0 through 4 directly; angles 0, 1, and 7 select slots 2, 1, and 3 with
horizontal flip. Decompiled composite wrappers also prove that `display_angle`
filters a delta and a visible child uses integer-scaled angle
`child_count * root_angle / root_count`.

## Runtime integration boundary

The pure `aoe::animation` contract now compiles every exact catalog role into
the production binary. It validates full versus mirrored physical layouts and
exposes exact graphic, SLP, frames-per-angle, angle-count, duration, replay
delay, sequence type, mirror mode, and layer for all 412 exact direct roles.
The renderer's canonical unit table is derived from that generated catalog;
its former separate hand-written SLP table and the SDL loader's partial exact
subset are gone. Standard idle, walking, attack, and death selection therefore
uses the same proved binding identity that the loader validates against the
packaged DAT. Existing composite naval, siege, building, construction, damage,
and topology paths retain their separately evidenced exact roots.

The runtime catalog also contains two exact Villager action bindings recovered
from task-bearing original objects rather than guessed SLP numbers. Builder
object 118 task 101 and Repairer object 156 task 106 both select work graphic
1598, which maps to SLP 1496 with a valid 15-frame/eight-angle mirrored layout.
Production construction and repair now render that animation. Builder's
distinct farm task graphic 3364 remains fail-closed because live SLP 3842 has
80 frames, matching neither its declared full nor mirrored layout.

Archer idle likewise remains fail-closed because SLP 8 has 52 physical frames
rather than either valid 50-frame mirrored or 80-frame full layout. Hunting
SLP 1528 is selected only for sheep, deer, and boar targets rather than
incorrectly representing wood, farm, or mining work. The 50 ambiguous and 406
absent direct roles are explicitly unavailable evidence, not silently promoted
to exact art or substituted with a generic unit's role.

Represented buildings have no DAT attack graphic. Their exact standing body
continues while projectiles render separately. Construction roots whose
sequence bit 0 is clear remain driven by authoritative construction progress,
because the original animator does not auto-advance those graphics. Animated
construction and damage composites use the recovered scheduler when their DAT
root is exact.

The API carries the proved direction contract with the catalog:

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
final-frame hold. Ambiguous SLP-to-graphic timing identity fails closed to the
existing fallback.

Each unit persists its current animation state and authoritative state-start
tick. Attack actions also persist their start tick, bound target/action key,
and remaining release delay. New orders cancel an unreleased action without
consuming reload; target movement after a valid windup starts does not retarget
or cancel that bound release. This keeps save/load, replay, and lockstep hashes
on the same attack frame and prevents a delayed projectile from hitting a newly
selected target.

Units now persist logical facing through movement stops, action/graphic
transitions, work, attacks, death effects, saves, replays, and lockstep hashes.
Buildings retain facing and turn defensive launch state toward their target.
Direct, composite, shadow, and projectile renderers use the same recovered
selector; projectile 8/18/72-angle quantization no longer uses a separate
openage-derived transform. Horizontal mirroring uses the reflected anchor
`width - 1 - hotspot_x`.

## Reproduction and tests

```sh
python3 tools/dat_metadata/generate_animation_evidence.py \
  /tmp/aoe-core-rules-metadata.json \
  /path/to/Data/graphics.drs
```

Focused tests pin all 124 records and 868 direct-role outcomes, all 414 compiled
exact direct/task bindings, parsed sequence flags, frame duration, replay hold,
non-advancing and final-frame sequences, world-time interpolation, and all 25
nonzero attack-delay records. Direction tests pin every eight-angle physical
slot/flip result, 18- and 72-angle projectile layouts, child-angle scaling,
stationary action turning, stop-state preservation, and save/load facing.
The production simulation regression proves delayed Archer release, save/load
phase preservation, cancellation, retarget binding, and release against a
moving target. Deterministic SDL capture smoke exercises the shipped renderer
path; the full repository gate covers save, replay, lockstep, gameplay, and SDL
smoke tests. The exact-binding SDL smoke runs the packaged app and proves King
idle/movement SLPs 1767/1771, Woad Raider idle/movement SLPs 1598/1602, and
Villager construction/repair SLP 1496. The same smoke against a clean
`ca2964d` package failed: both Kings had no auditable legacy sprite and both
Villager work actions rendered standing SLP 1479. The corrected package passes
the identical scenarios, commands, ticks, and overlap-capture assertions.

For BUG-ANIMATION-003, the same packaged scenario, camera, tick 1, and overlap
capture changed from screenshot SHA-256
`133aadbd894cdc605411eae73b1276b6ad346af6c119059e2017b9c0bd85af92`
to `0c84325761c717044d881a0f477e2a944b5b6bab1509bb00b0bcf85dcac0ca2a`.
Before correction, moving King/Woad objects were reported as reconstruction
direction 2 with unflipped frames 20/24. The packaged corrected path stores
logical east 7 and selects mirrored frames 30/36; stationary logical zero
selects mirrored frames 13/19 instead of being recomputed by the renderer.

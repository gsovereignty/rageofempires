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

These values do not by themselves prove renderer/simulation synchronization.
The supplied HD decompilation proves asset construction and drawing paths, but
review found no authoritative, typed call path joining the DAT frame-duration,
attack-frame-delay, replay-delay, and simulation tick domains for all
represented states. Decompiled numeric fields without a proved structure
identity are not promoted by resemblance.

Therefore:

- DAT frame duration and attack delay are exact data;
- live frame layout and mirroring compatibility are exact where classified;
- the conversion to reconstruction ticks is `ambiguous`;
- attack/projectile/action synchronization is `ambiguous`;
- no executable timing claim is generalized from HD to classic AoC.

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
continues while projectiles render separately. Construction roots expose
unresolved layered/shadow behavior, so incomplete building bodies remain
procedural.

The API keeps unresolved clock and logical-direction conversion external:

```text
AnimationEvidence {
  graphic_id, slp_id,
  frames_per_angle, dat_angle_count, stored_angle_count,
  frame_duration_seconds, replay_delay_seconds,
  sequence_type, mirroring_mode, deltas,
  layout_classification
}
```

Runtime state selection and attack release need a separately proved scheduler
contract before `frame_duration_seconds` or `attack_frame_delay` may control
simulation/render timing. The reconstruction advances at five simulation
ticks per second, while exact DAT durations range roughly 0.05–1.0 seconds per
frame. Current one-frame-per-tick modulo therefore remains explicitly
procedural. No fixed modulo was promoted merely because one duration happens
to equal 0.2 seconds.

Likewise, physical five-angle mirrored storage is exact, but the original
logical-direction and `display_angle` selector path remains unproved. Current
SDL mirroring stays procedural and is not labeled original-exact.

## Reproduction and tests

```sh
python3 tools/dat_metadata/generate_animation_evidence.py \
  /tmp/aoe-core-rules-metadata.json \
  /path/to/Data/graphics.drs
```

Focused tests pin all 121 records and 847 role outcomes, mirrored Villager
storage, exact high-use state bindings, Archer idle rejection, hunting-only
gather dispatch, explicit missing/mismatched assets, and the
exact/ambiguous/absent vocabulary. Deterministic SDL capture smoke verifies
the state/render path while retaining procedural fallback for unproved roles.

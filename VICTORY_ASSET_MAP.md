# Wonder and victory DAT asset map

## Evidence boundary

`generated/victory_dat_metadata.json` preserves the Wonder record, hidden
gate, Hun Atheism technology, raw victory-resource commands, civilization
boundaries, and asset links from validated VER 5.7 DAT metadata.

Pinned `genie-rs` exposes no victory-mode/countdown table, civilization
resource defaults, or localized message table. Extracted DAT therefore does
not establish Wonder/relic countdown length or units, relic ownership
threshold, reset rules, score/time-limit/conquest conditions, team aggregation,
or Atheism's runtime interpretation. Those require original-runtime validation.

## Wonder

Wonder 276 has 4,800 HP, LOS/search radius 8, terrain restriction 4, displayed
3 armor, and armor classes 21:0, 11:0, 4:3, and 3:10. Villagers build it in
slot 12 for 1,000 wood, 1,000 gold, 1,000 stone, and 3,500 source seconds. Its
default task is action 120 with no target, work value, or search behavior.

Hidden technology 144 `Wonder Plans` requires Imperial Age 103 and has no
research location or time. Effect 145 enables object 276. Its raw icon is 77
and button is 0. All 18 civilization trees retain the Wonder and gate.

Standing graphic 3068 maps to SLP 3534. Construction graphic 123 maps to SLP
241 with one frame and three angles. Death graphic 42 maps to SLP 75 and
triggers sound 323/WAV resources 5316–5318 and 5459. Selection sound 383 maps
to WAV resource 5456. Archive presence and decoding require DRS validation.

## Atheism and victory resources

Hun Atheism 21 requires Imperial Age 103 and belongs only to civilization 17.
It researches at Castle 82 in slot 8 for 500 food, 500 gold, and 60 source
seconds; icon 107.

Effect 464 contains exactly two type-1 resource commands:

- Resource 196 uses mode `b=1` and raw value 1000.
- Resource 197 uses mode `b=0` and raw value 1.

No other captured effect modifies resources 196 or 197. Pinned API exposes
neither names nor default values. Calling either value a particular year
extension, countdown multiplier, relic threshold, or income modifier would
exceed extracted evidence. Exact countdown, enemy-only, relic-income, team,
and diplomacy semantics require original-runtime validation.

## Victory-mode absence

No score target, time limit, conquest condition, Wonder/relic countdown,
ownership threshold, victory message, or team-victory record is exposed in
pinned extraction. Absence from generated JSON means “not available through
this parser,” not “absent from commercial runtime.”

Represented countdown, relic, score, time, conquest, alliance, simultaneous
completion, destruction/reset, ownership-transfer, and save/replay behavior
must remain bounded reconstruction behavior until validated.

## Bounded reconstruction contract

Victory countdown state is typed per player as Wonder or Relic, with one
active kind at a time. When neither kind is already active and both first
qualify together, Wonder takes precedence. While the active kind remains
qualified, only that timer advances; the alternate kind neither replaces nor
advances beside it. If the active qualification is lost while the alternate
kind qualifies, the old state clears and the alternate starts at its full
configured duration.

Wonder qualification belongs to the player, not one tracked building ID.
Destroying one completed Wonder therefore does not reset an active Wonder
timer when another completed, surviving Wonder remains. Losing the last
qualifying Wonder clears that Wonder state or starts a fully reset Relic timer
when the Relic condition is already met.

Both players resolve on the same simulation tick. Simultaneous opposing wins
draw; an allied-side win yields allied victory. With the represented two-player
alliance, Wonder qualification is shared, Relics are summed across both
players, and both typed countdowns advance identically. Allied score is the
sum of both player scores. Reaching an allied time limit yields allied victory
instead of comparing teammates.

Resignation is different: the other participant wins even when diplomacy is
allied. Score is the player's stockpiled food, wood, gold, and stone plus
remaining HP of surviving owned units and buildings. Reaching a configured
score limit is a win condition. For enemies, reaching the time limit on the
same tick overrides Wonder, Relic, and score-limit results and resolves by
that score, with equal scores drawing.

After any terminal result, updates stop and live state-changing commands,
including diplomacy and repeated resignation, are rejected. Current Save v109
stores match rules, outcome, and each player's typed countdown kind and
remaining ticks; v94 is the compatibility gate for typed countdown kinds.
Scenario records store match rules and entities. Replay records can carry
resignation, but do not replace the requirement for identical initial match
rules and state. HUD text derives `WONDER` or `RELIC` from countdown kind
rather than treating every active timer as Wonder.

These rules define deterministic reconstruction behavior only. Pinned DAT
does not validate countdown lengths, precedence, resets, score formula,
time-limit ordering, simultaneous/team outcomes, resignation, terminal freeze,
or persistence against original runtime.

Atheism resources 196/197 retain unknown runtime meaning. Represented Atheism
therefore does not modify Wonder/Relic timers, thresholds, score, or Relic
income. A parity test runs identical Wonder countdowns with and without
Atheism and requires equal remaining time. This is deliberate non-guessing,
not evidence that commercial Atheism has no victory effect.

The simulation has no separate defeated-player observer/controller state.
Terminal `MatchOutcome` freezes simulation updates and rejects live mutations;
camera observation, post-defeat control, and spectator permissions belong to
frontend or multiplayer contracts and are not modeled here.

Focused adversarial tests cover split 3+2 allied Relics, combined team score
where neither player individually reaches the limit, allied time expiry,
enemy-to-ally diplomacy during an active Wonder countdown, saved allied Relic
countdown completion, and Atheism timer parity. Existing tests cover conquest,
resignation, opposing simultaneous Wonder draw, Wonder/Relic precedence and
reset, multiple Wonders, time/score resolution, scenario rules, Save v108
countdowns, replayed resignation, and terminal command freeze.

## Regeneration

```sh
python3 tools/dat_metadata/generate_victory_metadata.py \
  /tmp/aoe-metadata.json
```

```sh
AOE_TEST_METADATA=/tmp/aoe-metadata.json \
  python3 -m unittest tools/dat_metadata/test_generate_victory_metadata.py
```

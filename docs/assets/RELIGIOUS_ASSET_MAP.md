# Missionary and monastery technology evidence

## Scope and evidence boundary

This map records Missionary DAT unit 775, hidden Spanish availability
technology 84, and monastery technologies 231, 252, 316, 319, 233, 230, 45,
438, and 439 from validated VER 5.7 `empires2_x1_p1.dat`.
[`generated/religious_dat_metadata.json`](../../generated/religious_dat_metadata.json)
is regenerated from legal live DAT; no original game data is bundled.

DAT proves records, costs, prerequisites, producer/button/icon placement, raw
effect commands, civilization disables, task filters, graphics-to-SLP links,
and conceptual sound-to-WAV resource links. It does not prove conversion
randomness or resistance, Theocracy group-charge behavior, or Heresy
ownership/death edge cases. Those remain original-runtime validation work.

## Missionary record and availability

Missionary 775 has 30 HP, speed 1.1, LOS/search radius 9, displayed range 7,
reload 1, displayed armor 0/0, and no conventional weapon. It costs 100 gold
and one population, trains in 51 source seconds at Monastery 104 button 14,
and uses icon 107. Its action work rate is 1.25.

Hidden tech 84 requires Castle Age 102 and civilization marker 266, belongs
to Spanish civilization ID 14, has no player-facing location/button, and
selects raw effect 496. Effect 496 enables unit 775. Missionary definitions
exist in every civilization array, so definition presence alone is not
availability evidence; tech ownership makes it Spanish-only.

Inherited tasks expose these numeric restrictions:

- Action 104 enables targeting, owner type 2, combat level 1, work values
  4/10, and conversion graphic 6616.
- Action 105 uses work graphic 6633.
- Action 3 accepts object classes 20 and 3 at range 1 with owner type 4.
- Action 109 auto-searches every three seconds.

Numeric action, class, and owner values remain raw DAT fields. Human-readable
semantics are not guessed beyond links independently proved by existing
gameplay and asset behavior.

## Monastery technologies

Resource IDs are 0 food and 3 gold. All technologies research at Monastery
104. `Effect` below is raw `time2` link to global effect table.

| Technology | DAT/effect | Age prerequisite | Cost | Time | Button/icon | Raw effect evidence |
|---|---|---|---|---:|---|---|
| Sanctity | 231/221 | Castle 102 | 120 gold | 60 s | 6/83 | +15 HP to classes 18 and 43 |
| Fervor | 252/241 | Castle 102 | 140 gold | 50 s | 4/73 | speed multipliers including 1.15 and 1.10 class commands |
| Redemption | 316/316 | Castle 102 | 475 gold | 50 s | 2/92 | resource 28 set to 1 |
| Atonement | 319/319 | Castle 102 | 325 gold | 40 s | 3/93 | resource 27 set to 1 |
| Illumination | 233/219 | Imperial 103 | 120 gold | 65 s | 8/84 | resource 35 adds 3 |
| Block Printing | 230/220 | Imperial 103 | 200 gold | 55 s | 9/82 | +3 range, LOS, and search radius to classes 18 and 43 |
| Faith | 45/45 | Imperial 103 | 750 food, 1,000 gold | 60 s | 7/11 | resources 77/178/179 add 3/2/4 |
| Theocracy | 438/494 | Imperial 103 | 200 gold | 75 s | 12/109 | resource 193 set to 1 |
| Heresy | 439/188 | Castle 102 | 1,000 gold | 60 s | 11/108 | resource 192 set to 1 |

Pinned parser calls this field `time2`; evidence strongly links each value to
corresponding effect record because referenced commands match technology
domain. Generated JSON names it `effect_id_raw`, preserving field status
instead of asserting undocumented binary semantics.

## Represented deterministic models

Block Printing implements every directly decoded effect-220 attribute for
represented Monk and Missionary classes: +3 conversion range, LOS, and
command/search radius.

Other resource-backed effects require behavior beyond this DAT:

- Fervor applies 1.15 to Monk and Missionary speed. Missionary's 1.10 base is
  represented through integer-percent rounding; exact 1.265 movement cadence
  and meaning of duplicate/wildcard effect selectors remain unvalidated.
- Redemption enables current siege and completed-building conversion targets;
  Atonement enables Monk and Missionary targets. Resources 28/27 do not expose
  exact eligible and excluded target classes, so Town Center, Castle,
  Monastery, Wonder, wall, and gate behavior requires original-runtime checks.
- Illumination deterministically halves the bounded conversion recharge from
  20 to 10 ticks. Resource 35 adding 3 does not independently prove this
  recharge constant or cadence.
- Unit conversion checks once per active update against one shared persisted
  MSVCRT stream. Live Monk task 1 supplies minimum/maximum work values 4/10;
  its DAT accuracy supplies chance 25. Recovered resistant classes add 3,
  commercial IDs 448/546/441/751/752 add 8, Faith adds resources 77/178/179
  of 3/2/4, and Teuton team effect 404 adds 2/1/2. Every check consumes one
  random value, including forced-minimum failures; forced maximum is
  inclusive. Save/load preserves next global consumer exactly.
- Theocracy makes only the successful participant recharge after grouped
  conversion. Heresy kills a converted unit rather than transferring it.
  Resources 193/192 only prove toggles; participant selection, mixed progress,
  group charges, target death ownership, building behavior, and simultaneous
  edge cases remain unvalidated.

Target eligibility, grouped participants, Heresy, and recharge remain bounded
reconstruction contracts where noted; random stream, resistance inputs, and
unit-conversion check schedule follow recovered commercial semantics.

The isolated commercial conversion-roll arithmetic is now recorded separately
in `../evidence/CONVERSION_RUNTIME_EVIDENCE.md`. That static evidence proves the
`rand()*100/0x7fff`, resistance conversion, threshold, comparison, and task
timing seam. It does not prove every target-policy branch.

## Conversion command contract

Both Monk and Missionary may convert a visible enemy unit inside displayed
conversion range. Block Printing adds the decoded three tiles. Dead,
garrisoned, neutral-animal, Relic, friendly, hidden, and out-of-range targets
are rejected. Religious targets require Atonement; Siege Workshop units and
Trebuchets require Redemption. Missionaries cannot collect or deposit Relics.

Redemption also enables the represented completed, living enemy-building
target path. A converter must be ungarrisoned, charged, in range, and currently
able to see the building. Exact commercial eligible building classes remain
unproved, so this broad completed-building policy is reconstruction behavior.

Conversion cancels when any validated target condition stops holding. Base
recharge is 20 simulation ticks; Illumination reduces it to 10. Without
Theocracy, all same-owner participants targeting the successfully converted
entity spend their charge. With Theocracy, only the successful participant
does. Heresy leaves the target with its old owner and kills it through normal
unit-death cleanup rather than transferring ownership.

Tests cover command rejection, Atonement and Redemption gates, Monk/Missionary
restrictions, commercial resistance variation, Faith delay, both Theocracy
group outcomes, Heresy death, recharge, replayed command identity, and a
mid-conversion save/load branch that reaches the same owner and cooldown.

## Civilization availability

Faith and Fervor are available to all 18 civilizations. Other live type-102
disable boundaries:

- Sanctity: all except Persians, Vikings, and Mongols.
- Redemption: Germans, Japanese, Chinese, Byzantine, Saracens, Turks,
  Spanish, and Aztecs.
- Atonement: Germans, Japanese, Chinese, Byzantine, Saracens, Turks, Vikings,
  Mongols, Spanish, Aztecs, Mayan, and Huns.
- Illumination: British, French, Goths, Germans, Japanese, Chinese, Byzantine,
  Saracens, Spanish, Aztecs, and Huns.
- Block Printing: British, French, Germans, Japanese, Byzantine, Persians,
  Saracens, Vikings, Spanish, Aztecs, Mayan, and Koreans.
- Theocracy: British, French, Goths, Germans, Japanese, Chinese, Byzantine,
  Persians, Saracens, Turks, Spanish, Aztecs, Mayan, and Koreans.
- Heresy: French, Germans, Byzantine, Saracens, Turks, Vikings, Mongols,
  Celts, Spanish, Aztecs, Mayan, and Huns.

## Graphics and sounds

All Missionary roots use palette 65535 and eight angles.

| Action | Graphic/SLP | Frames | Sound |
|---|---|---:|---|
| Conversion/attack | 6616/4865 | 13 | sound 417 at delay 2; WAV resources 5494/5495 |
| Death | 6617/4866 | 12 | sound 294; WAV resources 5309–5314 |
| Idle | 6618/4867 | 12 | none |
| Walk | 6620/4870 | 12 | none |
| Task 105 | 6633/4869 | 14 | sound 418; WAV resource 5497 |

Training sound 469 maps to WAV resource 6178. Selection sound 423 uses generic
5299/6535 and Spanish 6651–6654. Command/move sound 424 uses generic
5571–5573 and Spanish 6647–6650. DAT sound filenames are empty, so these are
DRS WAV resource IDs, not invented filenames.

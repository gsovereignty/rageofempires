# Audio archive fidelity

## Lookup contract

Optional playback resolves WAV resources in classic installation order:

1. `Data/sounds.drs`
2. `Data/sounds_x1.drs`
3. `Data/sounds_x2.drs`

Later archives replace earlier archives only for an identical numeric WAV
resource ID. Missing expansion IDs fall back to base. Missing expansion
archives are accepted, preserving base-only roots. Original bytes stay in
the user's installation and are read only when played.

## Live archive evidence

| Archive | SHA-256 | WAV IDs |
|---|---|---:|
| `sounds.drs` | `292eeff24657b70c9fa277a55daa0e6a47bc245141c0c533bab54eecda244fa6` | 1021 |
| `sounds_x1.drs` | `48efbb286fb73d4aa88ed7c05ab38cb0d045323ae04aed13f21792034b30f6c5` | 365 |

`sounds_x1.drs` contains 307 expansion-only WAV IDs and 58 IDs also present
in base. All 58 repeated payloads are byte-identical in this legal root.
Exact repeated IDs:

```text
5110, 5945-5949, 5951-5957, 5959-5995,
6391-6395, 6397-6398, 6432
```

This proves precedence is behaviorally neutral for those live duplicates.
Synthetic tests use differing bytes to lock expansion-first semantics.
Expansion-only evidence includes `5357` and IDs in `6450-6757` with gaps;
range notation does not claim every ID inside that range exists.

## Exhaustive DAT/archive catalog

`generated/audio_catalog.json` is a reproducible join of all 506 VER 5.7 DAT
sound records to the two live sound archives above. Regenerate it with:

```sh
python3 tools/dat_metadata/generate_audio_catalog.py \
  /tmp/aoe-core-rules-metadata.json /path/to/app
```

The catalog records exact archive hashes and inventories, every DAT item,
resolved archive under runtime precedence, WAV byte size and RIFF format,
duplicate identity, missing references, and archive WAVs not referenced by
the DAT. The audited join contains 493 nonempty sounds and 1,730 items:

| Exact inventory measure | Count |
|---|---:|
| unique nonnegative DAT WAV references | 1,650 |
| references present in supplied archives | 1,314 |
| references absent from supplied archives | 336 |
| unique WAV IDs after archive precedence | 1,328 |
| archive WAV IDs not referenced by DAT | 14 |
| IDs repeated across archives | 58 |
| repeated IDs with byte-identical payloads | 58 |

All 1,371 available DAT items resolve to a valid RIFF/WAVE payload. Item count
is greater than unique reference count because multiple sound records can
refer to one WAV. A negative DAT resource ID is preserved as a sentinel and
is not reported as a missing WAV. The 336 absent nonnegative IDs are exact
gaps relative to these supplied archives only; the catalog does not guess
their purpose, external media source, or replacement.

The 14 unreferenced archive IDs are:

```text
5492, 5493, 5555-5562, 5950, 5958, 6396, 6410
```

Source-only tests construct synthetic DRS files to prove later-archive
precedence and malformed bounds rejection, then pin the checked-in live
counts, hashes, and RIFF validity.

## Reactive playback evidence

Current reactive event layer sends conceptual DAT sound IDs to `AudioSystem`.

| Reaction | Sound ID | Generic WAV evidence |
|---|---:|---|
| unit training | 337 | 5423, base |
| siege death | 293 | 5367, base |
| common unit death | 294 | 5309-5314, base |
| projectile/explosion impact | 323 | 5316-5318, 5459, base |
| Bombard Cannon/Tower projectile | 411 | 5486-5488, base |
| Missionary conversion start | 417 | 5494-5495, base |
| Missionary healing start | 418 | 5497, base |
| Missionary selection | 423 | generic 5299, base |
| Missionary movement | 424 | generic 5571-5573, base |

Sound IDs 420-424 also contain civilization-specific records whose WAVs can
live in `sounds_x1.drs`. Sound 420 references x1-only `6511-6513`,
`6573-6575`, `6595-6597`, `6691-6693`, and `6750-6752`; sound 421 references
`6508-6510`, `6570-6572`, `6592-6594`, `6688-6690`, and `6747-6749`;
sound 422 references `6504-6507`, `6566-6568`, `6590-6591`, `6684-6687`,
and `6745-6746`.

Playback passes blue listener civilization from current simulation state.
An exact DAT civilization record takes precedence over generic `-1`; when no
exact record exists, generic records are used. Highest numeric probability
wins within chosen group and original DAT order breaks ties. Thus live sound
420 resolves Spanish to WAV `6692`, Huns to `6513`, and a civilization with
no override to generic WAV `5299`. If an exact civilization record points to
an unavailable expansion resource in a base-only root, playback retries the
generic record.

This is bounded deterministic selection, not reconstruction of original
weighted randomness. `probability` is used as stable priority because audio
must not introduce nondeterminism into simulation-adjacent playback. Icon-set
selection remains unsupported because caller has no evidenced icon-set
context; represented 420-424 records use `-1` in that field.

## Deterministic coverage

`aoe_legacy_assets_tests` proves:

- x1-only WAV lookup;
- differing x1 payload overrides repeated base ID;
- absent x1 ID falls back to base;
- x2 overrides x1/base;
- base-only root remains readable;
- malformed expansion archive does not disable a valid base archive;
- resource enumeration stays sorted;
- missing and invalid RIFF resources fail.

`aoe_legacy_dat_tests` additionally proves two exact civilization choices,
stable DAT-order tie-breaking, absent-override generic fallback, and live
sound 420 resource IDs when `AOE_TEST_DAT` is supplied.

## Reactive event coverage

`FrontendAudioEvents` observes already-published frontend state. It does not
change simulation timing or outcomes.

| Event | Deduplication / visibility | Bounded mapping |
|---|---|---|
| unit selection | fires only when selected ID set changes | exact live `selected_sound` mapping for 94/96 represented units |
| building selection | fires only when selected building ID changes | exact live `selected_sound` mapping for all 27 represented buildings |
| movement start | false-to-true moving transition | exact live `move_sound` mapping for 93/96 represented units |
| training completion | newly observed blue unit ID | exact live `train_sound` mapping for 91/96 represented units |
| attack | cooldown increase and blue visibility | exact documented special cases below; arrows 314; remaining represented units 329 |
| impact | newly created effect and blue visibility | 323 |
| death | newly created death effect and blue visibility | Fishing Ship 505; other represented ships 379; siege 293; Petard 323; ordinary single-trigger units 294 |
| building completion | newly observed blue building ID | represented Outpost/Bombard Tower 23; Wonder 383 |
| building destruction | newly created rubble effect and blue visibility | 323 for represented buildings whose live dying graphic carries 323; Farm/Fish Trap have no dying sound |

Exact special attack choices now include Hand Cannoneer, Janissary, and
Conquistador sound 385; Bombard Cannon sound 411; Mameluke 486; Tarkan 497;
War Elephant base fight sound 26; and Longbowman fight sound 312. These use
one sound per detected attack transition. Units whose graphic graph specifies
multiple differently delayed triggers remain only partially represented:
playing all simultaneously would invent timing. These omissions are explicit
rather than guessed.

Cataphract, War Elephant, and Tarkan death graphs carry multiple delayed or
otherwise non-generic sounds. Their death transition is intentionally silent
until frontend timing can schedule those graph delays; it no longer emits
unsupported generic sound 294. Building construction graphics expose no
direct completion trigger in pinned metadata. Existing bounded completion
sounds remain limited to previously documented Outpost/Bombard Tower and
Wonder links.

With `AOE_AUDIO_TRACE=1`, every accepted event logs conceptual sound ID and
resolved WAV ID. State transitions above make trace ordering deterministic
for a fixed simulation/replay; WAV selection uses deterministic policy
documented earlier.

### Accepted command acknowledgement

Right-click player commands emit each unit's live `command_sound` only after
`execute` succeeds and
the corresponding command is recorded. Replay command-count growth is the
frontend proof of acceptance. Invalid clicks, rejected commands, AI orders,
replay playback, screenshot setup, and internal movement do not cross this
boundary. A multi-unit click emits at most one acknowledgement.

Coverage is 93/96 represented units. Families retain distinct live identities:
Villager 301; common land 422; ordinary mounted 326; Scout Cavalry 474;
Cavalry Archer 415; elite Mangudai 467; Camel 326; War Elephant 483;
Monk/Missionary 424; ships 340; mobile siege 476; Packed Trebuchet 484;
Trebuchet 291; Trade Cart 306; Relic 30; Petard 421. Sheep, Deer, and Boar
have no player-command mapping.

## Exhaustive live field report

VER 5.7 DAT SHA-256
`e49d05b326ecf4a14e0cddd5171718c6849abe2548939bb9a93a8f3039753d9d`
was joined to every represented commercial unit ID. Public mapping helpers in
`frontend_audio.hpp` expose conceptual IDs independently of SDL:

- `selected_sound(UnitKind)`: 92/94;
- `accepted_command_sound(UnitKind)`: 91/94;
- `movement_sound(UnitKind)`: 91/94;
- `trained_sound(UnitKind)`: 89/94;
- `selected_sound(BuildingKind)`: 27/27.

Trade Cart selection and training vary by civilization record (305 versus
303, and 305 versus 317). Those two mappings remain `-1` because current
helper lacks source-civilization context; stable command/movement sound 306 is
represented. Boar has no four mapped event fields. Deer and Sheep retain only
fields actually present. Every other unsupported return is covered by count
tests, preventing silent breadth regression.

Research completion, age completion, completed conversion, relic deposit,
market/tribute transaction, wonder countdown, victory, defeat, and interface
alert conceptual sound IDs are not fields in the pinned technology/unit/
building records. They remain unwired: assigning familiar-sounding numeric
IDs without a proved source would be guessed mapping. Missionary conversion
and healing *start* sounds 417/418 remain backed by graphic trigger evidence.
Match outcome/UI notification work needs independently pinned executable or
interface-resource evidence.

## Music and ambience streams

Live legal-root inventory contains exactly:

```text
Sound/stream/town.mp3       842605 bytes
  SHA-256 905d55aa237cc7416e2b099ba0d95c5f5255821e79968c1d92691ae5164d8b2e
Sound/terrain/Wave1.wav     183928 bytes
  SHA-256 a07980e10e0792eece3a2fecaf14a9ef1a8b9c4ca2f84fa4c88f01f6de399e4f
```

No additional music or ambient stream was present in that root. This proves
installed payload contents only, not original playlist policy.

Playback discovers direct `.mp3` and `.wav` files under `Sound/stream`,
case-insensitively sorts by filename, plays each once, advances after queued
audio drains, then wraps to first track. Corrupt or build-unsupported files
are skipped in the same stable order. `town.mp3` remains discovered on the
validated single-track root, producing previous loop behavior through
one-item playlist wrap. MP3 requires configured `mpg123`; WAV uses SDL.

`Sound/terrain/Wave1.wav` remains independent looping shoreline ambience.
No ambient playlist is claimed because no second ambient stream was found.
Synthetic discovery tests prove direct-file filtering, `.mp3`/`.wav`
support, parent-`app` root handling, case-insensitive ordering, and nested or
unsupported-file exclusion. Filename ties after case folding use exact
filename as deterministic secondary order. `AOE_AUDIO_TRACE=1` logs each selected music
filename, making playlist transitions observable without bundling audio.
## Reconstruction-native mixing policy

Runtime now composes persisted master music/effects values with combat,
interface, and ambient category values. Options apply gains to live SDL audio
streams. Pause silences music and ambience while retaining interface feedback;
window focus loss and explicit mute silence every bus. These deterministic
rules are reconstruction-native: original volume scale, focus behavior, and
bus topology remain unproved by supplied executable evidence.

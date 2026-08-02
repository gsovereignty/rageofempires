# Original audio vs reconstruction

## Scope and evidence

Comparison uses supplied HD Edition installation at `../original-assets-hd/app`,
reconstruction runtime in `src/audio_system.cpp`, `src/frontend_audio.cpp`,
and `src/sdl_app.cpp`, plus the parsed DAT/DRS inventory in
`generated/audio_catalog.json`.

“Missing” below means original payload has no reachable reconstruction
playback path. “Unfaithful” means reconstruction plays original bytes under a
different policy, context, timing, or selection rule. Asset names prove asset
roles strongly enough to find unsupported and misrouted content, but do not by
themselves prove every detail of the original executable's sequencing.

## Verdict

The comparison below records the pre-repair runtime that motivated this work.
Current reconstruction behavior differs materially:

- `Sound/music/music1.mp3` and `xmusic1.mp3` are gameplay music; stream files
  have explicit opening, menu, civilization, countdown, outcome, and credits
  routes instead of entering one playlist.
- numeric multiplayer chat invokes matching `Taunt/en` audio;
- camera-center terrain selects across all 20 terrain WAVs;
- DAT sound items use probability-weighted selection from an audio-only RNG;
- the fixed 16-voice rejection cutoff is gone;
- world effects use camera-distance gain, stereo pan, visibility, and source
  civilization;
- DAT graphic frame/sound records are parsed and scheduled for attack/death
  animations;
- scenario trigger `audio_file` survives conversion, execution, and Save v112;
- native campaign entries can name briefing and debrief audio files.

Remaining fidelity is not proved for hard-coded executable event mappings
whose sound IDs are absent from supplied DAT fields: research/age completion,
conversion completion, relic transitions, market/tribute feedback, and
general alerts. Those are not guessed below. Exact original voice replacement
priority and mixer limits also remain unevidenced.

The supplied `AoK HD.exe` cannot close that evidence gap. SHA-256
`02ccff32765e19f4f75be5454c74e85bd78cc2fccea44ba21176371f715aa1fb`
is a 55 KiB PE32 dummy launcher. Its only user-facing UTF-16 string says
`This is not the .exe you are looking for.`; imports are limited to CRT,
`KERNEL32.dll`, and `MessageBoxW`. It contains no game or audio engine.
Finishing those exact event mappings requires the matching gameplay
executable or an authoritative runtime trace from it.

Supplied loose audio inventory:

| Original family | Files | Reconstruction behavior |
|---|---:|---|
| `Sound/stream` | 29 MP3 | All files become one alphabetical looping playlist |
| `Sound/music` | 2 MP3 | Never discovered or played |
| `Sound/terrain` | 20 WAV | Only `Wave1.wav` loops globally |
| `Sound/campaign/en` | 115 MP3 | Never played |
| `Sound/scenario/en` | 709 MP3 | Never played |
| `Taunt/en` | 42 MP3 | Never played |

DAT/DRS effects inventory has 493 nonempty conceptual sounds and 1,730 DAT
items. Reconstruction does not expose all those sound concepts as events; it
only requests a bounded hard-coded subset.

## Music mismatches

### Critical: stream stingers and themes are treated as background playlist

`discover_legacy_music_tracks()` enumerates every direct MP3/WAV under
`Sound/stream`, sorts names case-insensitively, and `start_next_music()` plays
the resulting list sequentially forever.

This routes all these distinct assets as generic background music:

- 18 civilization tracks: `Aztecs.mp3` through `Viking.mp3`;
- opening/menu tracks: `open.mp3`, `xopen.mp3`;
- town tracks: `town.mp3`, `xtown.mp3`;
- outcome/countdown tracks: `Countdwn.mp3`, `lost.mp3`, `won1.mp3`,
  `won2.mp3`;
- credits tracks: `credits.mp3`, `xcredits.mp3`;
- `Random.mp3`.

Consequences:

- victory, defeat, countdown, opening, and credits audio plays during ordinary
  gameplay;
- civilization themes are not selected by civilization or screen/context;
- base and expansion variants are not selected by game/content context;
- ordering is filename order, not original event/state policy.

### Critical: actual music files are silent

`Sound/music/music1.mp3` and `Sound/music/xmusic1.mp3` are outside
`Sound/stream`; discovery never sees them. No other reconstruction source
references `Sound/music`. Both supplied music assets are therefore absent.

### Missing state-specific stream playback

No dedicated path exists for:

- opening/menu;
- civilization-specific theme;
- countdown;
- victory;
- defeat;
- credits.

These files may happen to play inside the wrong global playlist, but that is
not faithful playback.

### Playback policy differs

Reconstruction decodes each discovered file fully into memory, plays it once,
then advances in sorted order. WAV entries loaded as music are marked looping,
while MP3 entries are non-looping. This is reconstruction policy, not an
evidenced reproduction of original playlist/state logic.

## Ambient sound mismatches

Only `Sound/terrain/Wave1.wav` is loaded. It loops continuously whenever audio
is active, independent of map terrain, camera, location, weather, or visible
water.

Nineteen supplied ambience files never play:

- `Cricket.wav`;
- `Wave2.wav` through `Wave5.wav`;
- `Wind1.wav` through `Wind3.wav`;
- `jungle1.wav` through `jungle4.wav`;
- `tf1.wav`, `tf2.wav`, `tf3.wav`, `tf4.wav`, `tf6.wav`, `tf7.wav`,
  `tf8.wav`.

Even `Wave1.wav` is unfaithful because it is global and unconditional rather
than spatial/contextual.

## Completely missing loose-audio families

### Campaign narration

All 115 `Sound/campaign/en/*.mp3` files are silent. This includes campaign
intro and scenario intro/end narration. Campaign loading/progression has no
audio call tied to these files.

### Scenario speech

All 709 `Sound/scenario/en/*.mp3` files are silent. Scenario triggers do not
resolve or play scenario audio filenames.

### Multiplayer taunts

All 42 `Taunt/en/*.mp3` files are silent. Chat/taunt number handling has no
audio playback path.

## DAT/DRS effect mismatches

### Sample selection is deterministic, not original weighted variation

For a conceptual DAT sound, `select_legacy_sound_item()` chooses the highest
numeric probability in the matching civilization group, with DAT order as a
tie-break. It does not perform weighted/random selection.

Result: repeated actions reuse one WAV instead of original variation. Original
WAV payload can be correct while audible result remains repetitive and
unfaithful.

### Hard concurrency cutoff

`AudioSystem::play_effect()` refuses new effects once 16 are active. There is
no priority, replacement, distance, or age policy; 17th and later sound is
silently lost. This can remove combat and interface sounds in busy scenes.

### No spatial audio

All effects use one gain. World position, camera distance, stereo pan, source
ownership, and on-screen distance do not affect playback. Visibility merely
gates some event emission.

### Missing or partial event families

No evidenced event wiring exists for:

- research completion;
- age advancement completion;
- conversion completion;
- relic pickup/deposit;
- market sale/purchase and tribute;
- wonder countdown;
- victory and defeat effects;
- general UI alerts and notifications;
- campaign/scenario trigger audio;
- taunts.

### Partial attack/death animation triggers

Attack audio is inferred from attack-cooldown increases and emits at most one
conceptual sound per detected transition. Original graphic graphs can contain
multiple sounds at different frame delays; those timings are not scheduled.

Cataphract, War Elephant, and Tarkan deaths are intentionally silent because
their multiple delayed triggers are unsupported. This avoids a guessed generic
sound, but remains missing audio.

### Event ownership and visibility differ

- movement sounds fire on every unit's false-to-true movement transition,
  without local ownership or visibility gating;
- training sounds fire only for newly observed units belonging to current
  view player;
- attacks, impacts, deaths, and rubble use reconstruction visibility checks;
- accepted command acknowledgements emit at most one unit voice for a
  multi-unit order.

These are reconstruction heuristics, not a reproduced original mixer/event
contract.

### Building completion/destruction coverage is narrow

Completion sound exists only for newly seen Outpost, Bombard Tower, and
Wonder. Other buildings have no completion event.

Building destruction maps almost every represented dying building to generic
sound 323; Farm and Fish Trap are silent. This coarse mapping does not
reproduce per-graphic timing or full building-specific behavior.

### Civilization context is incomplete

Listener civilization selects civilization-specific DAT records, but this is
the current view player's civilization, not necessarily sound source's
civilization. Trade Cart selection/training mappings are disabled because
helper lacks source-civilization context. Other exact-record fallback and
selection behavior is reconstruction-specific.

## What is faithful

- Playback reads original DRS WAV bytes rather than synthesized replacements.
- Archive precedence supports base, x1, and x2 DRS layers.
- Many unit command, selection, movement, training, attack, death, impact, and
  building-selection conceptual IDs come from pinned DAT/graphic evidence.
- Civilization-specific records can resolve for current listener
  civilization.
- MP3/WAV decoding preserves source payload when decoder/device succeeds.

These strengths establish asset-byte fidelity for played samples, not
behavioral fidelity for when, where, how often, or which variant plays.

## Priority repair order

1. Replace global `Sound/stream` playlist with explicit state routing; add
   `Sound/music/music1.mp3` and `xmusic1.mp3` as music sources.
2. Add campaign/scenario filename playback from scenario/campaign data and
   trigger events.
3. Add taunt-number playback.
4. Add terrain/camera-aware ambience selection for all 20 terrain WAV files.
5. Implement DAT probability-based variant selection without affecting
   simulation determinism.
6. Schedule graphic-trigger sounds at their recorded frame delays.
7. Wire age/research/conversion/relic/economy/outcome/UI events.
8. Add spatial mixing and an evidenced voice-limit/priority policy.

## Stale existing audit

`../fidelity/AUDIO_ARCHIVE_FIDELITY.md` says supplied live root contains exactly
`Sound/stream/town.mp3` and `Sound/terrain/Wave1.wav`. Current supplied
`../original-assets-hd/app` instead has 29 stream MP3s, 2 music MP3s, 20 terrain
WAVs, 115 campaign MP3s, 709 scenario MP3s, and 42 taunts. Its inventory-based
music/ambience conclusion is stale and must not be treated as current
full-root fidelity evidence.

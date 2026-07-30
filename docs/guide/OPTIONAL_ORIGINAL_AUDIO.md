## Packaged original audio

This working copy packages user-supplied audio from a legally obtained,
extracted HD installation under `game_data/`:

- supported `.mp3` and `.wav` files directly under `Sound/stream`, in stable
  case-insensitive filename order; a single `town.mp3` retains looping
  behavior;
- `Sound/terrain/Wave1.wav`: original shoreline ambience;
- WAV resources selected through `empires2_x1_p1.dat` from `Data/sounds.drs`,
  `Data/sounds_x1.drs`, and `Data/sounds_x2.drs`. Later archives override an
  identical resource ID; missing expansion archives fall back to base.

Reactive playback covers bounded selection, movement, accepted player command,
training, attack, impact, death, building-completion, and building-destruction
events. Civilization-specific DAT records take precedence over generic records
with deterministic fallback. See
[`docs/fidelity/AUDIO_ARCHIVE_FIDELITY.md`](../fidelity/AUDIO_ARCHIVE_FIDELITY.md).

Game runtime does not load audio from an external installation or workspace
path. Build and bundle steps copy only the repository-local `game_data/` tree.
Reconstruction-native synthesized cues remain available for events without a
mapped original resource.

SDL3 plays WAV ambience and DRS effects. macOS uses AudioToolbox for MP3 music,
covering both slices of the Universal 2 executable without Homebrew. Other
platforms use `mpg123` when configured:

```sh
cmake -S . -B build-audio \
  -DAOE_BUILD_SDL3=OFF -DAOE_ENABLE_MPG123=ON
cmake --build build-audio
```

If packaged files, decoder, or audio device are absent, gameplay
continues silently. `AOE_MUTE=1` disables all audio. `AOE_AUDIO_VOLUME` accepts
a master gain from `0.0` through `1.0` and defaults to `0.35`.

Match ends after one player loses all units and buildings. Simulation freezes
at final state; press `R` for a fresh match.

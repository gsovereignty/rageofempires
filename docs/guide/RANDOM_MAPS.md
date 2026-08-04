# Deterministic random maps

`aoe/random_map.hpp` exposes seeded, reconstruction-native Arabia, Black
Forest, Islands, and Rivers generators with six original named size presets:
Tiny (120), Small (144), Medium (168), Normal (200), Large (220), and Giant
(240 tiles per side). Normal is default. Islands applies original map-family
one-step size bump, reaching internal 255-tile extent from Giant. Generated maps
include fair two-player Dark Age starts, deterministic terrain/resources and
elevation, validation with bounded retry, stable map hashing, and Scenario66
serialization. See [docs/fidelity/RANDOM_MAP_FIDELITY.md](../fidelity/RANDOM_MAP_FIDELITY.md).

`aoe/rms_import.hpp` adds strict bounded inspection/import for an
evidence-backed subset of classic `.rms` sections and directives. Unsupported
map semantics retain exact line spans and refuse playable output. Accepted
scripts deterministically evaluate into Scenario66; full RMS compatibility is
not claimed. See [docs/fidelity/RMS_IMPORT_FIDELITY.md](../fidelity/RMS_IMPORT_FIDELITY.md).

`aoe/classic_save_import.hpp` provides bounded, read-only inspection of the
classic Genie save/record envelope: exact raw preservation, strict raw-DEFLATE
handling, proved version metadata, and structured diagnostics. Player/map/tick
metadata and conversion remain unavailable where evidence is incomplete. This
is not project Save110 compatibility. See
[docs/fidelity/SAVE_IMPORT_FIDELITY.md](../fidelity/SAVE_IMPORT_FIDELITY.md).

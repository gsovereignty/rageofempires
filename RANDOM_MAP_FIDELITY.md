# Deterministic random maps

This module generates reconstruction-native two-player scenarios. It does not
parse or claim compatibility with Ensemble random-map scripts (`.rms`).

## Historical shape used

The original *Age of Empires II* manual describes Arabia as an arid desert,
Black Forest as grass islands in dense trees, and its map setup exposes map
type and size choices. The original standard map list also includes Islands
and Rivers. These descriptions define broad identities, not byte-exact
generation rules:

- Original manual scan:
  <https://manuals.plus/m/bede02f48ca7b2aae252168379555520636b871dcd6c6fdba8ba2766ea0186e2.pdf>
- Current official map notes confirm these names remain distinct random-map
  families:
  <https://www.ageofempires.com/news/aoe2de-update-34699/>

## Reconstruction contract

- `RandomMapKind`: Arabia, Black Forest, Islands, Rivers.
- `RandomMapSize`: 48, 64, 80, or 96 square tiles.
- Optional blue/red civilization selections flow into generated scenarios;
  defaults remain generic.
- Fixed SplitMix64-derived PRNG; no platform RNG or floating random source.
- Blue/red starts use 180-degree paired placement.
- Each player receives one Town Center, three villagers, one scout, sheep,
  hunt, berries, gold, and stone.
- Arabia is open land with sparse forests; Black Forest has clearings joined
  by a narrow route; Islands has separate shore-ringed land masses and fish;
  Rivers has winding water, beaches, and shallow crossings.
- Elevation patches are deterministic and starting footprints are flattened.
- Validation checks start buildings/units, legal land, and expected land
  connectivity. Generation retries at most 16 derived seeds, then fails.
- `random_map_hash` covers dimensions, every terrain/elevation/resource tile,
  and all starting placements.
- `save_scenario` writes current Scenario66 output.

Property tests cover all four kinds and sizes over 24 seeds each, assert
same-seed hash identity, paired starting resources, validation, simulation
construction, and Scenario66 serialization.

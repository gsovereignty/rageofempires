#pragma once

#include <cstdint>
#include <string>

#include "aoe/scenario.hpp"

namespace aoe {

enum class RandomMapKind {
    arabia,
    black_forest,
    islands,
    rivers,
};

// Original map-size ladder recovered from the shipped executable; see
// ../../docs/fidelity/RANDOM_MAP_FIDELITY.md for the decompiled switch and tile counts.
enum class RandomMapSize {
    tiny,
    small,
    medium,
    normal,
    large,
    giant,
    // Index 6 in the original switch: 255 tiles, no recovered name string.
    maximum,
};

struct RandomMapSettings {
    RandomMapKind kind{RandomMapKind::arabia};
    // Modern choice: the recovered index-6 maximum (255 tiles). No original
    // default index was recovered; see ../../docs/fidelity/RANDOM_MAP_FIDELITY.md.
    RandomMapSize size{RandomMapSize::maximum};
    std::uint64_t seed{};
    Civilization blue_civilization{Civilization::generic};
    Civilization red_civilization{Civilization::generic};
};

struct RandomMapValidation {
    bool valid{};
    std::string reason;
};

[[nodiscard]] int random_map_dimension(RandomMapSize size);
[[nodiscard]] Scenario generate_random_map(
    const RandomMapSettings& settings
);
[[nodiscard]] RandomMapValidation validate_random_map(
    const Scenario& scenario,
    RandomMapKind kind
);
[[nodiscard]] std::string random_map_hash(const Scenario& scenario);

}  // namespace aoe

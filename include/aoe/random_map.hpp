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

enum class RandomMapSize {
    tiny,
    small,
    medium,
    large,
};

struct RandomMapSettings {
    RandomMapKind kind{RandomMapKind::arabia};
    RandomMapSize size{RandomMapSize::small};
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

#pragma once

#include <array>
#include <optional>
#include <string>

#include "aoe/random_map.hpp"

namespace aoe {

enum class OrdinarySlotKind { closed, human, computer };

struct OrdinaryPlayerSlot {
    OrdinarySlotKind kind{OrdinarySlotKind::closed};
    Civilization civilization{Civilization::britons};
    TeamId team{TeamId::none()};
};

struct OrdinaryMatchSetup {
    std::array<OrdinaryPlayerSlot, 8> slots{};
    PlayerSlotId local_slot{*PlayerSlotId::from_index(0)};

    [[nodiscard]] static OrdinaryMatchSetup standard();
    [[nodiscard]] std::optional<std::string> validate() const;
    [[nodiscard]] std::size_t occupied_count() const;
};

// Applies original ordinary-match roster semantics to a generated RMS map.
// Player colors remain stable slot colors; closed slots produce no entities.
[[nodiscard]] Scenario configure_ordinary_random_map(
    Scenario scenario,
    const OrdinaryMatchSetup& setup
);

}  // namespace aoe

#pragma once

#include <string>
#include <vector>

#include "aoe/simulation.hpp"

namespace aoe {

struct PlacementPreview {
    BuildingKind kind{BuildingKind::house};
    TilePosition origin{};
    std::vector<TilePosition> footprint;
    bool valid{};
    std::string reason;
    int wood_cost{};
    int stone_cost{};
    int gold_cost{};
};

PlacementPreview evaluate_building_placement(
    const Simulation& simulation,
    EntityId builder,
    BuildingKind kind,
    TilePosition origin
);
std::vector<TilePosition> deterministic_wall_segment(
    TilePosition first,
    TilePosition last
);

}  // namespace aoe

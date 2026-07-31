#pragma once

#include <span>

#include "aoe/types.hpp"

namespace aoe {

// Original matches present the local base first. Town Center selection is
// authoritative; unit and map-center fallbacks keep malformed/custom maps
// deterministic.
[[nodiscard]] inline TilePosition initial_camera_tile(
    std::span<const Building> buildings,
    std::span<const Unit> units,
    Player local_player,
    int map_width,
    int map_height
) {
    for (const Building& building : buildings) {
        if (building.owner == local_player &&
            building.kind == BuildingKind::town_center) {
            return building.position;
        }
    }
    for (const Unit& unit : units) {
        if (unit.owner == local_player && unit.garrisoned_in == 0) {
            return unit.position;
        }
    }
    return {
        map_width > 0 ? map_width / 2 : 0,
        map_height > 0 ? map_height / 2 : 0,
    };
}

}  // namespace aoe

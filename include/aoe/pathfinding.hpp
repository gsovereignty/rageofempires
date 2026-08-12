#pragma once

#include <functional>
#include <vector>

#include "aoe/game_map.hpp"

namespace aoe {

using TileBlocked = std::function<bool(TilePosition)>;
using TileTraversable = std::function<bool(TilePosition)>;

// Finds a shortest eight-directional route without cutting blocked corners.
// Returned path excludes start and includes goal. Empty result means start
// equals goal or no route exists.
std::vector<TilePosition> find_path(
    const GameMap& map,
    TilePosition start,
    TilePosition goal,
    const TileBlocked& blocked,
    const TileTraversable& traversable = {}
);

}  // namespace aoe

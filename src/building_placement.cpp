#include "aoe/building_placement.hpp"

#include <algorithm>
#include <cmath>

#include "aoe/game_rules.hpp"

namespace aoe {

PlacementPreview evaluate_building_placement(
    const Simulation& simulation,
    EntityId builder_id,
    BuildingKind kind,
    TilePosition origin
) {
    PlacementPreview result;
    result.kind = kind;
    result.origin = origin;
    const BuildingRules& rules = rules_for(kind);
    result.wood_cost = rules.wood_cost;
    result.stone_cost = rules.stone_cost;
    result.gold_cost = rules.gold_cost;
    const auto builder = std::ranges::find(
        simulation.units(), builder_id, &Unit::id
    );
    if (builder == simulation.units().end() ||
        builder->kind != UnitKind::villager ||
        builder->garrisoned_in != 0) {
        result.reason = "VILLAGER REQUIRED";
        return result;
    }
    for (int y = 0; y < rules.footprint_height; ++y) {
        for (int x = 0; x < rules.footprint_width; ++x) {
            const TilePosition tile{origin.x + x, origin.y + y};
            result.footprint.push_back(tile);
            const bool terrain =
                kind == BuildingKind::fish_trap
                    ? simulation.map().sailable(tile)
                    : simulation.map().contains(tile) &&
                        simulation.map().terrain_at(tile) == Terrain::grass;
            if (!terrain) {
                result.reason = "INVALID TERRAIN OR RESOURCE";
                return result;
            }
            if (std::ranges::any_of(
                    simulation.units(),
                    [tile](const Unit& unit) {
                        return unit.garrisoned_in == 0 &&
                            unit.position == tile;
                    }) ||
                std::ranges::any_of(
                    simulation.buildings(),
                    [tile](const Building& building) {
                        const BuildingRules& occupied =
                            rules_for(building.kind);
                        return tile.x >= building.position.x &&
                            tile.y >= building.position.y &&
                            tile.x < building.position.x +
                                occupied.footprint_width &&
                            tile.y < building.position.y +
                                occupied.footprint_height;
                    })) {
                result.reason = "FOOTPRINT OCCUPIED";
                return result;
            }
        }
    }
    const Economy& economy = simulation.economy(builder->owner);
    if (economy.wood < result.wood_cost ||
        economy.stone < result.stone_cost ||
        economy.gold < result.gold_cost) {
        result.reason = "INSUFFICIENT RESOURCES";
        return result;
    }
    const int elevation = simulation.map().elevation_at(result.footprint[0]);
    if (std::ranges::any_of(result.footprint, [&](TilePosition tile) {
            return simulation.map().elevation_at(tile) != elevation;
        })) {
        result.reason = "UNEVEN ELEVATION";
        return result;
    }
    result.valid = true;
    result.reason = "VALID";
    return result;
}

std::vector<TilePosition> deterministic_wall_segment(
    TilePosition first,
    TilePosition last
) {
    std::vector<TilePosition> result;
    int x = first.x;
    int y = first.y;
    const int dx = std::abs(last.x - first.x);
    const int sx = first.x < last.x ? 1 : -1;
    const int dy = -std::abs(last.y - first.y);
    const int sy = first.y < last.y ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        result.push_back({x, y});
        if (x == last.x && y == last.y) break;
        const int doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y += sy;
        }
    }
    return result;
}

}  // namespace aoe

#include "aoe/game_map.hpp"
#include "aoe/terrain_catalog.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace aoe {

GameMap::GameMap(int width, int height)
    : width_(width),
      height_(height),
      terrain_(static_cast<std::size_t>(width * height), Terrain::grass),
      elevations_(static_cast<std::size_t>(width * height), 0),
      resources_(static_cast<std::size_t>(width * height), 0),
      cliffs_(static_cast<std::size_t>(width * height), false) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("map dimensions must be positive");
    }
}

bool GameMap::contains(TilePosition position) const {
    return position.x >= 0 && position.y >= 0 &&
           position.x < width_ && position.y < height_;
}

std::size_t GameMap::index(TilePosition position) const {
    if (!contains(position)) {
        throw std::out_of_range("tile outside map");
    }
    return static_cast<std::size_t>(position.y * width_ + position.x);
}

Terrain GameMap::terrain_at(TilePosition position) const {
    return terrain_.at(index(position));
}

int GameMap::elevation_at(TilePosition position) const {
    return elevations_.at(index(position));
}

bool GameMap::walkable(TilePosition position) const {
    if (!contains(position)) return false;
    if (classic_terrain_id(terrain_at(position))) {
        return classic_terrain_passable(
            terrain_at(position),
            ClassicTerrainRestriction::land_unit
        );
    }
    switch (terrain_at(position)) {
        case Terrain::forest:
        case Terrain::pine_forest:
        case Terrain::oak_forest:
        case Terrain::bamboo_forest:
        case Terrain::palm_forest:
        case Terrain::jungle_forest:
        case Terrain::berry_bush:
        case Terrain::gold_mine:
        case Terrain::stone_mine:
        case Terrain::fish:
        case Terrain::fish_shore:
        case Terrain::fish_deep:
            return false;
        default:
            return false;
    }
}

bool GameMap::sailable(TilePosition position) const {
    if (!contains(position)) {
        return false;
    }
    const Terrain terrain = terrain_at(position);
    if (classic_terrain_id(terrain)) {
        return classic_terrain_passable(
            terrain, ClassicTerrainRestriction::ship
        );
    }
    return terrain == Terrain::fish || terrain == Terrain::fish_shore ||
        terrain == Terrain::fish_deep;
}

bool GameMap::buildable(TilePosition position) const {
    return contains(position) && classic_terrain_passable(
        terrain_at(position),
        ClassicTerrainRestriction::generic_building
    );
}

bool GameMap::supports_dock_foundation(TilePosition origin) const {
    // Original Dock 45: radius/construction radius 1.5 gives a 3x3
    // footprint, center_tile_req is Water (1) or Shallows (4), tile_req is
    // Beach (2) or Ice2 (35), and every footprint cell uses restriction 6.
    constexpr int footprint = 3;
    const TilePosition center{origin.x + 1, origin.y + 1};
    if (!contains(center)) return false;
    const Terrain center_terrain = terrain_at(center);
    if (center_terrain != Terrain::water &&
        center_terrain != Terrain::shallows) {
        return false;
    }
    for (int y = 0; y < footprint; ++y) {
        for (int x = 0; x < footprint; ++x) {
            const TilePosition tile{origin.x + x, origin.y + y};
            if (!contains(tile) || !classic_terrain_passable(
                    terrain_at(tile),
                    ClassicTerrainRestriction::dock
                )) {
                return false;
            }
        }
    }
    const auto coast = [this](TilePosition tile) {
        if (!contains(tile)) return false;
        const Terrain terrain = terrain_at(tile);
        // FUN_00577ac0 aliases Ice (26) and Ice Beach (37) to Dock 45's
        // Ice2 (35) perimeter requirement.
        return terrain == Terrain::beach || terrain == Terrain::ice2 ||
            terrain == Terrain::ice || terrain == Terrain::ice_beach;
    };
    for (int x = 0; x < footprint; ++x) {
        if (coast({origin.x + x, origin.y - 1}) ||
            coast({origin.x + x, origin.y + footprint})) {
            return true;
        }
    }
    for (int y = 0; y < footprint; ++y) {
        if (coast({origin.x - 1, origin.y + y}) ||
            coast({origin.x + footprint, origin.y + y})) {
            return true;
        }
    }
    return false;
}

bool GameMap::traversable(
    TilePosition from, TilePosition to
) const {
    if (!contains(from) || !walkable(to) || cliff_at(to)) return false;
    return std::abs(elevation_at(from) - elevation_at(to)) <= 1;
}

void GameMap::set_terrain(TilePosition position, Terrain terrain) {
    const std::size_t tile = index(position);
    const Terrain previous = terrain_.at(tile);
    terrain_.at(tile) = terrain;
    if (terrain != previous) {
        switch (terrain) {
            case Terrain::forest:
            case Terrain::pine_forest:
            case Terrain::oak_forest:
            case Terrain::bamboo_forest:
            case Terrain::palm_forest:
            case Terrain::jungle_forest:
                resources_.at(tile) = 100;
                break;
            case Terrain::berry_bush:
                resources_.at(tile) = 125;
                break;
            case Terrain::gold_mine:
                resources_.at(tile) = 800;
                break;
            case Terrain::stone_mine:
                resources_.at(tile) = 350;
                break;
            case Terrain::fish:
            case Terrain::fish_shore:
            case Terrain::fish_deep:
                resources_.at(tile) = 200;
                break;
            default:
                resources_.at(tile) = 0;
                break;
        }
    } else if (classic_terrain_id(terrain)) {
        resources_.at(tile) = 0;
    }
}

void GameMap::set_elevation(TilePosition position, int elevation) {
    if (elevation < 0 || elevation > 7) {
        throw std::invalid_argument(
            "tile elevation must be between 0 and 7"
        );
    }
    elevations_.at(index(position)) =
        static_cast<std::uint8_t>(elevation);
}

int GameMap::resource_amount_at(TilePosition position) const {
    return resources_.at(index(position));
}

bool GameMap::cliff_at(TilePosition position) const {
    return cliffs_.at(index(position));
}

void GameMap::set_cliff(TilePosition position, bool cliff) {
    cliffs_.at(index(position)) = cliff;
}

void GameMap::set_resource_amount(TilePosition position, int amount) {
    if (amount < 0) {
        throw std::invalid_argument("resource amount cannot be negative");
    }
    const Terrain terrain = terrain_at(position);
    if ((walkable(position) || terrain == Terrain::water ||
         terrain == Terrain::deep_water ||
         terrain == Terrain::beach || terrain == Terrain::shallows) &&
        amount != 0) {
        throw std::invalid_argument("plain terrain cannot contain resources");
    }
    resources_.at(index(position)) = amount;
}

int GameMap::take_resource(TilePosition position, int amount) {
    if (amount < 0) {
        throw std::invalid_argument("resource amount cannot be negative");
    }
    int& available = resources_.at(index(position));
    const int taken = std::min(available, amount);
    available -= taken;
    if (available == 0 && (terrain_at(position) == Terrain::fish ||
        terrain_at(position) == Terrain::fish_shore ||
        terrain_at(position) == Terrain::fish_deep)) {
        set_terrain(position, terrain_at(position) == Terrain::fish_deep
            ? Terrain::deep_water : Terrain::water);
    } else if (available == 0 &&
        terrain_at(position) != Terrain::grass &&
        terrain_at(position) != Terrain::water &&
        terrain_at(position) != Terrain::beach &&
        terrain_at(position) != Terrain::shallows) {
        set_terrain(position, Terrain::grass);
    }
    return taken;
}

GameMap GameMap::create_demo_map() {
    GameMap map(24, 16);

    for (int y = 0; y < map.height(); ++y) {
        map.set_terrain({11, y}, Terrain::water);
    }
    map.set_terrain({11, 7}, Terrain::grass);
    map.set_terrain({11, 8}, Terrain::grass);

    for (int y = 2; y <= 5; ++y) {
        for (int x = 3; x <= 6; ++x) {
            map.set_terrain({x, y}, Terrain::forest);
        }
    }
    for (int y = 10; y <= 13; ++y) {
        for (int x = 17; x <= 20; ++x) {
            map.set_terrain({x, y}, Terrain::forest);
        }
    }
    return map;
}

int apply_elevation_damage(
    const GameMap& map,
    TilePosition attacker,
    TilePosition defender,
    int damage
) {
    if (damage < 1) {
        throw std::invalid_argument("damage must be positive");
    }
    const int attacker_elevation = map.elevation_at(attacker);
    const int defender_elevation = map.elevation_at(defender);
    if (attacker_elevation > defender_elevation) {
        return std::max(1, damage * 125 / 100);
    }
    if (attacker_elevation < defender_elevation) {
        return std::max(1, damage * 75 / 100);
    }
    return damage;
}

}  // namespace aoe

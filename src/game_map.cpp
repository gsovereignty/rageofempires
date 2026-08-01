#include "aoe/game_map.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace aoe {

GameMap::GameMap(int width, int height)
    : width_(width),
      height_(height),
      terrain_(static_cast<std::size_t>(width * height), Terrain::grass),
      elevations_(static_cast<std::size_t>(width * height), 0),
      resources_(static_cast<std::size_t>(width * height), 0) {
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
    switch (terrain_at(position)) {
        case Terrain::grass:
        case Terrain::beach:
        case Terrain::shallows:
            return true;
        case Terrain::water:
        case Terrain::forest:
        case Terrain::berry_bush:
        case Terrain::gold_mine:
        case Terrain::stone_mine:
        case Terrain::fish:
            return false;
    }
    return false;
}

bool GameMap::sailable(TilePosition position) const {
    if (!contains(position)) {
        return false;
    }
    const Terrain terrain = terrain_at(position);
    return terrain == Terrain::water || terrain == Terrain::fish ||
        terrain == Terrain::beach || terrain == Terrain::shallows;
}

bool GameMap::traversable(
    TilePosition from, TilePosition to
) const {
    if (!contains(from) || !walkable(to)) return false;
    return std::abs(elevation_at(from) - elevation_at(to)) <= 1;
}

void GameMap::set_terrain(TilePosition position, Terrain terrain) {
    const std::size_t tile = index(position);
    const Terrain previous = terrain_.at(tile);
    terrain_.at(tile) = terrain;
    if (terrain != previous) {
        switch (terrain) {
            case Terrain::forest:
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
                resources_.at(tile) = 200;
                break;
            case Terrain::grass:
            case Terrain::water:
            case Terrain::beach:
            case Terrain::shallows:
                resources_.at(tile) = 0;
                break;
        }
    } else if (terrain == Terrain::grass || terrain == Terrain::water ||
               terrain == Terrain::beach || terrain == Terrain::shallows) {
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

void GameMap::set_resource_amount(TilePosition position, int amount) {
    if (amount < 0) {
        throw std::invalid_argument("resource amount cannot be negative");
    }
    const Terrain terrain = terrain_at(position);
    if ((terrain == Terrain::grass || terrain == Terrain::water ||
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
    if (available == 0 && terrain_at(position) == Terrain::fish) {
        set_terrain(position, Terrain::water);
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

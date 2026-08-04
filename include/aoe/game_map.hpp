#pragma once

#include <cstdint>
#include <vector>

#include "aoe/types.hpp"

namespace aoe {

class GameMap {
public:
    GameMap(int width, int height);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool contains(TilePosition position) const;
    [[nodiscard]] Terrain terrain_at(TilePosition position) const;
    [[nodiscard]] int elevation_at(TilePosition position) const;
    [[nodiscard]] bool walkable(TilePosition position) const;
    [[nodiscard]] bool sailable(TilePosition position) const;
    [[nodiscard]] bool traversable(
        TilePosition from, TilePosition to
    ) const;
    [[nodiscard]] int resource_amount_at(TilePosition position) const;
    [[nodiscard]] bool cliff_at(TilePosition position) const;

    void set_terrain(TilePosition position, Terrain terrain);
    void set_elevation(TilePosition position, int elevation);
    void set_resource_amount(TilePosition position, int amount);
    void set_cliff(TilePosition position, bool cliff);
    int take_resource(TilePosition position, int amount);
    static GameMap create_demo_map();

private:
    [[nodiscard]] std::size_t index(TilePosition position) const;

    int width_;
    int height_;
    std::vector<Terrain> terrain_;
    std::vector<std::uint8_t> elevations_;
    std::vector<int> resources_;
    std::vector<bool> cliffs_;
};

[[nodiscard]] int apply_elevation_damage(
    const GameMap& map,
    TilePosition attacker,
    TilePosition defender,
    int damage
);

}  // namespace aoe

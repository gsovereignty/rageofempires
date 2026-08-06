#include "aoe/terrain_catalog.hpp"

#include <array>

namespace aoe {
namespace {

constexpr std::array<ClassicTerrainRecord, 41> catalog{{
    {0, Terrain::grass, "Grass", "grass", 15001},
    {1, Terrain::water, "Water", "water", 15002},
    {2, Terrain::beach, "Beach", "beach", 15017},
    {3, Terrain::dirt3, "Dirt 3", "dirt3", 15007},
    {4, Terrain::shallows, "Shallows", "shallows", 15014},
    {5, Terrain::leaves, "Leaves", "leaves", -1},
    {6, Terrain::dirt, "Dirt", "dirt", 15000},
    {7, Terrain::farm1, "Farm1", "farm1", 15004},
    {8, Terrain::farm2, "Farm2", "farm2", 15005},
    {9, Terrain::grass3, "Grass 3", "grass3", 15009},
    {10, Terrain::classic_forest, "Forest", "classic_forest", 15011},
    {11, Terrain::dirt2, "Dirt 2", "dirt2", 15006},
    {12, Terrain::grass2, "Grass 2", "grass2", 15008},
    {13, Terrain::palm_desert, "Palm Desert", "palm_desert", 15010},
    {14, Terrain::desert, "Desert", "desert", -1},
    {15, Terrain::old_water, "Old Water", "old_water", -1},
    {16, Terrain::old_grass, "Old Grass", "old_grass", -1},
    {17, Terrain::jungle, "Jungle", "jungle", -1},
    {18, Terrain::bamboo, "Bamboo", "bamboo", -1},
    {19, Terrain::pine_forest_floor, "Pine Forest", "pine_forest_floor", -1},
    {20, Terrain::oak_forest_floor, "Oak Forest", "oak_forest_floor", -1},
    {21, Terrain::snow_forest, "Snow Forest", "snow_forest", 15029},
    {22, Terrain::deep_water, "Water 2", "deep_water", 15015},
    {23, Terrain::medium_water, "Water 3", "medium_water", 15016},
    {24, Terrain::road, "Road", "road", 15018},
    {25, Terrain::road2, "Road 2", "road2", 15019},
    {26, Terrain::ice, "Ice", "ice", 15024},
    {27, Terrain::foundation, "Foundation", "foundation", 15006},
    {28, Terrain::water_bridge, "Water Bridge", "water_bridge", -1},
    {29, Terrain::farm_construction1, "Farm Cnst1", "farm_construction1", 15021},
    {30, Terrain::farm_construction2, "Farm Cnst2", "farm_construction2", 15022},
    {31, Terrain::farm_construction3, "Farm Cnst3", "farm_construction3", 15023},
    {32, Terrain::snow, "Snow", "snow", 15026},
    {33, Terrain::snow_dirt, "Snow Dirt", "snow_dirt", 15027},
    {34, Terrain::snow_grass, "Snow Grass", "snow_grass", 15028},
    {35, Terrain::ice2, "Ice 2", "ice2", -1},
    {36, Terrain::snow_foundation, "Snow Foundati", "snow_foundation", -1},
    {37, Terrain::ice_beach, "Ice Beach", "ice_beach", -1},
    {38, Terrain::snow_road, "Snow Road", "snow_road", 15030},
    {39, Terrain::snow_road2, "Snow Road 2", "snow_road2", 15031},
    {40, Terrain::koh, "KOH", "koh", 15018},
}};

bool listed(std::uint8_t id, std::span<const std::uint8_t> ids) noexcept {
    for (const std::uint8_t candidate : ids) {
        if (candidate == id) return true;
    }
    return false;
}

}  // namespace

std::span<const ClassicTerrainRecord> classic_terrain_catalog() noexcept {
    return catalog;
}

std::optional<Terrain> classic_terrain_from_id(std::uint8_t id) noexcept {
    if (id >= catalog.size()) return std::nullopt;
    return catalog[id].terrain;
}

std::optional<std::uint8_t> classic_terrain_id(Terrain terrain) noexcept {
    for (const ClassicTerrainRecord& record : catalog) {
        if (record.terrain == terrain) return record.id;
    }
    return std::nullopt;
}

const ClassicTerrainRecord* classic_terrain_record(Terrain terrain) noexcept {
    const auto id = classic_terrain_id(terrain);
    return id ? &catalog[*id] : nullptr;
}

std::optional<Terrain> classic_terrain_from_token(
    std::string_view token
) noexcept {
    for (const ClassicTerrainRecord& record : catalog) {
        if (record.token == token) return record.terrain;
    }
    return std::nullopt;
}

bool classic_terrain_passable(
    Terrain terrain,
    ClassicTerrainRestriction restriction
) noexcept {
    const auto id = classic_terrain_id(terrain);
    if (!id) return false;
    constexpr std::array<std::uint8_t, 8> ship{
        1, 2, 4, 15, 22, 23, 26, 37
    };
    constexpr std::array<std::uint8_t, 9> dock{
        1, 2, 4, 15, 22, 23, 26, 35, 37
    };
    constexpr std::array<std::uint8_t, 8> fish_trap{
        1, 2, 4, 15, 22, 23, 26, 37
    };
    switch (restriction) {
        case ClassicTerrainRestriction::ship:
            return listed(*id, ship);
        case ClassicTerrainRestriction::dock:
            return listed(*id, dock);
        case ClassicTerrainRestriction::fish_trap:
            return listed(*id, fish_trap);
        case ClassicTerrainRestriction::land_unit:
            return *id != 1 && *id != 15 && *id != 22 && *id != 23;
        case ClassicTerrainRestriction::generic_building:
            return *id != 1 && *id != 2 && *id != 4 && *id != 15 &&
                *id != 22 && *id != 23 && *id != 26 && *id != 28 &&
                *id != 35 && *id != 37 && *id != 40;
    }
    return false;
}

}  // namespace aoe

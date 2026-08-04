#include "aoe/random_map.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace aoe {
namespace {

class FixedRandom {
public:
    explicit FixedRandom(std::uint64_t seed)
        : state_(seed ^ 0x9e3779b97f4a7c15ULL) {}

    std::uint64_t next() {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    int between(int low, int high) {
        return low + static_cast<int>(
            next() % static_cast<std::uint64_t>(high - low + 1)
        );
    }

private:
    std::uint64_t state_;
};

TilePosition mirror(const GameMap& map, TilePosition tile) {
    return {map.width() - 1 - tile.x, map.height() - 1 - tile.y};
}

void set_pair(GameMap& map, TilePosition tile, Terrain terrain) {
    map.set_terrain(tile, terrain);
    map.set_terrain(mirror(map, tile), terrain);
}

void paint_disc(
    GameMap& map, TilePosition center, int radius, Terrain terrain
) {
    for (int y = center.y - radius; y <= center.y + radius; ++y) {
        for (int x = center.x - radius; x <= center.x + radius; ++x) {
            const TilePosition tile{x, y};
            if (!map.contains(tile)) continue;
            const int dx = x - center.x;
            const int dy = y - center.y;
            if (dx * dx + dy * dy <= radius * radius) {
                map.set_terrain(tile, terrain);
            }
        }
    }
}

// Modern choice: no original feature-scaling rule was recovered. Blob
// counts already grow with the dimension, but the radii were fixed, so
// coverage grew as the dimension while the map grew as its square and the
// large presets came out nearly featureless. Scaling a radius by
// sqrt(dimension / reference) makes blob area grow with the dimension, so
// count x area tracks map area and feature density is the same at every
// preset. The reference is 96, the largest dimension the fixed radii were
// authored against, and the scale never shrinks a radius, so every preset
// at or below 96 tiles generates exactly as before.
constexpr int feature_reference_dimension = 96;

[[nodiscard]] int scale_feature_area(int extent, int dimension) {
    const double factor = std::sqrt(
        static_cast<double>(dimension) /
        static_cast<double>(feature_reference_dimension)
    );
    return std::max(
        extent,
        static_cast<int>(std::lround(static_cast<double>(extent) * factor))
    );
}

// Corridors are one-dimensional, so their widths scale with the dimension
// itself rather than with its square root; a five-tile river across 255
// tiles reads as a scratch.
[[nodiscard]] int scale_feature_span(int extent, int dimension) {
    return std::max(
        extent,
        extent * dimension / feature_reference_dimension
    );
}

void paint_symmetric_blobs(
    GameMap& map,
    FixedRandom& random,
    Terrain terrain,
    int count,
    int min_radius,
    int max_radius
) {
    const int dimension = std::max(map.width(), map.height());
    for (int index = 0; index < count; ++index) {
        const TilePosition center{
            random.between(3, map.width() / 2 - 2),
            random.between(3, map.height() - 4),
        };
        const int radius = scale_feature_area(
            random.between(min_radius, max_radius),
            dimension
        );
        paint_disc(map, center, radius, terrain);
        paint_disc(map, mirror(map, center), radius, terrain);
    }
}

void add_start(
    Scenario& scenario,
    Player player,
    Civilization civilization,
    TilePosition center
) {
    const auto place_unit = [&](UnitKind kind, Player owner,
                                TilePosition position) {
        // Feature blobs can extend beyond the cleared town-center disc.
        // Starting units must never inherit blocking resource terrain.
        scenario.map.set_terrain(position, Terrain::grass);
        scenario.units.push_back({
            kind, owner, position, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, false, {},
            UnitStance::aggressive, std::nullopt,
        });
    };
    scenario.buildings.push_back({
        BuildingKind::town_center, player, center, std::nullopt,
        std::nullopt, std::nullopt,
    });
    constexpr std::array<TilePosition, 3> villagers{{
        {-1, 1}, {1, -1}, {4, 1},
    }};
    for (const TilePosition offset : villagers) {
        place_unit(
            UnitKind::villager, player,
            {center.x + offset.x, center.y + offset.y}
        );
    }
    const int extra_villagers =
        civilization == Civilization::chinese ? 3 :
        civilization == Civilization::mayans ? 1 : 0;
    constexpr std::array<TilePosition, 3> extra_positions{{
        {-1, 4}, {1, 4}, {3, 4},
    }};
    for (int index = 0; index < extra_villagers; ++index) {
        place_unit(
            UnitKind::villager, player,
            {
                center.x + extra_positions[index].x,
                center.y + extra_positions[index].y,
            }
        );
    }
    place_unit(
        civilization == Civilization::aztecs ||
            civilization == Civilization::mayans
            ? UnitKind::eagle_warrior
            : UnitKind::scout_cavalry,
        player, {center.x + 2, center.y + 5}
    );
    constexpr std::array<TilePosition, 4> sheep{{
        {-2, -1}, {0, -2}, {3, -2}, {5, -1},
    }};
    for (const TilePosition offset : sheep) {
        place_unit(
            UnitKind::sheep, player,
            {center.x + offset.x, center.y + offset.y}
        );
    }
    place_unit(
        UnitKind::boar, Player::neutral,
        {center.x - 6, center.y + 2}
    );
    place_unit(
        UnitKind::deer, Player::neutral,
        {center.x + 5, center.y + 4}
    );
    place_unit(
        UnitKind::deer, Player::neutral,
        {center.x + 6, center.y + 4}
    );
}

void add_resource_pair(
    Scenario& scenario,
    TilePosition blue,
    Terrain terrain,
    int amount
) {
    const auto occupied_by_starting_unit = [&scenario](TilePosition tile) {
        return std::ranges::any_of(
            scenario.units,
            [tile](const UnitPlacement& unit) {
                return unit.position == tile;
            }
        );
    };
    while (occupied_by_starting_unit(blue) ||
           occupied_by_starting_unit(mirror(scenario.map, blue))) {
        --blue.y;
    }
    const TilePosition red = mirror(scenario.map, blue);
    scenario.map.set_terrain(blue, terrain);
    scenario.map.set_resource_amount(blue, amount);
    scenario.map.set_terrain(red, terrain);
    scenario.map.set_resource_amount(red, amount);
}

void clear_start(Scenario& scenario, TilePosition center) {
    paint_disc(scenario.map, center, 8, Terrain::grass);
    for (int y = center.y - 8; y <= center.y + 8; ++y) {
        for (int x = center.x - 8; x <= center.x + 8; ++x) {
            const TilePosition tile{x, y};
            if (scenario.map.contains(tile)) {
                scenario.map.set_elevation(tile, 0);
            }
        }
    }
}

void add_standard_starts(
    Scenario& scenario, TilePosition blue, TilePosition red
) {
    clear_start(scenario, blue);
    clear_start(scenario, red);
    add_start(
        scenario, Player::blue, scenario.blue_civilization, blue
    );
    add_start(
        scenario, Player::red, scenario.red_civilization, red
    );
    const auto apply_starting_economy = [&scenario](Player player) {
        const Civilization civilization =
            player == Player::blue
                ? scenario.blue_civilization
                : scenario.red_civilization;
        Economy& economy =
            player == Player::blue
                ? scenario.blue_economy : scenario.red_economy;
        if (civilization == Civilization::chinese) {
            economy.wood -= 50;
            economy.food -= 200;
        } else if (civilization == Civilization::mayans) {
            economy.food -= 50;
        }
    };
    apply_starting_economy(Player::blue);
    apply_starting_economy(Player::red);
    add_resource_pair(
        scenario, {blue.x - 5, blue.y - 4}, Terrain::berry_bush, 125
    );
    add_resource_pair(
        scenario, {blue.x + 6, blue.y - 3}, Terrain::gold_mine, 800
    );
    add_resource_pair(
        scenario, {blue.x - 6, blue.y + 5}, Terrain::stone_mine, 700
    );
}

void add_elevation(Scenario& scenario, FixedRandom& random) {
    const int dimension =
        std::max(scenario.map.width(), scenario.map.height());
    // Same density argument as the terrain blobs: count grows with the
    // dimension and radius with its square root, so hill coverage tracks
    // map area instead of thinning out on the larger presets.
    const int blobs = std::max(8, 8 * dimension / feature_reference_dimension);
    for (int blob = 0; blob < blobs; ++blob) {
        const TilePosition center{
            random.between(4, scenario.map.width() / 2 - 2),
            random.between(4, scenario.map.height() - 5),
        };
        const int radius =
            scale_feature_area(random.between(2, 5), dimension);
        for (int y = center.y - radius; y <= center.y + radius; ++y) {
            for (int x = center.x - radius; x <= center.x + radius; ++x) {
                const TilePosition tile{x, y};
                if (!scenario.map.contains(tile)) continue;
                const int distance =
                    std::abs(x - center.x) + std::abs(y - center.y);
                const int elevation = distance < radius / 2 ? 2 : 1;
                scenario.map.set_elevation(tile, elevation);
                scenario.map.set_elevation(
                    mirror(scenario.map, tile), elevation
                );
            }
        }
    }
}

Scenario generate_once(
    const RandomMapSettings& settings, std::uint64_t attempt
) {
    const int dimension = random_map_dimension(settings.size);
    Scenario scenario(dimension, dimension);
    scenario.blue_civilization = settings.blue_civilization;
    scenario.red_civilization = settings.red_civilization;
    FixedRandom random(settings.seed + attempt * 0x9e3779b97f4a7c15ULL);
    const TilePosition blue{dimension / 4, dimension / 2};
    const TilePosition red = mirror(scenario.map, blue);

    if (settings.kind == RandomMapKind::islands) {
        for (int y = 0; y < dimension; ++y) {
            for (int x = 0; x < dimension; ++x) {
                scenario.map.set_terrain({x, y}, Terrain::water);
            }
        }
        const int radius = dimension / 5;
        paint_disc(scenario.map, blue, radius + 1, Terrain::beach);
        paint_disc(scenario.map, red, radius + 1, Terrain::beach);
        paint_disc(scenario.map, blue, radius, Terrain::grass);
        paint_disc(scenario.map, red, radius, Terrain::grass);
        paint_symmetric_blobs(
            scenario.map, random, Terrain::forest,
            std::max(3, dimension / 20), 2, 3
        );
        for (int index = 0; index < dimension / 8; ++index) {
            TilePosition fish{
                random.between(dimension / 2 - 5, dimension / 2 + 5),
                random.between(2, dimension - 3),
            };
            set_pair(scenario.map, fish, Terrain::fish);
        }
    } else if (settings.kind == RandomMapKind::black_forest) {
        for (int y = 0; y < dimension; ++y) {
            for (int x = 0; x < dimension; ++x) {
                scenario.map.set_terrain({x, y}, Terrain::forest);
            }
        }
        paint_disc(scenario.map, blue, dimension / 7, Terrain::grass);
        paint_disc(scenario.map, red, dimension / 7, Terrain::grass);
        const int route_half_width = scale_feature_span(2, dimension);
        for (int x = blue.x; x <= red.x; ++x) {
            const int bend = static_cast<int>(
                std::sin(static_cast<double>(x) / 5.0) * 2.0
            );
            paint_disc(
                scenario.map, {x, dimension / 2 + bend}, route_half_width,
                Terrain::grass
            );
        }
    } else {
        if (settings.kind == RandomMapKind::arabia) {
            paint_symmetric_blobs(
                scenario.map, random, Terrain::forest,
                std::max(6, dimension / 8), 2, 4
            );
        } else {
            paint_symmetric_blobs(
                scenario.map, random, Terrain::forest,
                std::max(5, dimension / 10), 2, 3
            );
            const int river_half_width = scale_feature_span(2, dimension);
            const int ford_half_height =
                scale_feature_span(1, dimension) / 2;
            for (int y = 0; y < dimension; ++y) {
                const int bend = static_cast<int>(
                    std::sin(static_cast<double>(y) / 6.0) * 3.0
                );
                const int center = dimension / 2 + bend;
                for (int x = center - river_half_width;
                     x <= center + river_half_width; ++x) {
                    scenario.map.set_terrain({x, y}, Terrain::water);
                }
                scenario.map.set_terrain(
                    {center - river_half_width - 1, y}, Terrain::beach
                );
                scenario.map.set_terrain(
                    {center + river_half_width + 1, y}, Terrain::beach
                );
            }
            // The fords have to grow with the river, or a wide river has
            // no crossing at all.
            for (int ford : {dimension / 3, 2 * dimension / 3}) {
                for (int y = ford - ford_half_height;
                     y <= ford + ford_half_height; ++y) {
                    if (y < 0 || y >= dimension) continue;
                    const int center = dimension / 2 + static_cast<int>(
                        std::sin(static_cast<double>(y) / 6.0) * 3.0
                    );
                    for (int x = center - river_half_width;
                         x <= center + river_half_width; ++x) {
                        scenario.map.set_terrain({x, y}, Terrain::shallows);
                    }
                }
            }
        }
    }
    add_elevation(scenario, random);
    add_standard_starts(scenario, blue, red);
    return scenario;
}

}  // namespace

int random_map_dimension(RandomMapSize size) {
    // Original tile counts, from the map-size switch in FUN_00622010
    // (AoK-HD-patched.c:356938-356960).
    switch (size) {
        case RandomMapSize::tiny: return 120;
        case RandomMapSize::small: return 144;
        case RandomMapSize::medium: return 168;
        case RandomMapSize::normal: return 200;
        case RandomMapSize::large: return 220;
        case RandomMapSize::giant: return 240;
        case RandomMapSize::maximum: return 255;
    }
    return 144;
}

RandomMapValidation validate_random_map(
    const Scenario& scenario, RandomMapKind kind
) {
    std::array<TilePosition, 2> starts{};
    std::array<bool, 2> found{};
    for (const BuildingPlacement& building : scenario.buildings) {
        if (building.kind != BuildingKind::town_center) continue;
        const int slot = building.owner == Player::blue ? 0 :
                         building.owner == Player::red ? 1 : -1;
        if (slot >= 0 && !found[slot]) {
            starts[slot] = building.position;
            found[slot] = true;
        }
    }
    if (!found[0] || !found[1]) return {false, "missing town center"};
    for (TilePosition start : starts) {
        if (!scenario.map.walkable(start)) {
            return {false, "town center is not on land"};
        }
    }
    std::vector<bool> visited(
        static_cast<std::size_t>(
            scenario.map.width() * scenario.map.height()
        )
    );
    std::deque<TilePosition> queue{starts[0]};
    visited[static_cast<std::size_t>(
        starts[0].y * scenario.map.width() + starts[0].x
    )] = true;
    constexpr std::array<TilePosition, 4> directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    }};
    while (!queue.empty()) {
        const TilePosition current = queue.front();
        queue.pop_front();
        for (TilePosition direction : directions) {
            const TilePosition next{
                current.x + direction.x, current.y + direction.y,
            };
            if (!scenario.map.contains(next) ||
                !scenario.map.walkable(next)) continue;
            const std::size_t index = static_cast<std::size_t>(
                next.y * scenario.map.width() + next.x
            );
            if (visited[index]) continue;
            visited[index] = true;
            queue.push_back(next);
        }
    }
    const bool connected = visited[static_cast<std::size_t>(
        starts[1].y * scenario.map.width() + starts[1].x
    )];
    if (kind != RandomMapKind::islands && !connected) {
        return {false, "land starts are disconnected"};
    }
    if (kind == RandomMapKind::islands && connected) {
        return {false, "island starts share land"};
    }
    for (Player player : {Player::blue, Player::red}) {
        int villagers{};
        int scouts{};
        for (const UnitPlacement& unit : scenario.units) {
            if (unit.owner != player) continue;
            villagers += unit.kind == UnitKind::villager;
            scouts += unit.kind == UnitKind::scout_cavalry ||
                unit.kind == UnitKind::eagle_warrior;
        }
        const Civilization civilization =
            player == Player::blue
                ? scenario.blue_civilization
                : scenario.red_civilization;
        const int expected_villagers =
            civilization == Civilization::chinese ? 6 :
            civilization == Civilization::mayans ? 4 : 3;
        if (villagers != expected_villagers || scouts != 1) {
            return {false, "invalid player start units"};
        }
    }
    return {true, {}};
}

Scenario generate_random_map(const RandomMapSettings& settings) {
    std::string last_reason;
    for (std::uint64_t attempt = 0; attempt < 16; ++attempt) {
        Scenario scenario = generate_once(settings, attempt);
        const RandomMapValidation validation =
            validate_random_map(scenario, settings.kind);
        if (validation.valid) {
            return scenario;
        }
        last_reason = validation.reason;
    }
    throw std::runtime_error(
        "random map generation exhausted retries: " + last_reason
    );
}

std::string random_map_hash(const Scenario& scenario) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto add = [&hash](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (value >> (byte * 8)) & 0xffU;
            hash *= 1099511628211ULL;
        }
    };
    add(static_cast<std::uint64_t>(scenario.map.width()));
    add(static_cast<std::uint64_t>(scenario.map.height()));
    add(static_cast<std::uint64_t>(scenario.blue_civilization));
    add(static_cast<std::uint64_t>(scenario.red_civilization));
    for (const Economy economy : {
             scenario.blue_economy, scenario.red_economy,
         }) {
        add(static_cast<std::uint64_t>(economy.wood));
        add(static_cast<std::uint64_t>(economy.food));
        add(static_cast<std::uint64_t>(economy.gold));
        add(static_cast<std::uint64_t>(economy.stone));
    }
    for (int y = 0; y < scenario.map.height(); ++y) {
        for (int x = 0; x < scenario.map.width(); ++x) {
            const TilePosition tile{x, y};
            add(static_cast<std::uint64_t>(scenario.map.terrain_at(tile)));
            add(static_cast<std::uint64_t>(
                scenario.map.elevation_at(tile)
            ));
            add(static_cast<std::uint64_t>(
                scenario.map.resource_amount_at(tile)
            ));
            add(static_cast<std::uint64_t>(scenario.map.cliff_at(tile)));
        }
    }
    for (const UnitPlacement& unit : scenario.units) {
        add(static_cast<std::uint64_t>(unit.kind));
        const auto legacy_owner = unit.owner.legacy_player();
        add(legacy_owner
            ? static_cast<std::uint64_t>(*legacy_owner)
            : 3U + unit.owner.stable_id());
        add(static_cast<std::uint64_t>(unit.position.x));
        add(static_cast<std::uint64_t>(unit.position.y));
    }
    for (const BuildingPlacement& building : scenario.buildings) {
        add(static_cast<std::uint64_t>(building.kind));
        const auto legacy_owner = building.owner.legacy_player();
        add(legacy_owner
            ? static_cast<std::uint64_t>(*legacy_owner)
            : 3U + building.owner.stable_id());
        add(static_cast<std::uint64_t>(building.position.x));
        add(static_cast<std::uint64_t>(building.position.y));
    }
    std::ostringstream output;
    output << "random-map-fnv1a64:" << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return output.str();
}

}  // namespace aoe

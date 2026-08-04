#include "aoe/frontend_game_modes.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "aoe/game_rules.hpp"

namespace aoe {
namespace {

constexpr Economy death_match_resources{
    .wood = 20'000,
    .food = 20'000,
    .gold = 10'000,
    .stone = 5'000,
};

std::array<TilePosition, 2> town_centers(const Scenario& scenario) {
    std::array<TilePosition, 2> result{};
    std::array<bool, 2> found{};
    for (const BuildingPlacement& building : scenario.buildings) {
        if (building.kind != BuildingKind::town_center) continue;
        const int index = building.owner == Player::blue ? 0 :
            building.owner == Player::red ? 1 : -1;
        if (index >= 0 && !found[static_cast<std::size_t>(index)]) {
            result[static_cast<std::size_t>(index)] = building.position;
            found[static_cast<std::size_t>(index)] = true;
        }
    }
    if (!found[0] || !found[1]) {
        throw std::invalid_argument(
            "frontend game mode requires two town centers"
        );
    }
    return result;
}

UnitPlacement placed(UnitKind kind, Player owner, TilePosition position) {
    return {
        kind, owner, position, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, false, {}, UnitStance::aggressive,
        std::nullopt,
    };
}

TilePosition castle_site(
    const Scenario& scenario,
    TilePosition start,
    bool prefer_left
) {
    constexpr int width = 4;
    constexpr int height = 4;
    for (int distance = 4; distance <= 12; ++distance) {
        for (int y_offset = -distance; y_offset <= distance; ++y_offset) {
            const int x_offset = prefer_left ? -distance : distance;
            const TilePosition candidate{
                start.x + x_offset, start.y + y_offset
            };
            bool free = true;
            for (int y = 0; y < height && free; ++y) {
                for (int x = 0; x < width && free; ++x) {
                    const TilePosition tile{candidate.x + x, candidate.y + y};
                    free = scenario.map.contains(tile) &&
                        scenario.map.terrain_at(tile) == Terrain::grass &&
                        std::ranges::none_of(
                            scenario.units,
                            [tile](const UnitPlacement& unit) {
                                return unit.position == tile;
                            }
                        ) && std::ranges::none_of(
                            scenario.buildings,
                            [tile](const BuildingPlacement& building) {
                                const BuildingRules& rules =
                                    rules_for(building.kind);
                                return tile.x >= building.position.x &&
                                    tile.y >= building.position.y &&
                                    tile.x < building.position.x +
                                        rules.footprint_width &&
                                    tile.y < building.position.y +
                                        rules.footprint_height;
                            }
                        );
                }
            }
            if (free) return candidate;
        }
    }
    throw std::invalid_argument("random map has no Regicide castle site");
}

void add_regicide_villagers(
    Scenario& scenario,
    Player player,
    TilePosition start
) {
    int villagers = static_cast<int>(std::ranges::count_if(
        scenario.units,
        [player](const UnitPlacement& unit) {
            return unit.owner == player &&
                unit.kind == UnitKind::villager;
        }
    ));
    for (int distance = 2; villagers < 10 && distance <= 8; ++distance) {
        for (int y = -distance; y <= distance && villagers < 10; ++y) {
            for (int x = -distance; x <= distance && villagers < 10; ++x) {
                if (std::abs(x) != distance && std::abs(y) != distance) {
                    continue;
                }
                const TilePosition tile{start.x + x, start.y + y};
                if (!scenario.map.contains(tile) ||
                    scenario.map.terrain_at(tile) != Terrain::grass ||
                    std::ranges::any_of(
                        scenario.units,
                        [tile](const UnitPlacement& unit) {
                            return unit.position == tile;
                        }
                    ) || std::ranges::any_of(
                        scenario.buildings,
                        [tile](const BuildingPlacement& building) {
                            const BuildingRules& rules =
                                rules_for(building.kind);
                            return tile.x >= building.position.x &&
                                tile.y >= building.position.y &&
                                tile.x < building.position.x +
                                    rules.footprint_width &&
                                tile.y < building.position.y +
                                    rules.footprint_height;
                        }
                    )) {
                    continue;
                }
                scenario.units.push_back(placed(
                    UnitKind::villager, player, tile
                ));
                ++villagers;
            }
        }
    }
    if (villagers != 10) {
        throw std::invalid_argument(
            "random map has no Regicide Villager start"
        );
    }
}

TilePosition king_site(const Scenario& scenario, TilePosition start) {
    for (int distance = 2; distance <= 10; ++distance) {
        for (int y = -distance; y <= distance; ++y) {
            for (int x = -distance; x <= distance; ++x) {
                if (std::abs(x) != distance && std::abs(y) != distance) {
                    continue;
                }
                const TilePosition tile{start.x + x, start.y + y};
                if (scenario.map.contains(tile) &&
                    scenario.map.terrain_at(tile) == Terrain::grass &&
                    std::ranges::none_of(
                        scenario.units,
                        [tile](const UnitPlacement& unit) {
                            return unit.position == tile;
                        }
                    ) && std::ranges::none_of(
                        scenario.buildings,
                        [tile](const BuildingPlacement& building) {
                            const BuildingRules& rules =
                                rules_for(building.kind);
                            return tile.x >= building.position.x &&
                                tile.y >= building.position.y &&
                                tile.x < building.position.x +
                                    rules.footprint_width &&
                                tile.y < building.position.y +
                                    rules.footprint_height;
                        }
                    )) {
                    return tile;
                }
            }
        }
    }
    throw std::invalid_argument("random map has no Regicide King site");
}

void configure_regicide(Scenario& scenario) {
    const auto starts = town_centers(scenario);
    scenario.blue_age = Age::castle;
    scenario.red_age = Age::castle;
    scenario.match_rules.conquest_enabled = false;
    scenario.match_rules.wonder_enabled = false;
    scenario.match_rules.relic_enabled = false;
    scenario.match_rules.regicide_enabled = true;

    add_regicide_villagers(
        scenario, Player::blue, starts[0]
    );
    add_regicide_villagers(
        scenario, Player::red, starts[1]
    );

    const std::array<TilePosition, 2> king_positions{{
        king_site(scenario, starts[0]),
        king_site(scenario, starts[1]),
    }};
    scenario.units.push_back(placed(
        UnitKind::king, Player::blue, king_positions[0]
    ));
    scenario.match_rules.blue_king =
        static_cast<EntityId>(scenario.units.size());
    scenario.units.push_back(placed(
        UnitKind::king, Player::red, king_positions[1]
    ));
    scenario.match_rules.red_king =
        static_cast<EntityId>(scenario.units.size());
    scenario.buildings.push_back({
        BuildingKind::castle, Player::blue,
        castle_site(scenario, starts[0], true),
        std::nullopt, std::nullopt, std::nullopt,
    });
    scenario.buildings.push_back({
        BuildingKind::castle, Player::red,
        castle_site(scenario, starts[1], false),
        std::nullopt, std::nullopt, std::nullopt,
    });
}

void configure_death_match(Scenario& scenario) {
    scenario.blue_economy = death_match_resources;
    scenario.red_economy = death_match_resources;
    scenario.blue_age = Age::imperial;
    scenario.red_age = Age::imperial;
    scenario.blue_technologies.clear();
    scenario.red_technologies.clear();
    for (std::size_t index = 0; index < technology_count; ++index) {
        const Technology technology = static_cast<Technology>(index);
        if (civilization_has_technology(
                scenario.blue_civilization, technology
            )) {
            scenario.blue_technologies.push_back(technology);
        }
        if (civilization_has_technology(
                scenario.red_civilization, technology
            )) {
            scenario.red_technologies.push_back(technology);
        }
    }
    scenario.match_rules.conquest_enabled = true;
    scenario.match_rules.wonder_enabled = true;
    scenario.match_rules.relic_enabled = true;
    scenario.match_rules.regicide_enabled = false;
}

}  // namespace

Scenario configure_frontend_game_mode(
    Scenario scenario,
    FrontendGameMode mode
) {
    switch (mode) {
        case FrontendGameMode::standard:
            return scenario;
        case FrontendGameMode::regicide:
            configure_regicide(scenario);
            return scenario;
        case FrontendGameMode::death_match:
            configure_death_match(scenario);
            return scenario;
        case FrontendGameMode::learn_to_play:
            throw std::invalid_argument(
                "learn-to-play uses its authored scenario"
            );
    }
    return scenario;
}

Scenario make_learn_to_play_scenario(std::uint64_t seed) {
    RandomMapSettings settings{
        RandomMapKind::arabia, RandomMapSize::tiny, seed,
        Civilization::britons, Civilization::franks,
    };
    Scenario scenario = generate_random_map(settings);
    scenario.match_rules.conquest_enabled = true;
    scenario.match_rules.wonder_enabled = false;
    scenario.match_rules.relic_enabled = false;
    scenario.red_economy = {0, 0, 0, 0};
    scenario.objectives = {
        {1, Player::blue, true, false,
         "Select a Villager and gather food."},
        {2, Player::blue, true, false,
         "Use Villagers to build and grow your settlement."},
        {3, Player::blue, true, false,
         "Train an army and defeat the enemy."},
    };
    const auto red_town_center = std::ranges::find_if(
        scenario.buildings,
        [](const BuildingPlacement& building) {
            return building.kind == BuildingKind::town_center &&
                building.owner == Player::red;
        }
    );
    const EntityId red_town_center_id = static_cast<EntityId>(
        scenario.units.size() +
        std::distance(scenario.buildings.begin(), red_town_center) + 1
    );
    scenario.triggers.clear();
    scenario.triggers.emplace_back(
        1, 3, true, false,
        std::vector<std::string>{"elapsed_ticks >= 1"},
        std::vector<std::string>{
            "message player=blue ticks=240 text=\"Welcome. Select a "
            "Villager, then gather food.\""
        }
    );
    scenario.triggers.emplace_back(
        2, 2, true, false,
        std::vector<std::string>{"resource blue food >= 300"},
        std::vector<std::string>{
            "complete_objective 1",
            "message player=blue ticks=240 text=\"Good. Build Houses and "
            "an army.\""
        }
    );
    scenario.triggers.emplace_back(
        3, 1, true, false,
        std::vector<std::string>{
            "building_destroyed " + std::to_string(red_town_center_id)
        },
        std::vector<std::string>{
            "complete_objective 3", "victory blue"
        }
    );
    return scenario;
}

const ZoneServiceContract& zone_service_contract() {
    // Original Readme names zone.com and launch-from-Zone behavior. MSN
    // Gaming Zone retired its CD-ROM matchmaking service; no protocol or
    // endpoint remains for this executable to connect to.
    static constexpr ZoneServiceContract contract{
        "MSN Gaming Zone",
        "http://www.zone.com/",
        false,
        "Original MSN Gaming Zone matchmaking is retired.",
        "Use Multiplayer for supported direct host/join play.",
    };
    return contract;
}

}  // namespace aoe

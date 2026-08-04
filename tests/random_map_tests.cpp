#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "aoe/format_versions.hpp"
#include "aoe/computer_player.hpp"
#include "aoe/multiplayer.hpp"
#include "aoe/random_map.hpp"
#include "aoe/save_game.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

aoe::TilePosition town_center(
    const aoe::Scenario& scenario, aoe::Player player
) {
    for (const auto& building : scenario.buildings) {
        if (building.kind == aoe::BuildingKind::town_center &&
            building.owner == player) {
            return building.position;
        }
    }
    throw std::runtime_error("town center missing");
}

int nearby_resource(
    const aoe::Scenario& scenario,
    aoe::TilePosition center,
    aoe::Terrain terrain
) {
    int total{};
    for (int y = center.y - 10; y <= center.y + 10; ++y) {
        for (int x = center.x - 10; x <= center.x + 10; ++x) {
            const aoe::TilePosition tile{x, y};
            if (scenario.map.contains(tile) &&
                scenario.map.terrain_at(tile) == terrain) {
                total += scenario.map.resource_amount_at(tile);
            }
        }
    }
    return total;
}

void size_presets_match_original_tile_counts() {
    constexpr std::pair<aoe::RandomMapSize, int> expected[]{
        {aoe::RandomMapSize::tiny, 120},
        {aoe::RandomMapSize::small, 144},
        {aoe::RandomMapSize::medium, 168},
        {aoe::RandomMapSize::normal, 200},
        {aoe::RandomMapSize::large, 220},
        {aoe::RandomMapSize::giant, 240},
    };
    for (const auto& [size, dimension] : expected) {
        require(
            aoe::random_map_dimension(size) == dimension,
            "map size preset drifted from the original tile count"
        );
    }
    const aoe::RandomMapSettings defaults;
    require(
        defaults.size == aoe::RandomMapSize::normal,
        "random-map default is not Normal"
    );
    require(
        aoe::random_map_dimension(defaults.size) == 200,
        "random-map default does not resolve to 200"
    );
    require(aoe::random_map_dimension(
        aoe::RandomMapKind::islands, aoe::RandomMapSize::tiny
    ) == 144, "Islands did not bump Tiny one size step");
    require(aoe::random_map_dimension(
        aoe::RandomMapKind::islands, aoe::RandomMapSize::giant
    ) == 255, "Islands Giant did not reach internal 255-tile extent");
    require(aoe::random_map_dimension(
        aoe::RandomMapKind::arabia, aoe::RandomMapSize::giant
    ) == 240, "Arabia incorrectly applied map-family size bump");
}

void feature_density_survives_the_largest_presets() {
    // Blob counts grew with the dimension but the radii were fixed, so the
    // large presets came out nearly featureless: arabia dropped from 6.8%
    // non-grass at 120 tiles to 2.8% at 255, and raised ground from 8.8%
    // to 1.7%. Radii now scale, so density has to hold across the ladder.
    constexpr aoe::RandomMapKind kinds[]{
        aoe::RandomMapKind::arabia,
        aoe::RandomMapKind::black_forest,
        aoe::RandomMapKind::islands,
        aoe::RandomMapKind::rivers,
    };
    struct Density {
        double features{};
        double raised{};
    };
    const auto measure = [](aoe::RandomMapKind kind, aoe::RandomMapSize size) {
        Density total;
        constexpr int samples = 3;
        for (int sample = 0; sample < samples; ++sample) {
            aoe::RandomMapSettings settings;
            settings.kind = kind;
            settings.size = size;
            settings.seed = 11 + static_cast<std::uint64_t>(sample) * 97;
            const aoe::Scenario scenario = aoe::generate_random_map(settings);
            long long features{};
            long long raised{};
            for (int y = 0; y < scenario.map.height(); ++y) {
                for (int x = 0; x < scenario.map.width(); ++x) {
                    if (scenario.map.terrain_at({x, y}) !=
                        aoe::Terrain::grass) {
                        ++features;
                    }
                    if (scenario.map.elevation_at({x, y}) > 0) ++raised;
                }
            }
            const double area = static_cast<double>(scenario.map.width()) *
                scenario.map.height();
            total.features += static_cast<double>(features) / area / samples;
            total.raised += static_cast<double>(raised) / area / samples;
        }
        return total;
    };
    for (aoe::RandomMapKind kind : kinds) {
        const Density smallest = measure(kind, aoe::RandomMapSize::tiny);
        const Density largest = measure(kind, aoe::RandomMapSize::giant);
        require(
            largest.features >= smallest.features * 0.6,
            "Giant preset lost terrain feature density"
        );
        require(
            largest.raised >= smallest.raised * 0.6,
            "Giant preset lost elevation density"
        );
    }
}

void generated_maps_are_deterministic_and_balanced() {
    constexpr aoe::RandomMapKind kinds[]{
        aoe::RandomMapKind::arabia,
        aoe::RandomMapKind::black_forest,
        aoe::RandomMapKind::islands,
        aoe::RandomMapKind::rivers,
    };
    constexpr aoe::RandomMapSize sizes[]{
        aoe::RandomMapSize::tiny,
        aoe::RandomMapSize::small,
        aoe::RandomMapSize::medium,
        aoe::RandomMapSize::normal,
        aoe::RandomMapSize::large,
        aoe::RandomMapSize::giant,
    };
    for (aoe::RandomMapKind kind : kinds) {
        for (aoe::RandomMapSize size : sizes) {
            for (std::uint64_t seed = 0; seed < 24; ++seed) {
                const aoe::RandomMapSettings settings{kind, size, seed};
                const aoe::Scenario first =
                    aoe::generate_random_map(settings);
                const aoe::Scenario second =
                    aoe::generate_random_map(settings);
                require(
                    aoe::validate_random_map(first, kind).valid,
                    "generated map failed validation"
                );
                require(
                    aoe::random_map_hash(first) ==
                        aoe::random_map_hash(second),
                    "same seed produced different state"
                );
                require(
                    first.map.width() ==
                        aoe::random_map_dimension(kind, size) &&
                    first.map.height() ==
                        aoe::random_map_dimension(kind, size),
                    "map size preset mismatch"
                );
                const aoe::TilePosition blue =
                    town_center(first, aoe::Player::blue);
                const aoe::TilePosition red =
                    town_center(first, aoe::Player::red);
                for (aoe::Terrain terrain : {
                         aoe::Terrain::berry_bush,
                         aoe::Terrain::gold_mine,
                         aoe::Terrain::stone_mine,
                     }) {
                    require(
                        nearby_resource(first, blue, terrain) ==
                        nearby_resource(first, red, terrain),
                        "player resource starts are unequal"
                    );
                }
                if (seed == 0) {
                    (void)aoe::create_simulation(first);
                }
            }
        }
    }
}

void generated_map_writes_current_scenario() {
    const aoe::Scenario scenario = aoe::generate_random_map({
        aoe::RandomMapKind::rivers,
        aoe::RandomMapSize::tiny,
        0x123456789abcdef0ULL,
        aoe::Civilization::britons,
        aoe::Civilization::franks,
    });
    require(
        scenario.blue_civilization == aoe::Civilization::britons &&
        scenario.red_civilization == aoe::Civilization::franks,
        "generated map lost civilization selection"
    );
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-random-map-current.scenario";
    aoe::save_scenario(scenario, path);
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    std::filesystem::remove(path);
    require(
        magic == "AOE-ARCHAEOLOGY-SCENARIO" &&
        version == aoe::reconstruction_scenario_version,
        "generated map did not write current scenario version"
    );
}

void generated_civilization_starts_are_exact_and_durable() {
    const aoe::RandomMapSettings settings{
        aoe::RandomMapKind::arabia,
        aoe::RandomMapSize::tiny,
        0x4349565354415254ULL,
        aoe::Civilization::chinese,
        aoe::Civilization::mayans,
    };
    const aoe::Scenario first = aoe::generate_random_map(settings);
    const aoe::Scenario second = aoe::generate_random_map(settings);
    require(
        aoe::random_map_hash(first) == aoe::random_map_hash(second),
        "civilization start package is not deterministic"
    );
    require(
        first.blue_economy.wood == 50 &&
        first.blue_economy.food == 0,
        "Chinese starting resource offsets are wrong"
    );
    require(
        first.red_economy.wood == 100 &&
        first.red_economy.food == 150,
        "Mayan starting resource offsets are wrong"
    );
    const auto villager_count = [&first](aoe::Player player) {
        return std::ranges::count_if(
            first.units, [player](const aoe::UnitPlacement& unit) {
                return unit.owner == player &&
                    unit.kind == aoe::UnitKind::villager;
            }
        );
    };
    require(
        villager_count(aoe::Player::blue) == 6,
        "Chinese must start with six Villagers"
    );
    require(
        villager_count(aoe::Player::red) == 4,
        "Mayans must start with four Villagers"
    );
    require(
        aoe::validate_random_map(
            first, aoe::RandomMapKind::arabia
        ).valid,
        "civilization start package failed map validation"
    );

    aoe::Simulation simulation = aoe::create_simulation(first);
    require(
        simulation.population(aoe::Player::blue) == 7 &&
        simulation.population_capacity(aoe::Player::blue) == 5,
        "Chinese generated start population contract is wrong"
    );
    const auto town_center_id = simulation.buildings().front().id;
    require(
        !simulation.queue_unit_at(
            town_center_id, aoe::UnitKind::villager
        ),
        "over-cap Chinese start must not queue another Villager"
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {2, 2}
    );
    auto funded = simulation.economy(aoe::Player::blue);
    funded.food = 100;
    simulation.replace_state(
        simulation.units(), simulation.buildings(), funded,
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(
        simulation.queue_unit_at(
            town_center_id, aoe::UnitKind::villager
        ),
        "housing must safely unlock Chinese Villager production"
    );

    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-generated-civilization-start.scenario";
    aoe::save_scenario(first, scenario_path);
    const auto loaded_scenario = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(
        loaded_scenario.blue_civilization ==
            aoe::Civilization::chinese &&
        loaded_scenario.red_civilization ==
            aoe::Civilization::mayans &&
        loaded_scenario.blue_economy.food == 0 &&
        loaded_scenario.units.size() == first.units.size(),
        "generated civilization package did not survive scenario save"
    );

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-generated-civilization-start.save";
    aoe::save_game(simulation, save_path);
    const auto loaded_game = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded_game.civilization(aoe::Player::blue) ==
            aoe::Civilization::chinese &&
        loaded_game.economy(aoe::Player::blue).food == 50 &&
        loaded_game.population(aoe::Player::blue) == 7,
        "generated civilization package did not survive game save"
    );

    aoe::Scenario authored(16, 12);
    authored.blue_civilization = aoe::Civilization::chinese;
    authored.units.push_back({
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    });
    const auto authored_simulation = aoe::create_simulation(authored);
    require(
        authored_simulation.population(aoe::Player::blue) == 1 &&
        authored_simulation.economy(aoe::Player::blue).wood == 100 &&
        authored_simulation.economy(aoe::Player::blue).food == 200,
        "authored Scenario was incorrectly given a random-map package"
    );
}

void computer_players_make_deterministic_random_map_progress() {
    constexpr std::array kinds{
        aoe::RandomMapKind::arabia,
        aoe::RandomMapKind::black_forest,
        aoe::RandomMapKind::islands,
        aoe::RandomMapKind::rivers,
    };
    constexpr std::array sizes{
        aoe::RandomMapSize::tiny,
        aoe::RandomMapSize::small,
        aoe::RandomMapSize::medium,
        aoe::RandomMapSize::normal,
        aoe::RandomMapSize::large,
        aoe::RandomMapSize::giant,
    };
    constexpr std::array civilizations{
        aoe::Civilization::britons,
        aoe::Civilization::persians,
        aoe::Civilization::mayans,
        aoe::Civilization::vikings,
    };
    constexpr std::array difficulties{
        aoe::ComputerDifficulty::easy,
        aoe::ComputerDifficulty::standard,
        aoe::ComputerDifficulty::hard,
        aoe::ComputerDifficulty::expert,
    };
    struct Result {
        std::uint64_t tick{};
        aoe::MatchOutcome outcome{};
        aoe::Age blue_age{};
        aoe::Age red_age{};
        int blue_score{};
        int red_score{};
        int blue_population{};
        int red_population{};
        aoe::ComputerObjective blue_objective{};
        aoe::ComputerObjective red_objective{};
        auto operator<=>(const Result&) const = default;
    };
    for (std::size_t kind_index = 0;
         kind_index < kinds.size(); ++kind_index) {
            const std::size_t size_index = kind_index;
            const auto run = [&] {
                const auto blue_civilization =
                    civilizations[kind_index];
                const auto red_civilization =
                    civilizations[(kind_index + 1) %
                                  civilizations.size()];
                auto scenario = aoe::generate_random_map({
                    kinds[kind_index],
                    sizes[size_index],
                    0x4149535452455353ULL +
                        kind_index * 17 + size_index,
                    blue_civilization,
                    red_civilization,
                });
                aoe::MatchRules rules;
                const std::size_t rule_variant =
                    kind_index % 3;
                rules.conquest_enabled = rule_variant == 0;
                rules.wonder_enabled = rule_variant == 1;
                rules.relic_enabled = rule_variant == 2;
                rules.wonder_countdown_ticks = 200;
                rules.relic_countdown_ticks = 200;
                rules.relics_required = 1;
                scenario.match_rules = rules;
                aoe::Simulation simulation =
                    aoe::create_simulation(scenario);
                aoe::ComputerPlayer blue(
                    aoe::Player::blue,
                    difficulties[size_index]
                );
                aoe::ComputerPlayer red(
                    aoe::Player::red,
                    difficulties[(size_index + 1) %
                                 difficulties.size()]
                );
                for (int tick = 0;
                     tick < 1500 &&
                     simulation.outcome() ==
                         aoe::MatchOutcome::ongoing;
                     ++tick) {
                    simulation.update();
                    if (tick % 10 == 0) {
                        blue.update(simulation);
                        red.update(simulation);
                    }
                }
                return Result{
                    simulation.tick_number(),
                    simulation.outcome(),
                    simulation.age(aoe::Player::blue),
                    simulation.age(aoe::Player::red),
                    simulation.score(aoe::Player::blue),
                    simulation.score(aoe::Player::red),
                    simulation.population(aoe::Player::blue),
                    simulation.population(aoe::Player::red),
                    blue.status().objective,
                    red.status().objective,
                };
            };
            const Result first = run();
            if (kind_index == 0 && size_index == 0) {
                const Result second = run();
                require(
                    first == second,
                    "random-map ComputerPlayer run was nondeterministic"
                );
            }
            require(
                first.outcome != aoe::MatchOutcome::ongoing ||
                first.blue_age >= aoe::Age::feudal ||
                first.red_age >= aoe::Age::feudal ||
                first.blue_score > 100 ||
                first.red_score > 100,
                "both ComputerPlayers made no strategic progress"
            );
            require(
                first.outcome != aoe::MatchOutcome::ongoing ||
                (first.blue_population > 0 &&
                 first.red_population > 0),
                "ongoing AI match lost an entire player population"
            );
    }
}

void seed_scouts_remain_in_reachable_land_component() {
    for (const std::uint64_t seed : {1ULL, 2ULL, 99ULL}) {
        aoe::RandomMapSettings settings;
        settings.kind = aoe::RandomMapKind::arabia;
        settings.size = aoe::RandomMapSize::giant;
        settings.seed = seed;
        aoe::Simulation simulation = aoe::create_simulation(
            aoe::generate_random_map(settings)
        );
        aoe::ComputerPlayer blue(aoe::Player::blue);
        aoe::ComputerPlayer red(aoe::Player::red);
        int red_scout_boundary_streak{};
        int longest_red_scout_boundary_streak{};
        for (int tick = 0; tick < 1000; ++tick) {
            simulation.update();
            if (tick % 10 == 0) {
                blue.update(simulation);
                red.update(simulation);
            }
            const auto scout = std::ranges::find_if(
                simulation.units(), [](const aoe::Unit& unit) {
                    return unit.owner == aoe::Player::red &&
                        unit.kind == aoe::UnitKind::scout_cavalry;
                }
            );
            if (scout != simulation.units().end() &&
                (scout->position.x == 0 || scout->position.y == 0 ||
                 scout->position.x == simulation.map().width() - 1 ||
                 scout->position.y == simulation.map().height() - 1)) {
                ++red_scout_boundary_streak;
                longest_red_scout_boundary_streak = std::max(
                    longest_red_scout_boundary_streak,
                    red_scout_boundary_streak
                );
            } else {
                red_scout_boundary_streak = 0;
            }
        }
        if (seed == 99) {
            require(
                longest_red_scout_boundary_streak < 250,
                "seed 99 red scout remained fixed to map boundary"
            );
        }

        for (const aoe::Player player : {
                 aoe::Player::blue, aoe::Player::red,
             }) {
            const aoe::TilePosition start = [&] {
                for (const aoe::Building& building : simulation.buildings()) {
                    if (building.owner == player &&
                        building.kind == aoe::BuildingKind::town_center) {
                        return building.position;
                    }
                }
                throw std::runtime_error("seed test town center missing");
            }();
            const aoe::GameMap& map = simulation.map();
            const auto index = [&map](aoe::TilePosition position) {
                return static_cast<std::size_t>(
                    position.y * map.width() + position.x
                );
            };
            std::vector<bool> reachable(
                static_cast<std::size_t>(map.width() * map.height()), false
            );
            std::deque<aoe::TilePosition> queue{start};
            reachable[index(start)] = true;
            constexpr std::array<aoe::TilePosition, 4> directions{{
                {1, 0}, {-1, 0}, {0, 1}, {0, -1},
            }};
            while (!queue.empty()) {
                const aoe::TilePosition current = queue.front();
                queue.pop_front();
                for (const aoe::TilePosition direction : directions) {
                    const aoe::TilePosition next{
                        current.x + direction.x,
                        current.y + direction.y,
                    };
                    if (!map.traversable(current, next) ||
                        reachable[index(next)]) continue;
                    reachable[index(next)] = true;
                    queue.push_back(next);
                }
            }
            for (const aoe::Unit& unit : simulation.units()) {
                if (unit.owner == player &&
                    unit.kind == aoe::UnitKind::scout_cavalry) {
                    require(
                        unit.last_move_tick > 0,
                        "seed scout never made movement progress"
                    );
                    require(
                        reachable[index(unit.position)],
                        "seed scout escaped reachable land component"
                    );
                }
            }
        }
    }
}

}  // namespace

int main() {
    try {
        size_presets_match_original_tile_counts();
        feature_density_survives_the_largest_presets();
        generated_maps_are_deterministic_and_balanced();
        generated_map_writes_current_scenario();
        generated_civilization_starts_are_exact_and_durable();
        computer_players_make_deterministic_random_map_progress();
        seed_scouts_remain_in_reachable_land_component();
        std::cout << "All random map tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

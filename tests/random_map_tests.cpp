#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
        aoe::RandomMapSize::large,
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
                        aoe::random_map_dimension(size) &&
                    first.map.height() ==
                        aoe::random_map_dimension(size),
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
        aoe::RandomMapSize::large,
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

}  // namespace

int main() {
    try {
        generated_maps_are_deterministic_and_balanced();
        generated_map_writes_current_scenario();
        generated_civilization_starts_are_exact_and_durable();
        computer_players_make_deterministic_random_map_progress();
        std::cout << "All random map tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

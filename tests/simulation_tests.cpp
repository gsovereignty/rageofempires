#include <cstdlib>
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>
#include <tuple>

#include "aoe/computer_player.hpp"
#include "aoe/animation_contract.hpp"
#include "aoe/campaign.hpp"
#include "aoe/game_command.hpp"
#include "aoe/game_rules.hpp"
#include "aoe/multiplayer.hpp"
#include "aoe/multiplayer_checkpoint.hpp"
#include "aoe/multiplayer_transport.hpp"
#include "aoe/save_game.hpp"
#include "aoe/scenario.hpp"
#include "aoe/random_map.hpp"
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void require(
    bool condition,
    const std::source_location location = std::source_location::current()
) {
    if (!condition) {
        std::cerr << "Requirement failed at " << location.file_name() << ':'
                  << location.line() << '\n';
        std::abort();
    }
}

void prepare_dock_foundation(
    aoe::GameMap& map,
    aoe::TilePosition origin
) {
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            map.set_terrain(
                {origin.x + x, origin.y + y}, aoe::Terrain::water
            );
        }
    }
    map.set_terrain(
        {origin.x + 1, origin.y - 1}, aoe::Terrain::ice2
    );
}

void executable_conversion_arithmetic_is_exact() {
    const auto zero = aoe::evaluate_conversion_check(
        0, 0.0F, 25, 5.0F, 4.0F, 8.0F
    );
    require(zero.scaled_roll == 0);
    require(zero.threshold == 25);
    require(zero.succeeds);

    const auto maximum_random = aoe::evaluate_conversion_check(
        32767, 0.0F, 99, 5.0F, 4.0F, 8.0F
    );
    require(maximum_random.scaled_roll == 100);
    require(!maximum_random.succeeds);
    require(aoe::evaluate_conversion_check(
        32766, 0.0F, 99, 5.0F, 4.0F, 8.0F
    ).scaled_roll == 99);

    const auto before_minimum = aoe::evaluate_conversion_check(
        0, 0.0F, 1000, 3.999F, 4.0F, 8.0F
    );
    require(before_minimum.threshold == -1000);
    require(!before_minimum.succeeds);
    const auto at_maximum = aoe::evaluate_conversion_check(
        32767, 20.0F, -1000, 8.0F, 4.0F, 8.0F
    );
    require(at_maximum.threshold == 1000);
    require(at_maximum.scaled_roll == 2000);
    require(!at_maximum.succeeds);

    const int original_rounding = std::fegetround();
    require(std::fesetround(FE_TONEAREST) == 0);
    require(aoe::evaluate_conversion_check(
        328, 2.5F, 100, 5.0F, 4.0F, 8.0F
    ).scaled_roll == 2);
    require(aoe::evaluate_conversion_check(
        984, 2.5F, 100, 5.0F, 4.0F, 8.0F
    ).scaled_roll == 8);
    require(std::fesetround(FE_DOWNWARD) == 0);
    require(aoe::evaluate_conversion_check(
        984, 2.5F, 100, 5.0F, 4.0F, 8.0F
    ).scaled_roll == 7);
    require(std::fesetround(original_rounding) == 0);

    bool rejected = false;
    try {
        (void)aoe::evaluate_conversion_check(
            32768, 0.0F, 25, 5.0F, 4.0F, 8.0F
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected);
    rejected = false;
    try {
        (void)aoe::evaluate_conversion_check(
            0, std::nanf(""), 25, 5.0F, 4.0F, 8.0F
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected);
}

void commercial_conversion_stream_and_schedule_are_exact() {
    struct Trace {
        int success_tick{};
        int next_random{};
    };
    const auto trace = [](
        std::uint32_t seed,
        bool faith,
        bool teuton_team,
        aoe::UnitKind target_kind
    ) {
        aoe::Simulation simulation(aoe::GameMap(18, 8));
        simulation.seed_commercial_random(seed);
        const auto monk = simulation.add_unit(
            aoe::UnitKind::monk, aoe::Player::blue, {3, 3}
        );
        const auto target = simulation.add_unit(
            target_kind, aoe::Player::red, {7, 3}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {14, 4}
        );
        if (faith) {
            simulation.replace_technologies(
                aoe::Player::red, {aoe::Technology::faith}
            );
        }
        if (teuton_team) {
            simulation.replace_civilizations(
                aoe::Civilization::generic,
                aoe::Civilization::teutons
            );
        }
        require(simulation.command_convert(monk, target));
        int success_tick{};
        for (int tick = 1; tick <= 20; ++tick) {
            simulation.update();
            const auto converted = std::ranges::find(
                simulation.units(), target, &aoe::Unit::id
            );
            if (converted != simulation.units().end() &&
                converted->owner == aoe::Player::blue) {
                success_tick = tick;
                break;
            }
        }
        require(success_tick != 0);
        return Trace{success_tick, simulation.consume_commercial_random()};
    };

    // Seed zero produces rolls 0,23,64,7: first three still fail because
    // elapsed time is below four; inclusive ordinary chance succeeds at four.
    const Trace forced_minimum = trace(
        0, false, false, aoe::UnitKind::villager
    );
    require(forced_minimum.success_tick == 4);
    require(forced_minimum.next_random == 8855);

    // Default CRT seed reaches no ordinary success before forced maximum 10.
    const Trace forced_maximum = trace(
        1, false, false, aoe::UnitKind::villager
    );
    require(forced_maximum.success_tick == 10);
    require(forced_maximum.next_random == 5705);

    // Commercial ID 448 adds the recovered special-unit resistance eight.
    const Trace resistant = trace(
        1, false, false, aoe::UnitKind::scout_cavalry
    );
    require(resistant.success_tick == 10);
    require(resistant.next_random == 5705);

    // Faith adds resources 77/178/179 = 3/2/4.
    const Trace faith = trace(
        1, true, false, aoe::UnitKind::villager
    );
    require(faith.success_tick == 14);
    require(faith.next_random == 9961);

    // Teuton team effect 404 adds resources 77/178/179 = 2/1/2.
    const Trace teuton = trace(
        1, false, true, aoe::UnitKind::villager
    );
    require(teuton.success_tick == 12);
    require(teuton.next_random == 23281);
}

void unit_moves_deterministically() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.command_selected({5, 7}));

    simulation.update();
    simulation.update();
    simulation.update();
    simulation.update();
    simulation.update();

    require(simulation.units().front().position == aoe::TilePosition(5, 7));
}

void logical_facing_persists_and_actions_turn_toward_targets() {
    aoe::Simulation simulation(aoe::GameMap(10, 10));
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {4, 4}
    );
    const aoe::EntityId target = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {5, 3}
    );
    require(simulation.set_unit_stance(target, aoe::UnitStance::passive));
    require(simulation.command_unit(attacker, {5, 4}));
    for (int tick = 0;
         tick < 8 && simulation.units().front().position !=
             aoe::TilePosition{5, 4}; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().position == aoe::TilePosition{5, 4});
    require(simulation.units().front().facing == 7);
    require(simulation.stop_unit(attacker));
    require(simulation.units().front().previous_position ==
            simulation.units().front().position);
    require(simulation.units().front().facing == 7);

    require(simulation.command_unit(attacker, {5, 3}));
    simulation.update();
    const auto expected = aoe::animation::logical_direction(
        simulation.units().front().position, {5, 3}, 8
    );
    require(expected.has_value());
    require(simulation.units().front().facing == *expected);

    aoe::GameMap water(8, 8);
    water.set_terrain({4, 4}, aoe::Terrain::water);
    water.set_terrain({5, 4}, aoe::Terrain::water);
    aoe::Simulation naval(std::move(water));
    const aoe::EntityId cog = naval.add_unit(
        aoe::UnitKind::trade_cog, aoe::Player::blue, {4, 4}
    );
    require(naval.command_unit(cog, {5, 4}));
    for (int tick = 0;
         tick < 8 && naval.units().front().position !=
             aoe::TilePosition{5, 4}; ++tick) {
        naval.update();
    }
    require(naval.units().front().position == aoe::TilePosition{5, 4});
    require(naval.units().front().facing == 14);
}

void land_route_detours_around_cliff_and_makes_progress() {
    aoe::GameMap map(5, 3);
    map.set_elevation({1, 1}, 2);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {4, 2}
    );

    require(simulation.command_unit(villager, {4, 1}));
    for (int tick = 0; tick < 20; ++tick) simulation.update();
    require(simulation.units().front().position == aoe::TilePosition(4, 1));
    require(!simulation.units().front().moving);
}

void presentation_elevation_state_initializes_and_replaces_safely() {
    aoe::Simulation simulation(aoe::GameMap(8, 4));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {1, 1}
    );
    const aoe::Unit& added = simulation.units().front();
    require(
        simulation.render_previous_elevation_position(added) ==
            aoe::TilePosition(1, 1)
    );
    require(
        simulation.render_current_elevation_position(added) ==
            aoe::TilePosition(1, 1)
    );

    std::vector<aoe::Unit> restored = simulation.units();
    restored.front().position = {4, 2};
    restored.front().previous_position = {3, 2};
    restored.front().render_subtile_initialized = false;
    simulation.replace_state(
        std::move(restored), {}, {}, {}, 12
    );
    const aoe::Unit& replaced = simulation.units().front();
    require(replaced.id == scout);
    require(
        simulation.render_previous_elevation_position(replaced) ==
            aoe::TilePosition(4, 2)
    );
    require(
        simulation.render_current_elevation_position(replaced) ==
            aoe::TilePosition(4, 2)
    );

    aoe::Simulation deletion(aoe::GameMap(8, 4));
    const aoe::EntityId deleted_id = deletion.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    deletion.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 2}
    );
    aoe::Unit deleted = deletion.units().front();
    require(deletion.delete_unit(deleted_id));
    deleted.position = {6, 3};
    require(
        deletion.render_previous_elevation_position(deleted) ==
            aoe::TilePosition(6, 3)
    );
    require(
        deletion.render_current_elevation_position(deleted) ==
            aoe::TilePosition(6, 3)
    );
}

void replay_loader_rejects_malformed_and_invalid_enums() {
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-corrupt-v61.replay";
    const auto rejected = [&path](const std::string& record) {
        {
            std::ofstream output(path);
            output << "AOE-ARCHAEOLOGY-REPLAY 61\n" << record << '\n';
        }
        bool threw = false;
        try {
            (void)aoe::load_replay(path);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        require(threw);
    };
    rejected("resign");
    rejected("resign 0 99");
    rejected("civilization 0 0 99");
    rejected("diplomacy 0 0 1 99");
    rejected("buy-resource 0 0 99");
    rejected("stance 0 1 99");
    rejected("queue 0 1 999");
    rejected("research 0 1 999");
    rejected("move 0 1 2");
    rejected("resign 0 0 trailing");

    aoe::Simulation legacy(aoe::GameMap(8, 6));
    const aoe::EntityId farm = legacy.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {2, 2}
    );
    legacy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 4}
    );
    std::vector<aoe::Building> farms = legacy.buildings();
    farms.front().resource_amount = 0;
    legacy.replace_state(
        legacy.units(), std::move(farms), {60, 0, 0, 0},
        legacy.economy(aoe::Player::red), 0
    );
    {
        std::ofstream output(path);
        output << "AOE-ARCHAEOLOGY-REPLAY 61\n"
               << "reseed 0 " << farm << '\n';
    }
    aoe::Replay old_replay = aoe::load_replay(path);
    old_replay.apply_current_tick(legacy);
    require(legacy.buildings().front().resource_amount == 175);
    require(legacy.economy(aoe::Player::blue).wood == 0);
    std::filesystem::remove(path);
}

void save_loader_rejects_truncated_production_queue() {
    aoe::Simulation simulation(aoe::GameMap(8, 6));
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {1, 1}
    );
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-truncated-queue-v98.save";
    aoe::save_game(simulation, path);
    std::ifstream input(path);
    std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    const std::size_t building = contents.rfind("\nbuilding ");
    require(building != std::string::npos);
    const std::size_t line_end = contents.find('\n', building + 1);
    require(line_end != std::string::npos);
    const std::size_t facing_field = contents.rfind(' ', line_end);
    require(facing_field != std::string::npos);
    const std::size_t queue_count = contents.rfind(" 0", facing_field - 1);
    require(queue_count != std::string::npos && queue_count > building);
    contents.replace(queue_count, 2, " 3");
    contents.erase(line_end + 1);
    {
        std::ofstream output(path);
        output << contents;
    }
    bool threw = false;
    try {
        (void)aoe::load_game(path);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::filesystem::remove(path);
    require(threw);
}

void save_loader_rejects_invalid_loaded_placements() {
    const auto rejected_at = [](
        const aoe::Simulation& simulation,
        aoe::TilePosition position,
        std::string_view filename
    ) {
        const auto path = std::filesystem::temp_directory_path() / filename;
        aoe::save_game(simulation, path);
        std::ifstream input(path);
        std::ostringstream rewritten;
        std::string line;
        bool changed = false;
        while (std::getline(input, line)) {
            if (!changed && line.starts_with("unit ")) {
                std::istringstream fields(line);
                std::vector<std::string> values;
                std::string value;
                while (fields >> value) values.push_back(value);
                require(values.size() > 5);
                values[4] = std::to_string(position.x);
                values[5] = std::to_string(position.y);
                for (std::size_t index = 0; index < values.size(); ++index) {
                    if (index != 0) rewritten << ' ';
                    rewritten << values[index];
                }
                rewritten << '\n';
                changed = true;
            } else {
                rewritten << line << '\n';
            }
        }
        require(changed);
        {
            std::ofstream output(path);
            output << rewritten.str();
        }
        bool threw = false;
        try {
            (void)aoe::load_game(path);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        std::filesystem::remove(path);
        require(threw);
    };

    aoe::GameMap land_map(8, 6);
    land_map.set_terrain({4, 4}, aoe::Terrain::water);
    aoe::Simulation villager(std::move(land_map));
    villager.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    rejected_at(villager, {4, 4}, "aoe-villager-water.save");

    aoe::GameMap water_map(8, 6);
    water_map.set_terrain({1, 1}, aoe::Terrain::water);
    aoe::Simulation ship(std::move(water_map));
    ship.add_unit(aoe::UnitKind::galley, aoe::Player::blue, {1, 1});
    rejected_at(ship, {4, 4}, "aoe-ship-grass.save");

    aoe::Simulation overlap(aoe::GameMap(10, 8));
    overlap.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 0}
    );
    overlap.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    rejected_at(overlap, {4, 3}, "aoe-unit-building-overlap.save");
}

void loaded_resource_state_rejects_negative_and_over_cap_values() {
    const auto rejected = [](aoe::Simulation simulation) {
        bool threw = false;
        try {
            simulation.validate_loaded_state();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        require(threw);
    };

    aoe::Simulation animal(aoe::GameMap(8, 6));
    animal.add_unit(
        aoe::UnitKind::boar, aoe::Player::red, {3, 2}
    );
    animal.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {0, 0}
    );
    animal.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 4}
    );
    std::vector<aoe::Unit> excessive = animal.units();
    excessive.front().food_remaining = 341;
    animal.replace_state(
        std::move(excessive), animal.buildings(),
        animal.economy(aoe::Player::blue),
        animal.economy(aoe::Player::red), 0
    );
    rejected(animal);

    aoe::Simulation farm(aoe::GameMap(8, 6));
    farm.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {2, 2}
    );
    farm.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 4}
    );
    std::vector<aoe::Building> negative = farm.buildings();
    negative.front().resource_amount = -1;
    farm.replace_state(
        farm.units(), std::move(negative),
        farm.economy(aoe::Player::blue),
        farm.economy(aoe::Player::red), 0
    );
    rejected(farm);
}

void beach_and_shallows_follow_live_dat_restrictions() {
    aoe::GameMap map(8, 4);
    map.set_terrain({1, 1}, aoe::Terrain::beach);
    map.set_terrain({2, 1}, aoe::Terrain::shallows);
    map.set_terrain({3, 1}, aoe::Terrain::water);
    require(map.walkable({1, 1}));
    require(map.walkable({2, 1}));
    require(!map.walkable({3, 1}));
    require(map.sailable({1, 1}));
    require(map.sailable({2, 1}));
    require(map.sailable({3, 1}));
    require(!map.sailable({0, 1}));

    bool rejected_resource = false;
    try {
        map.set_resource_amount({1, 1}, 1);
    } catch (const std::invalid_argument&) {
        rejected_resource = true;
    }
    require(rejected_resource);

    aoe::Simulation movement(map);
    const aoe::EntityId villager = movement.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 1}
    );
    const aoe::EntityId galley = movement.add_unit(
        aoe::UnitKind::galley, aoe::Player::blue, {3, 1}
    );
    movement.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {7, 3}
    );
    require(movement.command_unit(villager, {2, 1}));
    for (int tick = 0; tick < 20; ++tick) movement.update();
    require(movement.units()[0].position == aoe::TilePosition(2, 1));
    require(movement.command_unit(villager, {0, 1}));
    for (int tick = 0; tick < 20; ++tick) movement.update();
    require(movement.command_unit(galley, {1, 1}));
    for (int tick = 0; tick < 20; ++tick) movement.update();
    require(movement.units()[1].position == aoe::TilePosition(1, 1));

    aoe::Simulation building(map);
    const aoe::EntityId builder = building.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 1}
    );
    require(!building.construct_building_at(
        builder, aoe::BuildingKind::house, {1, 1}
    ));
    const aoe::EntityId fishing_ship = building.add_unit(
        aoe::UnitKind::fishing_ship, aoe::Player::blue, {3, 1}
    );
    building.replace_ages(aoe::Age::castle, aoe::Age::dark);
    building.replace_technologies(
        aoe::Player::blue, {aoe::Technology::fish_trap_gate}
    );
    require(building.construct_building_at(
        fishing_ship, aoe::BuildingKind::fish_trap, {1, 1}
    ));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-beach-shallows.save";
    aoe::save_game(movement, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.map().terrain_at({1, 1}) == aoe::Terrain::beach);
    require(loaded.map().terrain_at({2, 1}) == aoe::Terrain::shallows);
    require(loaded.map().sailable({1, 1}));
    require(loaded.map().walkable({2, 1}));

    aoe::Scenario scenario(8, 4);
    scenario.map = map;
    const auto scenario_path = std::filesystem::temp_directory_path() /
        "aoe-beach-shallows.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded_scenario = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(
        loaded_scenario.map.terrain_at({1, 1}) == aoe::Terrain::beach
    );
    require(
        loaded_scenario.map.terrain_at({2, 1}) == aoe::Terrain::shallows
    );

    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{1, {1, 1}});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-beach-shallows.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first(map);
    aoe::Simulation second(map);
    first.add_unit(aoe::UnitKind::galley, aoe::Player::blue, {3, 1});
    second.add_unit(aoe::UnitKind::galley, aoe::Player::blue, {3, 1});
    first.add_unit(aoe::UnitKind::villager, aoe::Player::red, {7, 3});
    second.add_unit(aoe::UnitKind::villager, aoe::Player::red, {7, 3});
    for (int tick = 0; tick < 20; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().front().position == aoe::TilePosition(1, 1));
    require(first.units().front().position == second.units().front().position);
}

void cavalry_moves_faster_than_foot_units() {
    aoe::Simulation simulation(aoe::GameMap(12, 6));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 5}
    );
    require(simulation.command_unit(villager, {7, 1}));
    require(simulation.command_unit(scout, {7, 3}));
    for (int tick = 0; tick < 4; ++tick) {
        simulation.update();
    }
    require(simulation.units()[0].position == aoe::TilePosition(3, 1));
    require(simulation.units()[1].position == aoe::TilePosition(4, 3));
}

void blocked_cavalry_does_not_bank_movement_credit() {
    aoe::Simulation simulation(aoe::GameMap(8, 1));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 0}
    );
    require(simulation.command_unit(scout, {6, 0}));
    const int remainder_before =
        simulation.units().front().movement_speed_remainder;

    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 0}
    );
    simulation.update();

    const aoe::Unit& blocked = simulation.units().front();
    require(blocked.position == aoe::TilePosition(1, 0));
    require(!blocked.moving);
    require(
        blocked.movement_speed_remainder == remainder_before
    );
}

void save_preserves_movement_cooldown() {
    aoe::Simulation simulation(aoe::GameMap(8, 5));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 4}
    );
    require(simulation.command_unit(villager, {5, 1}));
    simulation.update();
    require(simulation.units().front().movement_cooldown == 1);
    require(
        simulation.units().front().previous_position ==
        aoe::TilePosition(1, 1)
    );
    require(simulation.units().front().last_move_tick == 1);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-movement-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.units().front().movement_cooldown == 1);
    require(
        loaded.units().front().previous_position ==
        aoe::TilePosition(1, 1)
    );
    require(loaded.units().front().last_move_tick == 1);
    loaded.update();
    require(loaded.units().front().position == aoe::TilePosition(2, 1));
    loaded.update();
    require(loaded.units().front().position == aoe::TilePosition(3, 1));
    require(
        loaded.units().front().previous_position ==
        aoe::TilePosition(2, 1)
    );
}

void wheelbarrow_adds_exact_persisted_villager_speed() {
    const auto make_simulation = [](bool wheelbarrow) {
        aoe::Simulation simulation(aoe::GameMap(30, 4));
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {1, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {29, 3}
        );
        if (wheelbarrow) {
            simulation.replace_technologies(
                aoe::Player::blue,
                {aoe::Technology::wheelbarrow}
            );
        }
        return simulation;
    };

    aoe::Simulation normal = make_simulation(false);
    aoe::Simulation boosted = make_simulation(true);
    require(normal.command_unit(1, {25, 1}));
    require(boosted.command_unit(1, {25, 1}));
    for (int tick = 0; tick < 9; ++tick) {
        normal.update();
        boosted.update();
    }
    require(boosted.unique_unit_movement_numerator(
        boosted.units().front()
    ) == 110);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-wheelbarrow-movement.save";
    aoe::save_game(boosted, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.units().front().movement_speed_remainder ==
        boosted.units().front().movement_speed_remainder
    );

    for (int tick = 9; tick < 20; ++tick) {
        normal.update();
        boosted.update();
        loaded.update();
    }
    require(normal.units().front().position == aoe::TilePosition(11, 1));
    require(boosted.units().front().position == aoe::TilePosition(12, 1));
    require(loaded.units().front().position == boosted.units().front().position);
    require(
        loaded.units().front().movement_speed_remainder ==
        boosted.units().front().movement_speed_remainder
    );

    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{1, {25, 1}});
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-wheelbarrow-movement.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first = make_simulation(true);
    aoe::Simulation second = make_simulation(true);
    for (int tick = 0; tick < 20; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().front().position == aoe::TilePosition(12, 1));
    require(
        first.units().front().position ==
        second.units().front().position
    );
    require(
        first.units().front().movement_speed_remainder ==
        second.units().front().movement_speed_remainder
    );
}

void area_selection_selects_only_owned_units() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    const aoe::EntityId first = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId second = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId enemy = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {3, 3}
    );

    require(simulation.select_units_in_area({5, 5}, {1, 1}, aoe::Player::blue));
    require(simulation.selected_units().size() == 2);
    require(simulation.is_unit_selected(first));
    require(simulation.is_unit_selected(second));
    require(simulation.selected_unit() == first);

    require(simulation.select_units(
        {first, 999999, enemy, first},
        aoe::Player::blue
    ));
    require(simulation.selected_units().size() == 1);
    require(simulation.selected_unit() == first);

    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {6, 6}
    );
    require(simulation.select_building_at({6, 6}, aoe::Player::blue));
    require(!simulation.select_units({}, aoe::Player::blue));
    require(!simulation.selected_unit());
    require(!simulation.selected_building());
}

void formations_allocate_unique_reachable_slots_around_obstacles() {
    aoe::GameMap map(12, 10);
    for (int y = 0; y < map.height(); ++y) {
        map.set_terrain({5, y}, aoe::Terrain::water);
    }
    aoe::Simulation simulation(std::move(map));
    std::vector<aoe::EntityId> selected;
    for (int y = 1; y <= 4; ++y) {
        selected.push_back(simulation.add_unit(
            aoe::UnitKind::militia,
            aoe::Player::blue,
            {1, y}
        ));
    }
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 8}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {10, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {3, 3}
    );

    const std::vector<aoe::TilePosition> destinations =
        simulation.formation_destinations(selected, {8, 4});
    require(destinations.size() == selected.size());
    std::vector<aoe::TilePosition> unique;
    for (std::size_t index = 0; index < destinations.size(); ++index) {
        const aoe::TilePosition destination = destinations[index];
        require(destination.x < 5);
        require(
            simulation.map().terrain_at(destination) ==
            aoe::Terrain::grass
        );
        require(destination != aoe::TilePosition(4, 8));
        require(
            std::ranges::find(unique, destination) == unique.end()
        );
        unique.push_back(destination);
        require(simulation.command_unit(
            selected[index],
            destination
        ));
    }
}

void idle_villagers_exclude_every_active_work_order() {
    aoe::GameMap map(12, 8);
    map.set_terrain({8, 4}, aoe::Terrain::forest);
    map.set_resource_amount({8, 4}, 40);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId gatherer = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {4, 6}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );

    require(
        simulation.idle_villagers(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({builder, gatherer})
    );
    require(simulation.command_unit(gatherer, {8, 4}));
    require(
        simulation.idle_villagers(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({builder})
    );
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::house,
        {2, 1}
    ));
    require(simulation.idle_villagers(aoe::Player::blue).empty());

    require(simulation.stop_unit(gatherer));
    require(
        simulation.idle_villagers(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({gatherer})
    );
    require(simulation.stop_unit(builder));
    require(
        simulation.idle_villagers(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({builder, gatherer})
    );
}

void idle_military_excludes_persistent_combat_orders() {
    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 4}
    );
    const aoe::EntityId mangonel = simulation.add_unit(
        aoe::UnitKind::mangonel,
        aoe::Player::blue,
        {1, 6}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {19, 9}
    );

    require(
        simulation.idle_military(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({knight, archer, mangonel})
    );
    require(simulation.command_attack_move(knight, {8, 2}));
    require(simulation.command_guard(archer, villager, false));
    require(simulation.command_attack_ground(mangonel, {10, 6}));
    require(simulation.idle_military(aoe::Player::blue).empty());

    require(simulation.stop_unit(knight));
    require(simulation.stop_unit(archer));
    require(simulation.stop_unit(mangonel));
    require(
        simulation.idle_military(aoe::Player::blue) ==
        std::vector<aoe::EntityId>({knight, archer, mangonel})
    );
}

void vision_reveals_and_remembers_explored_tiles() {
    aoe::Simulation simulation(aoe::GameMap(20, 20));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {18, 18}
    );

    require(simulation.is_visible(aoe::Player::blue, {2, 2}));
    require(simulation.is_explored(aoe::Player::blue, {2, 2}));
    require(!simulation.is_visible(aoe::Player::blue, {18, 18}));
    require(!simulation.is_explored(aoe::Player::blue, {18, 18}));

    require(simulation.command_unit(scout, {10, 2}));
    for (int tick = 0; tick < 16; ++tick) {
        simulation.update();
    }
    require(!simulation.is_visible(aoe::Player::blue, {2, 2}));
    require(simulation.is_explored(aoe::Player::blue, {2, 2}));
    require(simulation.is_visible(aoe::Player::blue, {10, 2}));
}

void enemy_attackers_reveal_per_victim_through_attack_action() {
    const auto blue = *aoe::PlayerSlotId::from_index(0);
    const auto red = *aoe::PlayerSlotId::from_index(1);
    const auto green = *aoe::PlayerSlotId::from_index(2);
    const auto team_one = *aoe::TeamId::numbered(1);
    const auto team_two = *aoe::TeamId::numbered(2);
    const auto roster = aoe::MatchRoster::create({
        {blue, true, team_one, false,
         {{"blue", aoe::RosterControllerKind::human}}},
        {red, true, team_two, false,
         {{"red", aoe::RosterControllerKind::human}}},
        {green, true, team_one, false,
         {{"green", aoe::RosterControllerKind::human}}},
    });
    require(roster.has_value());
    const auto diplomacy = aoe::RosterDiplomacy::create(
        *roster, {true, false}
    );
    require(diplomacy.has_value());

    aoe::Simulation simulation(aoe::GameMap(40, 16));
    simulation.replace_roster(*roster, *diplomacy);
    const aoe::EntityOwner blue_owner = aoe::entity_owner_from_slot(blue);
    const aoe::EntityOwner red_owner = aoe::entity_owner_from_slot(red);
    const aoe::EntityOwner green_owner = aoe::entity_owner_from_slot(green);
    const auto attacker_id = simulation.add_unit(
        aoe::UnitKind::mangonel, red_owner, {8, 7}
    );
    const auto victim_id = simulation.add_unit(
        aoe::UnitKind::knight, blue_owner, {15, 7}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, green_owner, {35, 13}
    );
    // Red reconnaissance sees target; neither victim nor ally sees attacker.
    simulation.add_building(
        aoe::BuildingKind::outpost, red_owner, {15, 9}
    );
    const auto attacker = [&simulation, attacker_id]() -> const aoe::Unit& {
        const auto found = std::ranges::find(
            simulation.units(), attacker_id, &aoe::Unit::id
        );
        require(found != simulation.units().end());
        return *found;
    };
    require(!simulation.is_visible(blue_owner, attacker().position));
    require(!simulation.is_unit_visible(blue_owner, attacker()));
    require(!simulation.is_unit_visible(green_owner, attacker()));
    require(simulation.command_unit(attacker_id, {15, 7}));
    for (int tick = 0;
         tick < 12 && simulation.projectiles().empty(); ++tick) {
        simulation.update();
    }
    require(!simulation.projectiles().empty());
    require(simulation.projectiles().front().source_entity_id == attacker_id);
    require(!simulation.is_visible(blue_owner, attacker().position));
    require(simulation.is_unit_visible(blue_owner, attacker()));
    require(!simulation.is_unit_visible(green_owner, attacker()));

    // Cartography shares victim's temporary unit reveal, without exposing
    // terrain at attacker position or granting every ally live state.
    auto green_state = simulation.player_state(green);
    green_state.technologies[static_cast<std::size_t>(
        aoe::Technology::cartography
    )] = true;
    simulation.replace_player_state(green, std::move(green_state));
    require(simulation.is_unit_visible(green_owner, attacker()));
    require(!simulation.is_visible(green_owner, attacker().position));
    const auto expiry = simulation.player_state(blue)
        .attack_reveal_expiries.at(attacker_id);
    require(expiry > simulation.tick_number());
    require(expiry - simulation.tick_number() >=
        static_cast<std::uint64_t>(
            simulation.projectiles().front().ticks_remaining
        ));
    aoe::Simulation repeated = simulation;
    for (int tick = 0; tick < 30 &&
         repeated.player_state(blue).attack_reveal_expiries.at(
             attacker_id
         ) <= expiry; ++tick) {
        repeated.update();
    }
    require(repeated.player_state(blue).attack_reveal_expiries.at(
        attacker_id
    ) > expiry);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-attacker-reveal.save";
    const std::string hash = aoe::deterministic_state_hash(simulation);
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(aoe::deterministic_state_hash(loaded) == hash);
    const auto loaded_attacker = std::ranges::find(
        loaded.units(), attacker_id, &aoe::Unit::id
    );
    require(loaded_attacker != loaded.units().end());
    require(loaded.is_unit_visible(blue_owner, *loaded_attacker));
    require(loaded.is_unit_visible(green_owner, *loaded_attacker));
    require(loaded.stop_unit(attacker_id));
    auto expiring_blue = loaded.player_state(blue);
    expiring_blue.attack_reveal_expiries[attacker_id] =
        loaded.tick_number() + 2;
    loaded.replace_player_state(blue, std::move(expiring_blue));
    loaded.update();
    const auto still_revealed = std::ranges::find(
        loaded.units(), attacker_id, &aoe::Unit::id
    );
    require(still_revealed != loaded.units().end());
    require(loaded.is_unit_visible(blue_owner, *still_revealed));
    loaded.update();
    const auto hidden_attacker = std::ranges::find(
        loaded.units(), attacker_id, &aoe::Unit::id
    );
    require(hidden_attacker != loaded.units().end());
    require(!loaded.is_unit_visible(blue_owner, *hidden_attacker));

    // Attack-ground reveals only players with units/buildings inside splash.
    aoe::Simulation ground(aoe::GameMap(32, 12));
    const auto ground_attacker = ground.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::red, {5, 5}
    );
    ground.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {12, 5}
    );
    require(!ground.is_unit_visible(
        aoe::Player::blue, ground.units().front()
    ));
    require(ground.command_attack_ground(ground_attacker, {12, 5}));
    for (int tick = 0;
         tick < 12 && ground.projectiles().empty(); ++tick) {
        ground.update();
    }
    require(!ground.projectiles().empty());
    require(ground.projectiles().front().target == 0);
    require(ground.is_unit_visible(
        aoe::Player::blue, ground.units().front()
    ));

    // Ship miss still triggers reveal and remains bounded by missile flight.
    aoe::GameMap sea_map(36, 12);
    for (int y = 0; y < sea_map.height(); ++y) {
        for (int x = 0; x < sea_map.width(); ++x) {
            sea_map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    aoe::Simulation sea(std::move(sea_map));
    sea.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::red, {30, 1}
    );
    sea.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::red, {31, 3}
    );
    sea.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::red, {32, 5}
    );
    const auto cannon = sea.add_unit(
        aoe::UnitKind::cannon_galleon, aoe::Player::red, {3, 7}
    );
    sea.add_unit(
        aoe::UnitKind::fishing_ship, aoe::Player::blue, {15, 7}
    );
    require(!sea.is_unit_visible(aoe::Player::blue, sea.units()[3]));
    require(sea.command_unit(cannon, {15, 7}));
    for (int tick = 0; tick < 12 && sea.projectiles().empty(); ++tick) {
        sea.update();
    }
    require(!sea.projectiles().empty());
    require(sea.projectiles().front().target == 0);
    require(sea.is_unit_visible(aoe::Player::blue, sea.units()[3]));
    require(sea.player_state(blue).attack_reveal_expiries.at(cannon) -
        sea.tick_number() >= static_cast<std::uint64_t>(
            sea.projectiles().front().ticks_remaining
        ));

    // Observer controller bypass remains presentation-only.
    loaded.replace_controller_states(
        aoe::PlayerControllerState::observer,
        aoe::PlayerControllerState::active
    );
    require(loaded.is_unit_visible_to_controller(
        aoe::Player::blue, *hidden_attacker
    ));
    (void)victim_id;
}

void exploration_sweep_matches_per_tile_visibility() {
    // update_exploration marks tiles from each vision source instead of
    // testing every tile, so it must stay identical to is_visible over
    // the whole map, including the cartography and spy branches.
    aoe::Simulation simulation(aoe::GameMap(64, 48));
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {6, 6}
    );
    simulation.add_building(
        aoe::BuildingKind::outpost, aoe::Player::blue, {20, 30}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {9, 20}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {33, 11}
    );
    simulation.add_building(
        aoe::BuildingKind::castle, aoe::Player::red, {50, 36}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {44, 40}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {57, 8}
    );
    for (aoe::Technology technology : {
             aoe::Technology::cartography,
             aoe::Technology::spy_technology,
             aoe::Technology::town_watch,
         }) {
        simulation.replace_technologies(aoe::Player::blue, {technology});
        for (aoe::Player player : {aoe::Player::blue, aoe::Player::red}) {
            std::vector<bool> before;
            before.reserve(64 * 48);
            for (int y = 0; y < 48; ++y) {
                for (int x = 0; x < 64; ++x) {
                    before.push_back(
                        simulation.is_explored(player, {x, y})
                    );
                }
            }
            simulation.update();
            std::size_t index{};
            for (int y = 0; y < 48; ++y) {
                for (int x = 0; x < 64; ++x) {
                    const aoe::TilePosition tile{x, y};
                    const bool expected =
                        before[index++] ||
                        simulation.is_visible(player, tile);
                    require(
                        simulation.is_explored(player, tile) == expected
                    );
                }
            }
        }
    }
}

void save_round_trip_preserves_exploration_memory() {
    aoe::Simulation simulation(aoe::GameMap(20, 20));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {18, 18}
    );
    require(simulation.command_unit(scout, {10, 2}));
    for (int tick = 0; tick < 16; ++tick) {
        simulation.update();
    }

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-vision-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);

    require(loaded.is_explored(aoe::Player::blue, {2, 2}));
    require(!loaded.is_visible(aoe::Player::blue, {2, 2}));
    require(loaded.is_visible(aoe::Player::blue, {10, 2}));
    require(!loaded.is_explored(aoe::Player::blue, {18, 18}));
}

void enemy_building_memory_is_stale_per_viewer_and_persistent() {
    aoe::Simulation simulation(aoe::GameMap(40, 24));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {8, 8}
    );
    require(simulation.set_unit_stance(scout, aoe::UnitStance::passive));
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {14, 8}
    );
    const aoe::EntityId house = simulation.next_entity_id();
    require(simulation.construct_building_at(
        builder, aoe::BuildingKind::house, {12, 8}
    ));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {36, 20}
    );
    simulation.update();

    const auto visible_memory =
        simulation.remembered_buildings(aoe::Player::blue).at(house);
    require(visible_memory.building.kind == aoe::BuildingKind::house);
    require(visible_memory.building.owner == aoe::Player::red);
    require(visible_memory.building.position == aoe::TilePosition{12, 8});
    require(visible_memory.owner_age == aoe::Age::dark);
    require(visible_memory.maximum_hit_points > 0);
    require(visible_memory.building.construction_ticks_remaining > 0);

    require(simulation.command_unit(scout, {1, 1}));
    for (int tick = 0; tick < 30; ++tick) simulation.update();
    require(!simulation.is_building_visible(
        aoe::Player::blue, visible_memory.building
    ));
    const auto last_seen =
        simulation.remembered_buildings(aoe::Player::blue).at(house);

    // Global age change and hidden destruction must not alter stale image.
    simulation.replace_ages(aoe::Age::dark, aoe::Age::imperial);
    require(simulation.delete_building(house));
    simulation.update();
    const auto& stale =
        simulation.remembered_buildings(aoe::Player::blue).at(house);
    require(stale.owner_age == aoe::Age::dark);
    require(stale.building.hit_points == last_seen.building.hit_points);
    require(stale.building.construction_ticks_remaining ==
        last_seen.building.construction_ticks_remaining);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-building-memory.save";
    const std::string hash_before = aoe::deterministic_state_hash(simulation);
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(aoe::deterministic_state_hash(loaded) == hash_before);
    const auto& restored =
        loaded.remembered_buildings(aoe::Player::blue).at(house);
    require(restored.owner_age == aoe::Age::dark);
    require(restored.building.position == aoe::TilePosition{12, 8});
    require(loaded.remembered_buildings(aoe::Player::red).empty());

    require(loaded.command_unit(scout, {12, 8}));
    for (int tick = 0; tick < 30; ++tick) loaded.update();
    require(!loaded.remembered_buildings(aoe::Player::blue).contains(house));
}

void allied_starting_town_centers_reveal_without_shared_vision() {
    const auto blue = *aoe::PlayerSlotId::from_index(0);
    const auto red = *aoe::PlayerSlotId::from_index(1);
    const auto green = *aoe::PlayerSlotId::from_index(2);
    const auto yellow = *aoe::PlayerSlotId::from_index(3);
    const auto team_one = *aoe::TeamId::numbered(1);
    const auto team_two = *aoe::TeamId::numbered(2);
    const auto roster = aoe::MatchRoster::create({
        {blue, true, team_one, false,
         {{"blue", aoe::RosterControllerKind::human}}},
        {red, true, team_two, false,
         {{"red", aoe::RosterControllerKind::human}}},
        {green, true, team_one, false,
         {{"green", aoe::RosterControllerKind::human}}},
        {yellow, true, team_one, false,
         {{"yellow", aoe::RosterControllerKind::human}}},
    });
    require(roster.has_value());
    auto diplomacy = aoe::RosterDiplomacy::create(
        *roster, {true, false}
    );
    require(diplomacy.has_value());
    // Directed diplomacy proves reveal follows viewer stance, not an assumed
    // symmetric two-player team or broad shared-vision option.
    require(diplomacy->set_stance(green, blue, aoe::Diplomacy::enemy));

    aoe::Simulation simulation(aoe::GameMap(48, 28));
    simulation.replace_roster(*roster, *diplomacy);
    std::vector<aoe::Building> buildings{
        {1, aoe::BuildingKind::town_center,
         aoe::entity_owner_from_slot(blue), {1, 1}, 600},
        {2, aoe::BuildingKind::town_center,
         aoe::entity_owner_from_slot(red), {10, 20}, 600},
        {3, aoe::BuildingKind::town_center,
         aoe::entity_owner_from_slot(green), {20, 2}, 600},
        {4, aoe::BuildingKind::town_center,
         aoe::entity_owner_from_slot(green), {28, 2}, 37, {}, 0, 12},
        {5, aoe::BuildingKind::town_center,
         aoe::entity_owner_from_slot(yellow), {36, 2}, 600},
        {6, aoe::BuildingKind::house,
         aoe::entity_owner_from_slot(green), {20, 14}, 550},
    };
    simulation.replace_state(
        {}, std::move(buildings), {1000, 1000, 1000, 1000},
        {1000, 1000, 1000, 1000}, 0
    );

    const auto& blue_memory = simulation.player_state(blue)
        .remembered_buildings;
    require(blue_memory.size() == 3);
    require(blue_memory.contains(3));
    require(blue_memory.contains(4));
    require(blue_memory.contains(5));
    require(blue_memory.at(4).building.construction_ticks_remaining == 12);
    require(!blue_memory.contains(2));
    require(!blue_memory.contains(6));
    require(simulation.is_explored(blue, {20, 2}));
    require(simulation.is_explored(blue, {23, 5}));
    require(!simulation.is_explored(blue, {24, 2}));
    require(!simulation.is_visible(blue, {20, 2}));
    require(!simulation.is_explored(blue, {20, 14}));
    require(simulation.player_state(green).remembered_buildings.count(1) == 0);
    require(simulation.player_state(yellow).remembered_buildings.contains(1));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-allied-starting-town-centers.save";
    const std::string hash_before = aoe::deterministic_state_hash(simulation);
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(aoe::deterministic_state_hash(loaded) == hash_before);
    require(loaded.player_state(blue).remembered_buildings.contains(3));
    require(!loaded.is_visible(blue, {20, 2}));

    // A post-start diplomacy change does not reveal a newly allied Town
    // Center.  Already revealed Town Centers remain stale reconnaissance when
    // their owner becomes hostile.
    loaded.update();
    auto changed = loaded.roster_diplomacy();
    require(changed.set_stance(blue, red, aoe::Diplomacy::ally));
    require(changed.set_stance(blue, green, aoe::Diplomacy::enemy));
    loaded.replace_roster(loaded.roster(), changed);
    loaded.update();
    require(!loaded.player_state(blue).remembered_buildings.contains(2));
    require(loaded.player_state(blue).remembered_buildings.contains(3));
    require(!loaded.is_explored(blue, {10, 20}));
    const auto frozen_green = loaded.player_state(blue)
        .remembered_buildings.at(3);
    auto green_state = loaded.player_state(green);
    green_state.age = aoe::Age::imperial;
    loaded.replace_player_state(green, std::move(green_state));
    require(loaded.delete_building(3));
    loaded.update();
    const auto& hidden_destroyed = loaded.player_state(blue)
        .remembered_buildings.at(3);
    require(hidden_destroyed.owner_age == frozen_green.owner_age);
    require(hidden_destroyed.building.hit_points ==
        frozen_green.building.hit_points);

    // Cartography transitions from frozen marker to normal allied LOS while
    // leaving unrelated enemy and non-Town-Center state hidden.
    auto blue_state = loaded.player_state(blue);
    blue_state.technologies[static_cast<std::size_t>(
        aoe::Technology::cartography
    )] = true;
    loaded.replace_player_state(blue, std::move(blue_state));
    require(loaded.player_state(blue).remembered_buildings.at(5).owner_age ==
        aoe::Age::dark);
    auto yellow_state = loaded.player_state(yellow);
    yellow_state.age = aoe::Age::imperial;
    loaded.replace_player_state(yellow, std::move(yellow_state));
    auto allied_again = loaded.roster_diplomacy();
    require(allied_again.set_stance(blue, yellow, aoe::Diplomacy::ally));
    loaded.replace_roster(loaded.roster(), allied_again);
    loaded.update();
    require(loaded.is_visible(blue, {36, 2}));
    require(loaded.is_visible(blue, {40, 2}));
    require(!loaded.is_visible(blue, {10, 20}));
    require(loaded.player_state(blue).remembered_buildings.at(5).owner_age ==
        aoe::Age::imperial);

    loaded.replace_controller_states(
        aoe::PlayerControllerState::observer,
        aoe::PlayerControllerState::active
    );
    require(loaded.is_visible_to_controller(aoe::Player::blue, {47, 27}));
    require(loaded.is_explored_to_controller(aoe::Player::blue, {47, 27}));
}

void building_los_is_radial_from_nearest_footprint_and_persists() {
    aoe::Simulation footprint(aoe::GameMap(30, 24));
    const auto castle = footprint.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {2, 2}
    );
    footprint.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {28, 22}
    );
    require(footprint.is_visible(aoe::Player::blue, {13, 12}));
    require(footprint.is_explored(aoe::Player::blue, {13, 12}));
    require(!footprint.is_visible(aoe::Player::blue, {14, 13}));
    const auto diagonal_house = footprint.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 12}
    );
    const auto house = std::ranges::find(
        footprint.buildings(), diagonal_house, &aoe::Building::id
    );
    require(house != footprint.buildings().end());
    require(footprint.is_building_visible(aoe::Player::blue, *house));
    require(footprint.delete_building(castle));
    require(!footprint.is_visible(aoe::Player::blue, {13, 12}));
    require(footprint.is_explored(aoe::Player::blue, {13, 12}));

    aoe::Simulation upgrades(aoe::GameMap(28, 22));
    upgrades.add_building(
        aoe::BuildingKind::outpost, aoe::Player::blue, {2, 2}
    );
    upgrades.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {26, 20}
    );
    require(!upgrades.is_visible(aoe::Player::blue, {12, 11}));
    upgrades.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::town_watch, aoe::Technology::town_patrol}
    );
    require(upgrades.is_visible(aoe::Player::blue, {12, 11}));
    upgrades.update();
    require(upgrades.is_explored(aoe::Player::blue, {12, 11}));
    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-building-radial-los.save";
    aoe::save_game(upgrades, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.is_visible(aoe::Player::blue, {12, 11}));
    require(loaded.is_explored(aoe::Player::blue, {12, 11}));

    aoe::Simulation shared(aoe::GameMap(30, 24));
    shared.add_building(
        aoe::BuildingKind::outpost, aoe::Player::red, {2, 2}
    );
    shared.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {28, 22}
    );
    shared.replace_technologies(
        aoe::Player::blue, {aoe::Technology::cartography}
    );
    require(!shared.is_visible(aoe::Player::blue, {7, 5}));
    aoe::Replay replay;
    replay.record(
        shared.tick_number(),
        aoe::SetDiplomacyCommand{
            aoe::Player::blue,
            aoe::Player::red,
            aoe::Diplomacy::ally
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-building-radial-los.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(shared);
    require(shared.is_visible(aoe::Player::blue, {7, 5}));
    require(shared.is_explored(aoe::Player::blue, {7, 5}));
}

void villager_gathers_wood() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    const int original_wood = simulation.economy(aoe::Player::blue).wood;
    const int original_tree = simulation.map().resource_amount_at({3, 5});
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.command_selected({3, 5}));

    for (int tick = 0;
         tick < 250 &&
         simulation.economy(aoe::Player::blue).wood == original_wood;
         ++tick) {
        simulation.update();
    }

    require(simulation.economy(aoe::Player::blue).wood == original_wood + 10);
    require(simulation.map().resource_amount_at({3, 5}) == original_tree - 10);
    require(simulation.units().front().has_resource_target);
    require(simulation.units().front().carried_amount == 0);
}

void gathering_retries_after_temporary_route_obstruction() {
    aoe::GameMap map(8, 3);
    for (int x = 0; x < map.width(); ++x) {
        map.set_terrain({x, 0}, aoe::Terrain::water);
        map.set_terrain({x, 2}, aoe::Terrain::water);
    }
    map.set_terrain({3, 0}, aoe::Terrain::grass);
    map.set_terrain({6, 1}, aoe::Terrain::forest);
    map.set_resource_amount({6, 1}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 1}
    );
    require(simulation.command_unit(worker, {6, 1}));

    const aoe::EntityId blocker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 1}
    );
    for (int tick = 0;
         tick < 10 && simulation.units().front().moving;
         ++tick) {
        simulation.update();
    }
    const aoe::Unit& stopped = simulation.units().front();
    require(stopped.has_resource_target);
    require(!stopped.moving);
    require(stopped.position == aoe::TilePosition(2, 1));

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-gathering-obstruction.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().has_resource_target);
    require(!loaded.units().front().moving);

    require(simulation.command_unit(blocker, {3, 0}));
    require(loaded.command_unit(blocker, {3, 0}));
    for (int tick = 0; tick < 60; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(simulation.units().front().has_resource_target);
    require(simulation.units().front().carried_amount > 0);
    require(loaded.units().front().has_resource_target);
    require(
        loaded.units().front().carried_amount ==
        simulation.units().front().carried_amount
    );
}

void gathering_order_survives_initial_route_blockage() {
    aoe::GameMap map(9, 3);
    for (int x = 0; x < map.width(); ++x) {
        map.set_terrain({x, 0}, aoe::Terrain::water);
        map.set_terrain({x, 2}, aoe::Terrain::water);
    }
    map.set_terrain({3, 0}, aoe::Terrain::grass);
    map.set_terrain({6, 1}, aoe::Terrain::berry_bush);
    map.set_resource_amount({6, 1}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId blocker = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {3, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {8, 1}
    );

    require(simulation.command_unit(worker, {6, 1}));
    require(simulation.units().front().has_resource_target);
    require(!simulation.units().front().moving);

    require(simulation.command_unit(blocker, {3, 0}));
    for (int tick = 0; tick < 60; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().has_resource_target);
    require(simulation.units().front().carried_amount > 0);
    require(
        simulation.units().front().position != aoe::TilePosition(6, 1)
    );
}

void gathering_collision_pauses_before_fresh_route() {
    aoe::GameMap map(10, 7);
    map.set_terrain({8, 3}, aoe::Terrain::berry_bush);
    map.set_resource_amount({8, 3}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 3}
    );
    require(simulation.command_unit(worker, {8, 3}));
    const aoe::Unit& routed = simulation.units().front();
    require(routed.moving);
    require(routed.next_path_step < routed.path.size());
    const aoe::TilePosition blocked_step =
        routed.path[routed.next_path_step];
    const aoe::EntityId blocker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        blocked_step
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 6}
    );

    simulation.update();
    require(simulation.units().front().has_resource_target);
    require(!simulation.units().front().moving);

    require(simulation.command_unit(blocker, {1, 1}));
    for (int tick = 0; tick < 60; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().carried_amount > 0);
}

void returning_gatherer_retries_blocked_valid_drop_off() {
    aoe::GameMap map(16, 4);
    for (int x = 0; x < map.width(); ++x) {
        map.set_terrain({x, 1}, aoe::Terrain::water);
        map.set_terrain({x, 3}, aoe::Terrain::water);
    }
    map.set_terrain({1, 1}, aoe::Terrain::forest);
    map.set_resource_amount({1, 1}, 100);
    map.set_terrain({2, 1}, aoe::Terrain::grass);
    map.set_terrain({3, 1}, aoe::Terrain::grass);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::blue,
        {12, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 2}
    );
    require(simulation.command_unit(worker, {1, 1}));
    for (int tick = 0;
         tick < 200 && !simulation.units().front().returning_resource;
         ++tick) {
        simulation.update();
    }
    const aoe::Unit& returning = simulation.units().front();
    require(returning.returning_resource);
    require(returning.moving);
    require(returning.next_path_step < returning.path.size());
    const aoe::TilePosition blocked_step =
        returning.path[returning.next_path_step];
    require(blocked_step == aoe::TilePosition(2, 2));
    const aoe::EntityId blocker = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        blocked_step
    );

    simulation.update();
    require(simulation.units().front().returning_resource);
    require(!simulation.units().front().moving);
    require(simulation.command_unit(
        blocker,
        {blocked_step.x + 1, blocked_step.y - 1}
    ));
    for (int tick = 0;
         tick < 5 &&
         simulation.units().back().position !=
             aoe::TilePosition(
                 blocked_step.x + 1,
                 blocked_step.y - 1
             );
         ++tick) {
        simulation.update();
    }
    require(
        simulation.units().back().position ==
        aoe::TilePosition(
            blocked_step.x + 1,
            blocked_step.y - 1
        )
    );
    const int wood_before = simulation.economy(aoe::Player::blue).wood;
    for (int tick = 0;
         tick < 30 &&
         simulation.economy(aoe::Player::blue).wood == wood_before;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).wood == wood_before + 10);
}

void gatherer_retargets_depleted_resource_before_arrival() {
    aoe::GameMap map(12, 7);
    map.set_terrain({7, 3}, aoe::Terrain::berry_bush);
    map.set_resource_amount({7, 3}, 1);
    map.set_terrain({9, 3}, aoe::Terrain::berry_bush);
    map.set_resource_amount({9, 3}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId traveler = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 3}
    );
    const aoe::EntityId nearby = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {6, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 6}
    );
    require(simulation.command_unit(traveler, {7, 3}));
    require(simulation.command_unit(nearby, {7, 3}));
    for (int tick = 0;
         tick < 20 && simulation.map().resource_amount_at({7, 3}) > 0;
         ++tick) {
        simulation.update();
    }
    require(simulation.map().resource_amount_at({7, 3}) == 0);

    simulation.update();
    require(simulation.units().front().has_resource_target);
    require(
        simulation.units().front().resource_target ==
        aoe::TilePosition(9, 3)
    );
}

void diagonal_berry_workers_gather_without_route_churn() {
    aoe::GameMap map(10, 8);
    map.set_terrain({5, 4}, aoe::Terrain::berry_bush);
    map.set_resource_amount({5, 4}, 100);
    aoe::Simulation simulation(std::move(map));
    constexpr std::array<aoe::TilePosition, 3> starts{{
        {4, 3}, {5, 3}, {6, 3},
    }};
    for (aoe::TilePosition start : starts) {
        const aoe::EntityId worker = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            start
        );
        require(simulation.command_unit(worker, {5, 4}));
    }
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 7}
    );

    for (int tick = 0; tick < 17; ++tick) simulation.update();
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const aoe::Unit& worker = simulation.units()[index];
        require(worker.position == starts[index]);
        require(worker.has_resource_target);
        require(!worker.moving);
        require(worker.carried_amount == 1);
    }
}

void crowded_berry_ring_allows_nearby_workers_to_gather() {
    aoe::GameMap map(12, 10);
    constexpr aoe::TilePosition berry{6, 5};
    map.set_terrain(berry, aoe::Terrain::berry_bush);
    map.set_resource_amount(berry, 100);
    aoe::Simulation simulation(std::move(map));
    constexpr std::array<aoe::TilePosition, 3> workers{{
        {5, 4}, {4, 4}, {5, 3},
    }};
    for (aoe::TilePosition position : workers) {
        const aoe::EntityId worker = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            position
        );
        require(simulation.command_unit(worker, berry));
    }
    constexpr std::array<aoe::TilePosition, 7> occupied_ring{{
        {5, 5}, {5, 6}, {6, 4}, {6, 6},
        {7, 4}, {7, 5}, {7, 6},
    }};
    for (aoe::TilePosition position : occupied_ring) {
        simulation.add_unit(
            aoe::UnitKind::sheep,
            aoe::Player::blue,
            position
        );
    }
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 9}
    );

    for (int tick = 0; tick < 150; ++tick) {
        simulation.update();
    }
    for (std::size_t index = 0; index < workers.size(); ++index) {
        const aoe::Unit& worker = simulation.units()[index];
        require(worker.has_resource_target);
        require(worker.carried_amount > 0);
    }
}

void gathering_waits_for_a_temporarily_unavailable_drop_off() {
    aoe::GameMap map(12, 6);
    map.set_terrain({2, 2}, aoe::Terrain::forest);
    map.set_resource_amount({2, 2}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 5}
    );
    require(simulation.command_unit(worker, {2, 2}));

    for (int tick = 0; tick < 150; ++tick) {
        simulation.update();
    }
    const aoe::Unit& waiting = simulation.units().front();
    require(waiting.carried_amount == 10);
    require(waiting.has_resource_target);
    require(waiting.returning_resource);
    require(!waiting.moving);

    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {6, 1}
    );
    const int wood_before = simulation.economy(aoe::Player::blue).wood;
    for (int tick = 0;
         tick < 30 &&
         simulation.economy(aoe::Player::blue).wood == wood_before;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).wood == wood_before + 10);
    require(simulation.units().front().has_resource_target);
}

void villagers_share_resource_through_repeated_deposit_cycles() {
    aoe::GameMap map(14, 9);
    map.set_terrain({7, 3}, aoe::Terrain::forest);
    map.set_resource_amount({7, 3}, 300);
    aoe::Simulation simulation(std::move(map));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 1}
    );
    constexpr std::array<aoe::TilePosition, 4> starts{{
        {6, 2}, {6, 3}, {6, 4}, {7, 5},
    }};
    for (aoe::TilePosition start : starts) {
        const aoe::EntityId worker = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            start
        );
        require(simulation.command_unit(worker, {7, 3}));
    }
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 8}
    );
    const int wood_before = simulation.economy(aoe::Player::blue).wood;
    for (int tick = 0;
         tick < 500 &&
         simulation.economy(aoe::Player::blue).wood < wood_before + 80;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).wood >= wood_before + 80);
    for (std::size_t index = 0; index < starts.size(); ++index) {
        require(simulation.units()[index].has_resource_target);
    }
}

void late_arriving_villager_retargets_after_shared_depletion() {
    aoe::GameMap map(14, 7);
    map.set_terrain({4, 3}, aoe::Terrain::forest);
    map.set_resource_amount({4, 3}, 1);
    map.set_terrain({8, 3}, aoe::Terrain::forest);
    map.set_resource_amount({8, 3}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId near_worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 3}
    );
    const aoe::EntityId far_worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {0, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {10, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 6}
    );
    require(simulation.command_unit(near_worker, {4, 3}));
    require(simulation.command_unit(far_worker, {4, 3}));

    bool retargeted = false;
    for (int tick = 0; tick < 30; ++tick) {
        simulation.update();
        const aoe::Unit& worker = simulation.units()[1];
        if (worker.resource_target == aoe::TilePosition(8, 3)) {
            retargeted = true;
            break;
        }
    }
    require(retargeted);
    require(simulation.units()[1].has_resource_target);
}

void land_gathering_command_is_deterministic_through_replay() {
    aoe::GameMap map(12, 7);
    map.set_terrain({6, 3}, aoe::Terrain::forest);
    map.set_resource_amount({6, 3}, 100);
    const auto make_simulation = [&map]() {
        aoe::Simulation simulation(map);
        simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {0, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {5, 3}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {11, 6}
        );
        return simulation;
    };
    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    const aoe::EntityId worker = first.units().front().id;

    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{worker, {6, 3}});
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-land-gathering.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);

    for (int tick = 0; tick < 80; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.economy(aoe::Player::blue).wood > 0);
    require(first.economy(aoe::Player::blue).wood ==
        second.economy(aoe::Player::blue).wood);
    require(first.map().resource_amount_at({6, 3}) ==
        second.map().resource_amount_at({6, 3}));
    require(first.units().front().has_resource_target);
    require(second.units().front().has_resource_target);
}

void double_bit_axe_adds_exact_persisted_wood_rate() {
    const aoe::TechnologyRules& rules =
        aoe::rules_for(aoe::Technology::double_bit_axe);
    require(rules.researched_at == aoe::BuildingKind::lumber_camp);
    require(rules.minimum_age == aoe::Age::feudal);
    require(rules.food_cost == 100);
    require(rules.wood_cost == 50);

    aoe::GameMap map(12, 8);
    map.set_terrain({5, 2}, aoe::Terrain::forest);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId lumber_camp = simulation.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::blue,
        {0, 4}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 5}
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {50, 100, 0, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.research_technology_at(
        lumber_camp,
        aoe::Technology::double_bit_axe
    ));
    require(simulation.economy(aoe::Player::blue).wood == 0);
    require(simulation.economy(aoe::Player::blue).food == 0);
    for (int tick = 0; tick < rules.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::double_bit_axe
    ));

    require(simulation.command_unit(villager, {5, 2}));
    for (int tick = 0;
         tick < 20 && simulation.units().front().carried_amount == 0;
         ++tick) {
        simulation.update();
    }
    require(simulation.units().front().carried_amount == 1);
    require(simulation.units().front().gather_work_remainder < 10000);
    for (int tick = 0;
         tick < 20 && simulation.units().front().carried_amount == 1;
         ++tick) {
        simulation.update();
    }
    require(simulation.units().front().carried_amount == 2);
    require(simulation.units().front().gather_work_remainder < 10000);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-double-bit-axe-test.save";
    const int saved_remainder =
        simulation.units().front().gather_work_remainder;
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::double_bit_axe
    ));
    require(
        loaded.units().front().gather_work_remainder == saved_remainder
    );
    for (int tick = 0;
         tick < 20 && loaded.units().front().carried_amount == 2;
         ++tick) {
        loaded.update();
    }
    require(loaded.units().front().carried_amount == 3);
    require(loaded.units().front().gather_work_remainder < 10000);
}

void forest_depletes_after_finite_wood_is_delivered() {
    aoe::GameMap map(8, 5);
    map.set_terrain({2, 1}, aoe::Terrain::forest);
    map.set_resource_amount({2, 1}, 3);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {4, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {4, 4}
    );
    const int original_wood = simulation.economy(aoe::Player::blue).wood;

    require(simulation.command_unit(villager, {2, 1}));
    for (int tick = 0; tick < 100; ++tick) {
        simulation.update();
    }

    require(simulation.economy(aoe::Player::blue).wood == original_wood + 3);
    require(simulation.map().terrain_at({2, 1}) == aoe::Terrain::grass);
    require(simulation.map().resource_amount_at({2, 1}) == 0);
}

void villagers_continue_to_nearest_same_resource_after_depletion() {
    constexpr std::array<aoe::Terrain, 4> terrains{
        aoe::Terrain::forest,
        aoe::Terrain::berry_bush,
        aoe::Terrain::gold_mine,
        aoe::Terrain::stone_mine,
    };
    for (std::size_t index = 0; index < terrains.size(); ++index) {
        aoe::GameMap map(12, 8);
        map.set_terrain({5, 1}, terrains[index]);
        map.set_resource_amount({5, 1}, 1);
        map.set_terrain({7, 1}, terrains[index]);
        map.set_resource_amount({7, 1}, 100);
        aoe::Simulation simulation(std::move(map));
        const aoe::EntityId villager = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {4, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {0, 0}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {11, 7}
        );
        require(simulation.command_unit(villager, {5, 1}));

        bool retasked = false;
        for (int tick = 0; tick < 20; ++tick) {
            simulation.update();
            const aoe::Unit& worker = simulation.units().front();
            if (worker.resource_target == aoe::TilePosition(7, 1) &&
                simulation.map().resource_amount_at({5, 1}) == 0) {
                retasked = true;
                break;
            }
        }
        require(retasked);
        require(simulation.units().front().has_resource_target);

        const auto path =
            std::filesystem::temp_directory_path() /
            ("aoe-resource-retask-" + std::to_string(index) + ".save");
        aoe::save_game(simulation, path);
        aoe::Simulation loaded = aoe::load_game(path);
        std::filesystem::remove(path);
        require(
            loaded.units().front().resource_target ==
            aoe::TilePosition(7, 1)
        );
        for (int tick = 0; tick < 30; ++tick) {
            simulation.update();
            loaded.update();
        }
        require(
            loaded.map().resource_amount_at({7, 1}) ==
            simulation.map().resource_amount_at({7, 1})
        );
        require(simulation.map().resource_amount_at({7, 1}) < 100);
    }
}

void sheep_retask_after_gold_deposit_carries_food() {
    aoe::GameMap map(16, 9);
    map.set_terrain({6, 4}, aoe::Terrain::gold_mine);
    map.set_resource_amount({6, 4}, 100);
    aoe::Simulation simulation(std::move(map));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId worker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {5, 4}
    );
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {9, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 8}
    );
    require(simulation.command_unit(worker, {6, 4}));
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
    }
    const int carried_gold = simulation.units().front().carried_amount;
    require(carried_gold > 0 && carried_gold < 10);
    const int gold_before = simulation.economy(aoe::Player::blue).gold;
    require(simulation.command_unit(worker, {9, 4}));

    for (int tick = 0;
         tick < 40 &&
         simulation.economy(aoe::Player::blue).gold == gold_before;
         ++tick) {
        simulation.update();
    }
    require(
        simulation.economy(aoe::Player::blue).gold ==
        gold_before + carried_gold
    );
    const int deposited_gold =
        simulation.economy(aoe::Player::blue).gold;
    const int food_before = simulation.economy(aoe::Player::blue).food;
    for (int tick = 0;
         tick < 60 &&
         simulation.economy(aoe::Player::blue).food == food_before;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).food > food_before);
    require(simulation.economy(aoe::Player::blue).gold == deposited_gold);
    require(simulation.units()[1].id == sheep);
}

void villagers_deliver_all_resource_types() {
    constexpr std::array<std::pair<aoe::Terrain, aoe::ResourceKind>, 4>
        resources{{
            {aoe::Terrain::forest, aoe::ResourceKind::wood},
            {aoe::Terrain::berry_bush, aoe::ResourceKind::food},
            {aoe::Terrain::gold_mine, aoe::ResourceKind::gold},
            {aoe::Terrain::stone_mine, aoe::ResourceKind::stone},
        }};
    const auto amount = [](const aoe::Economy& economy,
                           aoe::ResourceKind resource) {
        switch (resource) {
            case aoe::ResourceKind::wood: return economy.wood;
            case aoe::ResourceKind::food: return economy.food;
            case aoe::ResourceKind::gold: return economy.gold;
            case aoe::ResourceKind::stone: return economy.stone;
            case aoe::ResourceKind::none: return 0;
        }
        return 0;
    };

    for (const auto& [terrain, resource] : resources) {
        aoe::GameMap map(8, 5);
        map.set_terrain({2, 1}, terrain);
        map.set_resource_amount({2, 1}, 3);
        aoe::Simulation simulation(std::move(map));
        const aoe::EntityId villager = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {1, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {4, 0}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {7, 4}
        );
        const int before = amount(
            simulation.economy(aoe::Player::blue),
            resource
        );

        require(simulation.command_unit(villager, {2, 1}));
        for (int tick = 0; tick < 10; ++tick) {
            simulation.update();
        }
        require(
            amount(simulation.economy(aoe::Player::blue), resource) ==
            before + 3
        );
        require(simulation.map().terrain_at({2, 1}) == aoe::Terrain::grass);
        require(simulation.units().front().carried_amount == 0);
        require(
            simulation.units().front().carried_resource ==
            aoe::ResourceKind::none
        );
    }
}

void villagers_choose_compatible_specialized_drop_offs() {
    struct Case {
        aoe::Terrain terrain;
        aoe::BuildingKind compatible;
        aoe::BuildingKind incompatible;
    };
    constexpr std::array<Case, 4> cases{{
        {aoe::Terrain::forest,
         aoe::BuildingKind::lumber_camp,
         aoe::BuildingKind::mill},
        {aoe::Terrain::berry_bush,
         aoe::BuildingKind::mill,
         aoe::BuildingKind::lumber_camp},
        {aoe::Terrain::gold_mine,
         aoe::BuildingKind::mining_camp,
         aoe::BuildingKind::mill},
        {aoe::Terrain::stone_mine,
         aoe::BuildingKind::mining_camp,
         aoe::BuildingKind::lumber_camp},
    }};

    for (const Case& test : cases) {
        aoe::GameMap map(10, 5);
        map.set_terrain({8, 1}, test.terrain);
        map.set_resource_amount({8, 1}, 20);
        aoe::Simulation simulation(std::move(map));
        const aoe::EntityId villager = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {7, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {0, 0}
        );
        simulation.add_building(
            test.incompatible,
            aoe::Player::blue,
            {7, 2}
        );
        simulation.add_building(
            test.compatible,
            aoe::Player::blue,
            {6, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {9, 4}
        );
        const auto path = std::filesystem::temp_directory_path() /
            "aoe-drop-off-test.save";
        aoe::save_game(simulation, path);
        aoe::Simulation loaded = aoe::load_game(path);
        std::filesystem::remove(path);
        require(loaded.buildings()[2].kind == test.compatible);

        require(simulation.command_unit(villager, {8, 1}));
        for (int tick = 0; tick < 10; ++tick) {
            simulation.update();
        }
        require(simulation.units().front().returning_resource);
        require(
            simulation.units().front().destination ==
            aoe::TilePosition(6, 1)
        );
    }
}

void destroyed_drop_off_reroutes_carrier() {
    aoe::GameMap map(10, 5);
    map.set_terrain({8, 1}, aoe::Terrain::forest);
    map.set_resource_amount({8, 1}, 20);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {7, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::blue,
        {4, 1}
    );
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {5, 1}
    );
    std::vector<aoe::Building> weakened = simulation.buildings();
    weakened.back().hit_points = 1;
    simulation.replace_state(
        simulation.units(),
        std::move(weakened),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );

    require(simulation.command_unit(villager, {8, 1}));
    for (int tick = 0; tick < 10; ++tick) {
        simulation.update();
    }
    require(
        simulation.units().front().destination ==
        aoe::TilePosition(4, 1)
    );
    require(simulation.command_unit(attacker, {4, 1}));
    simulation.update();
    simulation.update();
    require(simulation.buildings().size() == 1);
    require(
        simulation.units().front().destination ==
        aoe::TilePosition(0, 0)
    );
    require(simulation.units().front().returning_resource);
}

void farms_construct_harvest_exhaust_and_reseed() {
    aoe::Simulation simulation(aoe::GameMap(12, 5));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId mill = simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::blue,
        {6, 1}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {7, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 4}
    );
    require(simulation.construct_building_at(
        villager,
        aoe::BuildingKind::farm,
        {8, 1}
    ));
    require(!simulation.buildings().back().completed());
    for (int tick = 0;
         tick < aoe::rules_for(aoe::BuildingKind::farm).construction_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.buildings().back().completed());
    require(simulation.buildings().back().resource_amount == 175);

    const aoe::EntityId farm = simulation.buildings().back().id;
    std::vector<aoe::Building> short_farm = simulation.buildings();
    short_farm.back().resource_amount = 3;
    simulation.replace_state(
        simulation.units(),
        std::move(short_farm),
        {140, 200, 0, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    aoe::Replay recorded;
    recorded.record(
        simulation.tick_number(),
        aoe::ReseedFarmCommand{mill}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-farm-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    recorded.apply_current_tick(simulation);
    require(simulation.farm_reseed_queue(aoe::Player::blue) == 1);
    require(simulation.economy(aoe::Player::blue).wood == 80);
    const int original_food =
        simulation.economy(aoe::Player::blue).food;
    require(simulation.command_unit(villager, {8, 1}));
    simulation.update();
    simulation.update();
    require(simulation.units().front().resource_building_id == farm);
    require(simulation.units().front().carried_resource ==
            aoe::ResourceKind::food);
    require(simulation.units().front().carried_amount == 2);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-farm-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().back().resource_amount == 1);
    require(loaded.units().front().resource_building_id == farm);
    require(loaded.farm_reseed_queue(aoe::Player::blue) == 1);

    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(simulation.buildings().back().resource_amount > 0);
    require(
        loaded.buildings().back().resource_amount ==
        simulation.buildings().back().resource_amount
    );
    require(simulation.farm_reseed_queue(aoe::Player::blue) == 0);
    require(loaded.farm_reseed_queue(aoe::Player::blue) == 0);
    require(
        loaded.economy(aoe::Player::blue).food ==
        simulation.economy(aoe::Player::blue).food
    );

}

void horse_collar_upgrades_existing_future_and_reseeded_farms() {
    const aoe::TechnologyRules& rules =
        aoe::rules_for(aoe::Technology::horse_collar);
    require(rules.researched_at == aoe::BuildingKind::mill);
    require(rules.minimum_age == aoe::Age::feudal);
    require(rules.food_cost == 75);
    require(rules.wood_cost == 75);

    aoe::Simulation simulation(aoe::GameMap(12, 9));
    const aoe::EntityId mill = simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId active_farm = simulation.add_building(
        aoe::BuildingKind::farm,
        aoe::Player::blue,
        {3, 1}
    );
    const aoe::EntityId exhausted_farm = simulation.add_building(
        aoe::BuildingKind::farm,
        aoe::Player::blue,
        {5, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 6}
    );
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings[1].resource_amount = 100;
    buildings[2].resource_amount = 0;
    simulation.replace_state(
        simulation.units(),
        std::move(buildings),
        {75, 75, 0, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.research_technology_at(
        mill,
        aoe::Technology::horse_collar
    ));
    require(simulation.economy(aoe::Player::blue).wood == 0);
    require(simulation.economy(aoe::Player::blue).food == 0);
    for (int tick = 0; tick < rules.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::horse_collar
    ));
    require(simulation.buildings()[1].id == active_farm);
    require(simulation.buildings()[1].resource_amount == 175);
    require(simulation.buildings()[2].id == exhausted_farm);
    require(simulation.buildings()[2].resource_amount == 0);
    require(simulation.farm_capacity(aoe::Player::blue) == 250);

    const aoe::EntityId future_farm = simulation.add_building(
        aoe::BuildingKind::farm,
        aoe::Player::blue,
        {7, 1}
    );
    require(simulation.buildings().back().id == future_farm);
    require(simulation.buildings().back().resource_amount == 250);

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {60, 0, 0, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.reseed_farm(mill));
    require(simulation.farm_reseed_queue(aoe::Player::blue) == 1);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-horse-collar-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::horse_collar
    ));
    require(loaded.farm_capacity(aoe::Player::blue) == 250);
    require(loaded.buildings()[1].resource_amount == 175);
    require(loaded.buildings()[2].resource_amount == 0);
    require(loaded.farm_reseed_queue(aoe::Player::blue) == 1);
    require(loaded.buildings()[4].resource_amount == 250);
}

void farm_reseed_payment_is_atomic_and_civilization_aware() {
    struct Case {
        aoe::Civilization civilization;
        int cost;
    };
    constexpr std::array cases{
        Case{aoe::Civilization::britons, 60},
        Case{aoe::Civilization::teutons, 36},
    };

    for (const Case& test : cases) {
        aoe::Simulation simulation(aoe::GameMap(8, 6));
        simulation.replace_civilizations(
            test.civilization, aoe::Civilization::goths
        );
        const aoe::EntityId mill = simulation.add_building(
            aoe::BuildingKind::mill, aoe::Player::blue, {1, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {6, 4}
        );
        simulation.replace_state(
            simulation.units(),
            simulation.buildings(),
            {test.cost - 1, 0, 0, 0},
            simulation.economy(aoe::Player::red),
            simulation.tick_number()
        );
        require(!simulation.reseed_farm(mill));
        require(simulation.economy(aoe::Player::blue).wood ==
                test.cost - 1);
        require(simulation.farm_reseed_queue(aoe::Player::blue) == 0);

        simulation.replace_state(
            simulation.units(),
            simulation.buildings(),
            {test.cost, 0, 0, 0},
            simulation.economy(aoe::Player::red),
            simulation.tick_number()
        );
        require(simulation.reseed_farm(mill));
        require(simulation.economy(aoe::Player::blue).wood == 0);
        require(simulation.farm_reseed_queue(aoe::Player::blue) == 1);
        require(!simulation.reseed_farm(mill));
        require(simulation.economy(aoe::Player::blue).wood == 0);
        require(simulation.farm_reseed_queue(aoe::Player::blue) == 1);
    }

    aoe::Simulation queued(aoe::GameMap(10, 6));
    const aoe::EntityId mill = queued.add_building(
        aoe::BuildingKind::mill, aoe::Player::blue, {0, 0}
    );
    const aoe::EntityId farm = queued.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {4, 2}
    );
    const aoe::EntityId farmer = queued.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 2}
    );
    queued.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {8, 4}
    );
    std::vector<aoe::Building> buildings = queued.buildings();
    buildings[1].resource_amount = 1;
    queued.replace_state(
        queued.units(), buildings, {60, 0, 0, 0},
        queued.economy(aoe::Player::red), 0
    );
    require(queued.reseed_farm(mill));
    buildings.erase(buildings.begin());
    queued.replace_state(
        queued.units(), std::move(buildings),
        queued.economy(aoe::Player::blue),
        queued.economy(aoe::Player::red), 0
    );
    require(queued.farm_reseed_queue(aoe::Player::blue) == 1);
    queued.replace_technologies(
        aoe::Player::blue, {aoe::Technology::horse_collar}
    );
    require(queued.command_unit(farmer, {4, 2}));
    queued.update();
    require(queued.farm_reseed_queue(aoe::Player::blue) == 0);
    const auto replanted = std::ranges::find_if(
        queued.buildings(), [farm](const aoe::Building& building) {
            return building.id == farm;
        }
    );
    require(replanted != queued.buildings().end());
    require(replanted->resource_amount == 250);

    aoe::Simulation capped(aoe::GameMap(8, 6));
    const aoe::EntityId capped_mill = capped.add_building(
        aoe::BuildingKind::mill, aoe::Player::blue, {1, 1}
    );
    capped.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 4}
    );
    capped.replace_state(
        capped.units(), capped.buildings(), {100, 0, 0, 0},
        capped.economy(aoe::Player::red), 0
    );
    capped.replace_farm_reseed_queues(
        aoe::Simulation::maximum_farm_reseed_queue, 0
    );
    require(!capped.reseed_farm(capped_mill));
    require(capped.economy(aoe::Player::blue).wood == 100);

    aoe::Simulation simultaneous(aoe::GameMap(12, 7));
    simultaneous.add_building(
        aoe::BuildingKind::mill, aoe::Player::red, {0, 0}
    );
    simultaneous.add_building(
        aoe::BuildingKind::farm, aoe::Player::red, {4, 2}
    );
    simultaneous.add_building(
        aoe::BuildingKind::farm, aoe::Player::red, {7, 2}
    );
    simultaneous.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {10, 5}
    );
    std::vector<aoe::Building> empty = simultaneous.buildings();
    empty[1].resource_amount = 0;
    empty[2].resource_amount = 0;
    simultaneous.replace_state(
        simultaneous.units(), std::move(empty),
        simultaneous.economy(aoe::Player::blue),
        {60, 0, 0, 0}, 5
    );
    simultaneous.replace_farm_reseed_queues(0, 1);
    aoe::ComputerPlayer computer(aoe::Player::red);
    computer.update(simultaneous);
    require(simultaneous.buildings()[1].resource_amount == 175);
    require(simultaneous.buildings()[2].resource_amount == 175);
    require(simultaneous.farm_reseed_queue(aoe::Player::red) == 0);
    require(simultaneous.economy(aoe::Player::red).wood == 0);
}

void fortified_wall_upgrades_existing_future_walls_and_gates() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::fortified_wall);
    require(technology.researched_at == aoe::BuildingKind::university);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 200);
    require(technology.wood_cost == 100);
    require(technology.research_ticks == 10);
    require(aoe::rules_for(aoe::BuildingKind::stone_gate_x).pierce_armor == 6);

    aoe::Simulation simulation(aoe::GameMap(18, 12));
    const aoe::EntityId university = simulation.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::stone_wall, aoe::Player::blue, {5, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::stone_gate_x, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 8}
    );
    auto buildings = simulation.buildings();
    buildings[1].hit_points = 1000;
    buildings[2].hit_points = 2000;
    simulation.replace_state(
        simulation.units(), std::move(buildings),
        {100, 200, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(simulation.research_technology_at(
        university, aoe::Technology::fortified_wall
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::fortified_wall
    ));
    require(simulation.buildings()[1].hit_points == 2200);
    require(simulation.buildings()[2].hit_points == 3250);
    require(simulation.buildings()[1].kind ==
            aoe::BuildingKind::fortified_wall);
    require(simulation.buildings()[2].kind ==
            aoe::BuildingKind::fortified_gate_x);
    require(simulation.maximum_hit_points(simulation.buildings()[1]) == 3000);
    require(simulation.maximum_hit_points(simulation.buildings()[2]) == 4000);
    require(simulation.melee_armor(simulation.buildings()[1]) == 12);
    require(simulation.pierce_armor(simulation.buildings()[1]) == 12);
    require(simulation.melee_armor(simulation.buildings()[2]) == 6);
    require(simulation.pierce_armor(simulation.buildings()[2]) == 6);

    simulation.add_building(
        aoe::BuildingKind::stone_wall, aoe::Player::blue, {5, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::stone_gate_y, aoe::Player::blue, {12, 2}
    );
    require(simulation.buildings()[4].hit_points == 3000);
    require(simulation.buildings()[5].hit_points == 4000);
    require(simulation.buildings()[4].kind ==
            aoe::BuildingKind::fortified_wall);
    require(simulation.buildings()[5].kind ==
            aoe::BuildingKind::fortified_gate_y);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-fortified-wall-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::fortified_wall
    ));
    require(loaded.maximum_hit_points(loaded.buildings()[4]) == 3000);
    require(loaded.maximum_hit_points(loaded.buildings()[5]) == 4000);
    require(loaded.buildings()[4].kind ==
            aoe::BuildingKind::fortified_wall);
    require(loaded.buildings()[5].kind ==
            aoe::BuildingKind::fortified_gate_y);
}

void guard_tower_upgrades_existing_future_towers_and_attack() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::guard_tower);
    require(technology.researched_at == aoe::BuildingKind::university);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 100);
    require(technology.wood_cost == 250);
    require(technology.research_ticks == 6);

    aoe::Simulation simulation(aoe::GameMap(16, 10));
    const aoe::EntityId university = simulation.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {5, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {9, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 7}
    );
    auto buildings = simulation.buildings();
    buildings[1].hit_points = 800;
    auto units = simulation.units();
    units[0].stance = aoe::UnitStance::passive;
    simulation.replace_state(
        std::move(units), std::move(buildings),
        {250, 100, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(simulation.research_technology_at(
        university, aoe::Technology::guard_tower
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::guard_tower
    ));
    require(simulation.buildings()[1].hit_points == 1280);
    require(simulation.buildings()[1].kind ==
            aoe::BuildingKind::guard_tower);
    require(simulation.maximum_hit_points(simulation.buildings()[1]) == 1500);
    require(simulation.melee_armor(simulation.buildings()[1]) == 2);
    require(simulation.pierce_armor(simulation.buildings()[1]) == 8);
    for (int tick = 0;
         tick < 12 && simulation.projectiles().empty();
         ++tick) {
        simulation.update();
    }
    require(!simulation.projectiles().empty());
    require(simulation.projectiles().back().damage == 7);

    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {11, 2}
    );
    require(simulation.buildings().back().hit_points == 1500);
    require(simulation.buildings().back().kind ==
            aoe::BuildingKind::guard_tower);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-guard-tower-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::guard_tower
    ));
    require(loaded.maximum_hit_points(loaded.buildings().back()) == 1500);
    require(loaded.melee_armor(loaded.buildings().back()) == 2);
    require(loaded.pierce_armor(loaded.buildings().back()) == 8);
    require(loaded.buildings().back().kind ==
            aoe::BuildingKind::guard_tower);

    const auto make_replay_simulation = [] {
        aoe::Simulation candidate(aoe::GameMap(16, 8));
        candidate.add_building(
            aoe::BuildingKind::university, aoe::Player::blue, {0, 0});
        candidate.add_building(
            aoe::BuildingKind::watch_tower, aoe::Player::blue, {5, 2});
        candidate.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {12, 4});
        candidate.replace_ages(aoe::Age::castle, aoe::Age::dark);
        candidate.replace_state(
            candidate.units(), candidate.buildings(), {250, 100, 0, 0},
            candidate.economy(aoe::Player::red), 0);
        return candidate;
    };
    aoe::Replay replay;
    replay.record(0, aoe::ResearchTechnologyCommand{
        1, aoe::Technology::guard_tower});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-guard-tower-identity.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed = make_replay_simulation();
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        loaded_replay.apply_current_tick(replayed);
        replayed.update();
    }
    require(replayed.buildings()[1].kind ==
            aoe::BuildingKind::guard_tower);
    require(replayed.maximum_hit_points(replayed.buildings()[1]) == 1500);
}

void keep_requires_guard_tower_and_upgrades_tower_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::keep);
    require(technology.researched_at == aoe::BuildingKind::university);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 500);
    require(technology.wood_cost == 350);
    require(technology.research_ticks == 15);

    aoe::Simulation simulation(aoe::GameMap(16, 10));
    const aoe::EntityId university = simulation.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {5, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {10, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 7}
    );
    auto units = simulation.units();
    units[0].stance = aoe::UnitStance::passive;
    simulation.replace_state(
        std::move(units), simulation.buildings(),
        {600, 600, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        university, aoe::Technology::keep
    ));
    require(simulation.economy(aoe::Player::blue).wood == 600);
    require(simulation.economy(aoe::Player::blue).food == 600);
    const aoe::TechnologyRules& guard =
        aoe::rules_for(aoe::Technology::guard_tower);
    require(simulation.research_technology_at(
        university, aoe::Technology::guard_tower
    ));
    for (int tick = 0; tick < guard.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.research_technology_at(
        university, aoe::Technology::keep
    ));
    require(simulation.economy(aoe::Player::blue).wood == 0);
    require(simulation.economy(aoe::Player::blue).food == 0);
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::keep
    ));
    require(simulation.buildings()[1].hit_points == 2250);
    require(simulation.buildings()[1].kind == aoe::BuildingKind::keep);
    require(simulation.maximum_hit_points(simulation.buildings()[1]) == 2250);
    require(simulation.melee_armor(simulation.buildings()[1]) == 3);
    require(simulation.pierce_armor(simulation.buildings()[1]) == 9);
    for (int tick = 0;
         tick < 12 &&
         std::ranges::none_of(
             simulation.projectiles(),
             [](const aoe::Projectile& projectile) {
                 return projectile.damage == 8;
             }
         );
         ++tick) {
        simulation.update();
    }
    require(std::ranges::any_of(
        simulation.projectiles(),
        [](const aoe::Projectile& projectile) {
            return projectile.damage == 8;
        }
    ));

    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {11, 2}
    );
    require(simulation.buildings().back().hit_points == 2250);
    require(simulation.buildings().back().kind == aoe::BuildingKind::keep);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-keep-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::keep
    ));
    require(loaded.maximum_hit_points(loaded.buildings().back()) == 2250);
    require(loaded.melee_armor(loaded.buildings().back()) == 3);
    require(loaded.pierce_armor(loaded.buildings().back()) == 9);
    require(loaded.buildings().back().kind == aoe::BuildingKind::keep);
}

void bodkin_arrow_requires_fletching_and_upgrades_arrow_attacks() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::bodkin_arrow);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 200);
    require(technology.gold_cost == 100);
    require(technology.research_ticks == 7);

    aoe::Simulation simulation(aoe::GameMap(20, 12));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {4, 4}
    );
    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {3, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {8, 7}
    );
    simulation.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {14, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {17, 9}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 300, 150, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::bodkin_arrow
    ));
    const aoe::TechnologyRules& fletching =
        aoe::rules_for(aoe::Technology::fletching);
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::fletching
    ));
    for (int tick = 0; tick < fletching.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::bodkin_arrow
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::bodkin_arrow
    ));
    require(simulation.economy(aoe::Player::blue).food == 0);
    require(simulation.economy(aoe::Player::blue).gold == 0);
    require(simulation.units()[0].attack == 6);
    require(simulation.effective_attack_range(simulation.units()[0]) == 6);
    require(simulation.effective_building_attack(
        simulation.buildings()[1]
    ) == 7);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[1]
    ) == 10);
    require(simulation.effective_building_attack(
        simulation.buildings()[2]
    ) == 7);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[2]
    ) == 6);
    require(simulation.effective_building_attack(
        simulation.buildings()[3]
    ) == 13);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[3]
    ) == 10);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-bodkin-arrow-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::bodkin_arrow
    ));
    require(loaded.units()[0].attack == 6);
    require(loaded.effective_attack_range(loaded.units()[0]) == 6);
    require(loaded.effective_building_attack_range(
        loaded.buildings()[2]
    ) == 6);
}

void bracer_requires_bodkin_and_completes_missile_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::bracer);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 300);
    require(technology.gold_cost == 200);
    require(technology.research_ticks == 8);

    aoe::Simulation simulation(aoe::GameMap(20, 12));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {4, 4}
    );
    simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {3, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {8, 7}
    );
    simulation.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {14, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {17, 9}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 300, 200, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::bracer
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fletching, aoe::Technology::bodkin_arrow}
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::bracer
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::bracer
    ));
    require(simulation.units()[0].attack == 8);
    require(simulation.effective_attack_range(simulation.units()[0]) == 8);
    require(simulation.effective_building_attack(
        simulation.buildings()[1]
    ) == 8);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[1]
    ) == 11);
    require(simulation.effective_building_attack(
        simulation.buildings()[2]
    ) == 8);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[2]
    ) == 6);
    require(simulation.effective_building_attack(
        simulation.buildings()[3]
    ) == 14);
    require(simulation.effective_building_attack_range(
        simulation.buildings()[3]
    ) == 11);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-bracer-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::bracer
    ));
    require(loaded.units()[0].attack == 8);
    require(loaded.effective_attack_range(loaded.units()[0]) == 8);
    require(loaded.effective_building_attack_range(
        loaded.buildings()[2]
    ) == 6);
}

void iron_casting_requires_forging_and_excludes_generic_villagers() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::iron_casting);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 220);
    require(technology.gold_cost == 120);
    require(technology.research_ticks == 15);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 220, 120, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::iron_casting
    ));
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::forging}
    );
    require(simulation.units()[0].attack == 11);
    require(simulation.units()[3].attack == 3);
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::iron_casting
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::iron_casting
    ));
    require(simulation.units()[0].attack == 12);
    require(simulation.units()[1].attack == 8);
    require(simulation.units()[2].attack == 6);
    require(simulation.units()[3].attack == 3);
    require(simulation.units()[4].attack == 4);
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {9, 2}
    );
    require(simulation.units().back().id == scout);
    require(simulation.units().back().attack == 7);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-iron-casting-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::iron_casting
    ));
    require(loaded.units()[0].attack == 12);
    require(loaded.units()[3].attack == 3);
    require(loaded.units().back().attack == 7);
}

void blast_furnace_requires_iron_casting_and_adds_two_attack() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::blast_furnace);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 275);
    require(technology.gold_cost == 225);
    require(technology.research_ticks == 20);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 275, 225, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::blast_furnace
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::forging, aoe::Technology::iron_casting}
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::blast_furnace
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::blast_furnace
    ));
    require(simulation.units()[0].attack == 14);
    require(simulation.units()[1].attack == 13);
    require(simulation.units()[2].attack == 8);
    require(simulation.units()[3].attack == 3);
    require(simulation.units()[4].attack == 4);
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {9, 2}
    );
    require(simulation.units().back().id == scout);
    require(simulation.units().back().attack == 9);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-blast-furnace-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::blast_furnace
    ));
    require(loaded.units()[0].attack == 14);
    require(loaded.units()[3].attack == 3);
    require(loaded.units().back().attack == 9);
}

void scale_mail_armor_upgrades_infantry_and_excludes_other_units() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::scale_mail_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::feudal);
    require(technology.food_cost == 100);
    require(technology.gold_cost == 0);
    require(technology.research_ticks == 8);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    const std::array infantry{
        aoe::UnitKind::militia,
        aoe::UnitKind::man_at_arms,
        aoe::UnitKind::long_swordsman,
        aoe::UnitKind::spearman,
        aoe::UnitKind::pikeman,
    };
    for (std::size_t index = 0; index < infantry.size(); ++index) {
        simulation.add_unit(
            infantry[index], aoe::Player::blue,
            {4 + static_cast<int>(index), 2}
        );
    }
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {9, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {10, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 100, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::scale_mail_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::scale_mail_armor
    ));
    for (std::size_t index = 0; index < infantry.size(); ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 1
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 1
        );
    }
    const aoe::Unit& villager = simulation.units()[infantry.size()];
    require(
        simulation.melee_armor(villager) ==
        aoe::rules_for(villager.kind).melee_armor
    );
    require(
        simulation.pierce_armor(villager) ==
        aoe::rules_for(villager.kind).pierce_armor
    );
    const aoe::Unit& knight = simulation.units()[infantry.size() + 1];
    require(
        simulation.melee_armor(knight) ==
        aoe::rules_for(knight.kind).melee_armor
    );
    require(
        simulation.pierce_armor(knight) ==
        aoe::rules_for(knight.kind).pierce_armor
    );

    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {11, 2}
    );
    const aoe::Unit& future_infantry = simulation.units().back();
    require(
        simulation.melee_armor(future_infantry) ==
        aoe::rules_for(future_infantry.kind).melee_armor + 1
    );
    require(
        simulation.pierce_armor(future_infantry) ==
        aoe::rules_for(future_infantry.kind).pierce_armor + 1
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-scale-mail-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::scale_mail_armor
    ));
    require(
        loaded.melee_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).melee_armor + 1
    );
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 1
    );
}

void chain_mail_armor_requires_scale_mail_and_stacks_in_combat() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::chain_mail_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 200);
    require(technology.gold_cost == 100);
    require(technology.research_ticks == 11);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 200, 100, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::chain_mail_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::scale_mail_armor}
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::chain_mail_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::chain_mail_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 2
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 2
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.melee_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::man_at_arms).melee_armor + 2
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-chain-mail-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::chain_mail_armor
    ));
    require(
        loaded.melee_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).melee_armor + 2
    );

    aoe::Simulation combat(aoe::GameMap(6, 5));
    combat.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {2, 1}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {1, 1}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_mail_armor,
            aoe::Technology::chain_mail_armor,
        }
    );
    require(combat.command_unit(attacker, {2, 1}));
    combat.update();
    require(combat.units().front().hit_points == 37);
}

void plate_mail_armor_requires_chain_mail_and_finishes_infantry_armor() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::plate_mail_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 300);
    require(technology.gold_cost == 150);
    require(technology.research_ticks == 14);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 300, 150, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::plate_mail_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_mail_armor,
            aoe::Technology::chain_mail_armor,
        }
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::plate_mail_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::plate_mail_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 3
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 4
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.melee_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::man_at_arms).melee_armor + 3
    );
    require(
        simulation.pierce_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::man_at_arms).pierce_armor + 4
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-plate-mail-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::plate_mail_armor
    ));
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 4
    );

    aoe::Simulation combat(aoe::GameMap(6, 5));
    combat.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {2, 1}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {1, 1}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_mail_armor,
            aoe::Technology::chain_mail_armor,
            aoe::Technology::plate_mail_armor,
        }
    );
    require(combat.command_unit(attacker, {2, 1}));
    combat.update();
    require(combat.units().front().hit_points == 38);
}

void scale_barding_armor_upgrades_cavalry_and_applies_in_combat() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::scale_barding_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::feudal);
    require(technology.food_cost == 150);
    require(technology.gold_cost == 0);
    require(technology.research_ticks == 9);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {7, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, technology.food_cost, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::scale_barding_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::scale_barding_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 1
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 1
        );
    }
    for (std::size_t index = 2; index < 5; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {9, 2}
    );
    require(
        simulation.melee_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::knight).melee_armor + 1
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-scale-barding-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::scale_barding_armor
    ));
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 1
    );

    aoe::Simulation combat(aoe::GameMap(6, 5));
    combat.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 1}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {1, 1}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::scale_barding_armor}
    );
    const int expected_damage = std::max(
        1,
        combat.units()[1].attack -
            combat.melee_armor(combat.units().front())
    );
    require(combat.command_unit(attacker, {2, 1}));
    combat.update();
    require(
        combat.units().front().hit_points ==
        aoe::rules_for(aoe::UnitKind::scout_cavalry).hit_points -
            expected_damage
    );
}

void chain_barding_armor_requires_scale_barding_and_stacks() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::chain_barding_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 250);
    require(technology.gold_cost == 150);
    require(technology.research_ticks == 12);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 250, 150, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::chain_barding_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::scale_barding_armor}
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::chain_barding_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::chain_barding_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 2
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 2
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.melee_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::knight).melee_armor + 2
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-chain-barding-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::chain_barding_armor
    ));
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 2
    );

    aoe::Simulation combat(aoe::GameMap(6, 5));
    combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {2, 1}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {1, 1}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_barding_armor,
            aoe::Technology::chain_barding_armor,
        }
    );
    require(combat.command_unit(attacker, {2, 1}));
    combat.update();
    require(combat.units().front().hit_points == 94);
}

void plate_barding_armor_requires_chain_and_finishes_cavalry_armor() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::plate_barding_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 350);
    require(technology.gold_cost == 200);
    require(technology.research_ticks == 15);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 350, 200, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::plate_barding_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_barding_armor,
            aoe::Technology::chain_barding_armor,
        }
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::plate_barding_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::plate_barding_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 3
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 4
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.melee_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::knight).melee_armor + 3
    );
    require(
        simulation.pierce_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::knight).pierce_armor + 4
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-plate-barding-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::plate_barding_armor
    ));
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 4
    );

    aoe::Simulation combat(aoe::GameMap(6, 5));
    combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {2, 1}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {1, 1}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::scale_barding_armor,
            aoe::Technology::chain_barding_armor,
            aoe::Technology::plate_barding_armor,
        }
    );
    require(combat.command_unit(attacker, {2, 1}));
    combat.update();
    require(combat.units().front().hit_points == 95);
}

void padded_archer_armor_upgrades_archers_and_reduces_projectile_damage() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::padded_archer_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::feudal);
    require(technology.food_cost == 100);
    require(technology.gold_cost == 0);
    require(technology.research_ticks == 8);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::man_at_arms, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {7, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 100, 0, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::padded_archer_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::padded_archer_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 1
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 1
        );
    }
    for (std::size_t index = 2; index < 5; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {9, 2}
    );
    require(
        simulation.pierce_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::crossbowman).pierce_armor + 1
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-padded-archer-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::padded_archer_armor
    ));
    require(
        loaded.melee_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).melee_armor + 1
    );

    aoe::Simulation combat(aoe::GameMap(8, 5));
    combat.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {4, 2}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {1, 2}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {6, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::padded_archer_armor}
    );
    require(combat.set_unit_stance(
        combat.units().front().id, aoe::UnitStance::passive
    ));
    require(combat.command_unit(attacker, {4, 2}));
    for (int tick = 0; tick < 5; ++tick) {
        combat.update();
    }
    require(combat.units().front().hit_points == 32);
}

void leather_archer_armor_requires_padded_and_stacks_against_arrows() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::leather_archer_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 150);
    require(technology.gold_cost == 150);
    require(technology.research_ticks == 11);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 150, 150, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::leather_archer_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::padded_archer_armor}
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::leather_archer_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::leather_archer_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 2
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 2
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.pierce_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::crossbowman).pierce_armor + 2
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-leather-archer-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::leather_archer_armor
    ));
    require(
        loaded.melee_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).melee_armor + 2
    );

    aoe::Simulation combat(aoe::GameMap(8, 5));
    combat.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {4, 2}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {1, 2}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {6, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::padded_archer_armor,
            aoe::Technology::leather_archer_armor,
        }
    );
    require(combat.set_unit_stance(
        combat.units().front().id, aoe::UnitStance::passive
    ));
    require(combat.command_unit(attacker, {4, 2}));
    for (int tick = 0; tick < 5; ++tick) {
        combat.update();
    }
    require(combat.units().front().hit_points == 33);
}

void ring_archer_armor_requires_leather_and_finishes_archer_armor() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::ring_archer_armor);
    require(technology.researched_at == aoe::BuildingKind::blacksmith);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 250);
    require(technology.gold_cost == 250);
    require(technology.research_ticks == 14);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {7, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 250, 250, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);

    require(!simulation.research_technology_at(
        blacksmith, aoe::Technology::ring_archer_armor
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::padded_archer_armor,
            aoe::Technology::leather_archer_armor,
        }
    );
    require(simulation.research_technology_at(
        blacksmith, aoe::Technology::ring_archer_armor
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::ring_archer_armor
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor + 3
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor + 4
        );
    }
    for (std::size_t index = 2; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(
            simulation.melee_armor(unit) ==
            aoe::rules_for(unit.kind).melee_armor
        );
        require(
            simulation.pierce_armor(unit) ==
            aoe::rules_for(unit.kind).pierce_armor
        );
    }
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {8, 2}
    );
    require(
        simulation.pierce_armor(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::crossbowman).pierce_armor + 4
    );

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-ring-archer-armor-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::ring_archer_armor
    ));
    require(
        loaded.pierce_armor(loaded.units().back()) ==
        aoe::rules_for(loaded.units().back().kind).pierce_armor + 4
    );

    aoe::Simulation combat(aoe::GameMap(8, 5));
    combat.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {4, 2}
    );
    const aoe::EntityId attacker = combat.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {1, 2}
    );
    combat.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {6, 3}
    );
    combat.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::padded_archer_armor,
            aoe::Technology::leather_archer_armor,
            aoe::Technology::ring_archer_armor,
        }
    );
    require(combat.set_unit_stance(
        combat.units().front().id, aoe::UnitStance::passive
    ));
    require(combat.command_unit(attacker, {4, 2}));
    for (int tick = 0; tick < 5; ++tick) {
        combat.update();
    }
    require(combat.units().front().hit_points == 34);
}

void bloodlines_adds_twenty_hit_points_to_current_and_future_cavalry() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::bloodlines);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::feudal);
    require(technology.food_cost == 150);
    require(technology.gold_cost == 100);
    require(technology.research_ticks == 10);

    aoe::Simulation simulation(aoe::GameMap(18, 10));
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {6, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {7, 2}
    );
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 150, 100, 0},
        simulation.economy(aoe::Player::red), 0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.command_unit(attacker, {4, 2}));
    simulation.update();
    require(simulation.units()[0].id == knight);
    require(simulation.units()[0].hit_points == 92);
    require(simulation.set_unit_stance(attacker, aoe::UnitStance::passive));
    require(simulation.stop_unit(attacker));

    require(simulation.research_technology_at(
        stable, aoe::Technology::bloodlines
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }

    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::bloodlines
    ));
    require(simulation.maximum_hit_points(simulation.units()[0]) == 120);
    require(simulation.units()[0].hit_points == 112);
    require(simulation.maximum_hit_points(simulation.units()[1]) == 65);
    require(simulation.units()[1].hit_points == 65);
    require(simulation.maximum_hit_points(simulation.units()[2]) == 30);
    require(simulation.units()[2].hit_points == 30);

    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {8, 2}
    );
    require(simulation.units().back().hit_points == 120);
    require(simulation.maximum_hit_points(simulation.units().back()) == 120);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-bloodlines-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::bloodlines
    ));
    require(loaded.units()[0].hit_points == 112);
    require(loaded.maximum_hit_points(loaded.units()[0]) == 120);
    require(loaded.units().back().hit_points == 120);
}

void husbandry_adds_exact_persisted_cavalry_speed() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::husbandry);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 250);
    require(technology.gold_cost == 0);
    require(technology.research_ticks == 8);

    aoe::Simulation research(aoe::GameMap(12, 6));
    const aoe::EntityId stable = research.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    research.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 3}
    );
    research.replace_state(
        research.units(), research.buildings(),
        {0, technology.food_cost, 0, 0},
        research.economy(aoe::Player::red), 0
    );
    research.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(research.research_technology_at(
        stable, aoe::Technology::husbandry
    ));
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        research.update();
    }
    require(research.has_technology(
        aoe::Player::blue, aoe::Technology::husbandry
    ));

    const auto make_simulation = [](bool husbandry) {
        aoe::Simulation simulation(aoe::GameMap(100, 4));
        simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
        if (husbandry) {
            simulation.replace_technologies(
                aoe::Player::blue,
                {aoe::Technology::husbandry}
            );
        }
        simulation.add_unit(
            aoe::UnitKind::knight, aoe::Player::blue, {1, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {99, 3}
        );
        return simulation;
    };

    aoe::Simulation normal = make_simulation(false);
    aoe::Simulation boosted = make_simulation(true);
    require(normal.command_unit(1, {90, 1}));
    require(boosted.command_unit(1, {90, 1}));
    for (int tick = 0; tick < 17; ++tick) {
        normal.update();
        boosted.update();
    }
    require(normal.units().front().position == aoe::TilePosition(15, 1));
    require(boosted.units().front().position == aoe::TilePosition(16, 1));
    require(boosted.units().front().movement_speed_remainder == 272);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-husbandry-movement.save";
    aoe::save_game(boosted, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().movement_speed_remainder == 272);

    for (int tick = 17; tick < 64; ++tick) {
        normal.update();
        boosted.update();
        loaded.update();
    }
    require(normal.units().front().position == aoe::TilePosition(55, 1));
    require(boosted.units().front().position == aoe::TilePosition(60, 1));
    require(loaded.units().front().position == boosted.units().front().position);
    require(
        loaded.units().front().movement_speed_remainder ==
        boosted.units().front().movement_speed_remainder
    );

    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{1, {90, 1}});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-husbandry-movement.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first = make_simulation(true);
    aoe::Simulation second = make_simulation(true);
    for (int tick = 0; tick < 64; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().front().position == aoe::TilePosition(60, 1));
    require(
        first.units().front().position ==
        second.units().front().position
    );
    require(
        first.units().front().movement_speed_remainder ==
        second.units().front().movement_speed_remainder
    );
}

void cavalier_upgrade_converts_existing_queued_and_future_knights() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::cavalier);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 300);
    require(technology.gold_cost == 300);
    require(technology.research_ticks == 20);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::cavalier);
    require(rules.hit_points == 120);
    require(rules.attack == 12);
    require(rules.melee_armor == 2);
    require(rules.pierce_armor == 2);
    require(rules.food_cost == 60);
    require(rules.gold_cost == 75);
    require(rules.training_ticks == 12);
    require(rules.vision_range == 4);
    require(rules.minimum_age == aoe::Age::imperial);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 300, 300, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.research_technology_at(
        stable, aoe::Technology::cavalier
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 93;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::knight,
        7,
        0,
        60,
        75,
    });
    simulation.replace_state(
        std::move(units),
        std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );

    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::cavalier
    ));
    require(simulation.units().front().kind == aoe::UnitKind::cavalier);
    require(simulation.units().front().hit_points == 113);
    require(simulation.units().front().attack == 12);
    require(
        simulation.buildings().front().production_queue.front().kind ==
        aoe::UnitKind::cavalier
    );
    require(
        simulation.buildings().front().production_queue.front().paid_food ==
        60
    );

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 60, 75, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(stable, aoe::UnitKind::knight));
    require(
        simulation.buildings().front().production_queue.back().kind ==
        aoe::UnitKind::cavalier
    );

    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::cavalier,
            aoe::Technology::bloodlines,
            aoe::Technology::forging,
            aoe::Technology::iron_casting,
            aoe::Technology::blast_furnace,
            aoe::Technology::scale_barding_armor,
            aoe::Technology::chain_barding_armor,
            aoe::Technology::plate_barding_armor,
            aoe::Technology::husbandry,
        }
    );
    require(simulation.maximum_hit_points(
        simulation.units().front()
    ) == 140);
    require(simulation.units().front().attack == 16);
    require(simulation.melee_armor(simulation.units().front()) == 5);
    require(simulation.pierce_armor(simulation.units().front()) == 6);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-cavalier-upgrade.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::cavalier
    ));
    require(loaded.units().front().kind == aoe::UnitKind::cavalier);
    require(
        loaded.buildings().front().production_queue.back().kind ==
        aoe::UnitKind::cavalier
    );

    const auto make_replay_simulation = [] {
        aoe::Simulation candidate(aoe::GameMap(16, 6));
        candidate.add_building(
            aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
        );
        candidate.add_unit(
            aoe::UnitKind::knight, aoe::Player::blue, {5, 2}
        );
        candidate.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {13, 3}
        );
        candidate.replace_ages(aoe::Age::imperial, aoe::Age::dark);
        candidate.replace_state(
            candidate.units(),
            candidate.buildings(),
            {0, 300, 300, 0},
            candidate.economy(aoe::Player::red),
            0
        );
        return candidate;
    };
    aoe::Replay replay;
    replay.record(
        0,
        aoe::ResearchTechnologyCommand{
            1,
            aoe::Technology::cavalier,
        }
    );
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-cavalier-upgrade.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first = make_replay_simulation();
    aoe::Simulation second = make_replay_simulation();
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().front().kind == aoe::UnitKind::cavalier);
    require(second.units().front().kind == aoe::UnitKind::cavalier);
    require(first.units().front().hit_points ==
            second.units().front().hit_points);
    require(first.units().front().attack ==
            second.units().front().attack);
    require(first.economy(aoe::Player::blue).food ==
            second.economy(aoe::Player::blue).food);
    require(first.tick_number() == second.tick_number());
}

void light_cavalry_upgrade_converts_scout_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::light_cavalry);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.food_cost == 150);
    require(technology.gold_cost == 50);
    require(technology.research_ticks == 9);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::light_cavalry);
    require(rules.hit_points == 60);
    require(rules.attack == 7);
    require(rules.melee_armor == 0);
    require(rules.pierce_armor == 2);
    require(rules.vision_range == 8);
    require(rules.food_cost == 80);
    require(rules.gold_cost == 0);
    require(rules.training_ticks == 6);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 150, 50, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(simulation.buildings().front().completed());
    require(simulation.buildings().front().kind ==
            aoe::BuildingKind::stable);
    require(simulation.buildings().front().production_queue.empty());
    require(simulation.buildings().front().age_research_ticks_remaining == 0);
    require(simulation.buildings().front()
                .technology_research_ticks_remaining == 0);
    require(simulation.age(aoe::Player::blue) == aoe::Age::castle);
    require(!simulation.has_technology(
        aoe::Player::blue, aoe::Technology::light_cavalry
    ));
    require(simulation.economy(aoe::Player::blue).food == 150);
    require(simulation.economy(aoe::Player::blue).gold == 50);
    require(simulation.research_technology_at(
        stable, aoe::Technology::light_cavalry
    ));
    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 40;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::scout_cavalry, 4, 0, 80, 0,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::light_cavalry
    ));
    require(simulation.units().front().kind ==
            aoe::UnitKind::light_cavalry);
    require(simulation.units().front().hit_points == 55);
    require(simulation.units().front().attack == 7);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::light_cavalry);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 80, 0, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        stable, aoe::UnitKind::scout_cavalry
    ));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::light_cavalry);
}

void two_handed_swordsman_finishes_imperial_infantry_step() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::two_handed_swordsman);
    require(technology.researched_at == aoe::BuildingKind::barracks);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 300);
    require(technology.gold_cost == 100);
    require(technology.research_ticks == 15);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::two_handed_swordsman);
    require(rules.hit_points == 60);
    require(rules.attack == 11);
    require(rules.melee_armor == 0);
    require(rules.pierce_armor == 1);
    require(rules.bonus_vs_buildings == 4);
    require(rules.food_cost == 60);
    require(rules.gold_cost == 20);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 300, 100, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(!simulation.research_technology_at(
        barracks, aoe::Technology::two_handed_swordsman
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::man_at_arms,
            aoe::Technology::long_swordsman,
        }
    );
    require(simulation.research_technology_at(
        barracks, aoe::Technology::two_handed_swordsman
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 45;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::long_swordsman, 4, 0, 60, 20,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().kind ==
            aoe::UnitKind::two_handed_swordsman);
    require(simulation.units().front().hit_points == 50);
    require(simulation.units().front().attack == 11);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::two_handed_swordsman);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 60, 20, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        barracks, aoe::UnitKind::militia
    ));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::two_handed_swordsman);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-two-handed-swordsman.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::two_handed_swordsman
    ));
    require(loaded.units().front().kind ==
            aoe::UnitKind::two_handed_swordsman);
}

void champion_completes_classic_militia_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::champion);
    require(technology.researched_at == aoe::BuildingKind::barracks);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 750);
    require(technology.gold_cost == 350);
    require(technology.research_ticks == 20);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::champion);
    require(rules.hit_points == 70);
    require(rules.attack == 13);
    require(rules.melee_armor == 1);
    require(rules.pierce_armor == 1);
    require(rules.bonus_vs_buildings == 4);
    require(rules.food_cost == 60);
    require(rules.gold_cost == 20);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::two_handed_swordsman,
        aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 750, 350, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(!simulation.research_technology_at(
        barracks, aoe::Technology::champion
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::man_at_arms,
            aoe::Technology::long_swordsman,
            aoe::Technology::two_handed_swordsman,
        }
    );
    require(simulation.research_technology_at(
        barracks, aoe::Technology::champion
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 50;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::two_handed_swordsman, 4, 0, 60, 20,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().kind == aoe::UnitKind::champion);
    require(simulation.units().front().hit_points == 60);
    require(simulation.units().front().attack == 13);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::champion);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 60, 20, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        barracks, aoe::UnitKind::militia
    ));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::champion);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-champion-upgrade.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::champion
    ));
    require(loaded.units().front().kind == aoe::UnitKind::champion);
}

void arbalester_completes_classic_foot_archer_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::arbalester);
    require(technology.researched_at ==
            aoe::BuildingKind::archery_range);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 350);
    require(technology.gold_cost == 300);
    require(technology.research_ticks == 10);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::arbalester);
    require(rules.hit_points == 40);
    require(rules.attack == 6);
    require(rules.attack_range == 5);
    require(rules.wood_cost == 25);
    require(rules.gold_cost == 45);
    require(rules.training_ticks == 11);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::crossbowman, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 350, 300, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(!simulation.research_technology_at(
        range, aoe::Technology::arbalester
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::crossbowman}
    );
    require(simulation.research_technology_at(
        range, aoe::Technology::arbalester
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 30;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::crossbowman, 4, 25, 0, 45,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().kind == aoe::UnitKind::arbalester);
    require(simulation.units().front().hit_points == 35);
    require(simulation.units().front().attack == 6);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::arbalester);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {25, 0, 45, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(range, aoe::UnitKind::archer));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::arbalester);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-arbalester-upgrade.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::arbalester
    ));
    require(loaded.units().front().kind == aoe::UnitKind::arbalester);
}

void elite_skirmisher_completes_castle_counter_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::elite_skirmisher);
    require(technology.researched_at ==
            aoe::BuildingKind::archery_range);
    require(technology.minimum_age == aoe::Age::castle);
    require(technology.wood_cost == 250);
    require(technology.gold_cost == 160);
    require(technology.research_ticks == 10);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::elite_skirmisher);
    require(rules.hit_points == 35);
    require(rules.attack == 3);
    require(rules.attack_range == 5);
    require(rules.minimum_attack_range == 1);
    require(rules.pierce_armor == 4);
    require(rules.bonus_vs_archers == 4);
    require(rules.bonus_vs_spearmen == 3);
    require(rules.food_cost == 25);
    require(rules.wood_cost == 35);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::skirmisher, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {technology.wood_cost, 0, technology.gold_cost, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(simulation.research_technology_at(
        range, aoe::Technology::elite_skirmisher
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 25;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::skirmisher, 4, 35, 25, 0,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().kind ==
            aoe::UnitKind::elite_skirmisher);
    require(simulation.units().front().hit_points == 30);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::elite_skirmisher);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {35, 25, 0, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        range, aoe::UnitKind::skirmisher
    ));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::elite_skirmisher);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-elite-skirmisher.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::elite_skirmisher
    ));
    require(loaded.units().front().kind ==
            aoe::UnitKind::elite_skirmisher);

    aoe::Simulation counter(aoe::GameMap(10, 6));
    const aoe::EntityId elite = counter.add_unit(
        aoe::UnitKind::elite_skirmisher,
        aoe::Player::blue, {1, 2}
    );
    counter.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::red, {3, 2}
    );
    counter.replace_technologies(
        aoe::Player::blue, {aoe::Technology::ballistics}
    );
    require(counter.command_unit(elite, {3, 2}));
    for (int tick = 0; tick < 20 &&
         counter.units()[1].hit_points ==
             aoe::rules_for(aoe::UnitKind::pikeman).hit_points;
         ++tick) {
        counter.update();
    }
    require(counter.units()[1].hit_points == 49);
}

void hussar_requires_light_cavalry_and_finishes_scout_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::hussar);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 500);
    require(technology.gold_cost == 600);
    require(technology.research_ticks == 10);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::hussar);
    require(rules.hit_points == 75);
    require(rules.attack == 7);
    require(rules.melee_armor == 0);
    require(rules.pierce_armor == 2);
    require(rules.vision_range == 10);
    require(rules.food_cost == 80);
    require(rules.gold_cost == 0);
    require(rules.training_ticks == 6);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::light_cavalry, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 500, 600, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(!simulation.research_technology_at(
        stable, aoe::Technology::hussar
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::light_cavalry}
    );
    require(simulation.research_technology_at(
        stable, aoe::Technology::hussar
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 50;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::light_cavalry, 4, 0, 80, 0,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::hussar
    ));
    require(simulation.units().front().kind == aoe::UnitKind::hussar);
    require(simulation.units().front().hit_points == 65);
    require(simulation.units().front().attack == 7);
    require(simulation.buildings().front().production_queue.front().kind ==
            aoe::UnitKind::hussar);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 80, 0, 0}, simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        stable, aoe::UnitKind::scout_cavalry
    ));
    require(simulation.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::hussar);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-hussar-upgrade.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::hussar
    ));
    require(loaded.units().front().kind == aoe::UnitKind::hussar);
    require(loaded.buildings().front().production_queue.back().kind ==
            aoe::UnitKind::hussar);
}

void paladin_requires_cavalier_and_finishes_heavy_cavalry_line() {
    const aoe::TechnologyRules& technology =
        aoe::rules_for(aoe::Technology::paladin);
    require(technology.researched_at == aoe::BuildingKind::stable);
    require(technology.minimum_age == aoe::Age::imperial);
    require(technology.food_cost == 1300);
    require(technology.gold_cost == 750);
    require(technology.research_ticks == 34);

    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::paladin);
    require(rules.hit_points == 160);
    require(rules.attack == 14);
    require(rules.melee_armor == 2);
    require(rules.pierce_armor == 3);
    require(rules.vision_range == 5);
    require(rules.food_cost == 60);
    require(rules.gold_cost == 75);
    require(rules.training_ticks == 12);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {3, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::cavalier, aoe::Player::blue, {5, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 5}
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 1300, 750, 0},
        simulation.economy(aoe::Player::red), 0
    );
    require(!simulation.research_technology_at(
        stable, aoe::Technology::paladin
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::cavalier}
    );
    require(simulation.research_technology_at(
        stable, aoe::Technology::paladin
    ));

    std::vector<aoe::Unit> units = simulation.units();
    units.front().hit_points = 111;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().production_queue.push_back({
        aoe::UnitKind::cavalier, 9, 0, 60, 75,
    });
    simulation.replace_state(
        std::move(units), std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    for (int tick = 0; tick < technology.research_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::paladin
    ));
    require(simulation.units().front().kind == aoe::UnitKind::paladin);
    require(simulation.units().front().hit_points == 151);
    require(simulation.units().front().attack == 14);
    require(
        simulation.buildings().front().production_queue.front().kind ==
        aoe::UnitKind::paladin
    );

    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {0, 60, 75, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(stable, aoe::UnitKind::knight));
    require(
        simulation.buildings().front().production_queue.back().kind ==
        aoe::UnitKind::paladin
    );
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::cavalier,
            aoe::Technology::paladin,
            aoe::Technology::bloodlines,
            aoe::Technology::forging,
            aoe::Technology::iron_casting,
            aoe::Technology::blast_furnace,
            aoe::Technology::scale_barding_armor,
            aoe::Technology::chain_barding_armor,
            aoe::Technology::plate_barding_armor,
            aoe::Technology::husbandry,
        }
    );
    require(simulation.maximum_hit_points(
        simulation.units().front()
    ) == 180);
    require(simulation.units().front().attack == 18);
    require(simulation.melee_armor(simulation.units().front()) == 5);
    require(simulation.pierce_armor(simulation.units().front()) == 7);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-paladin-upgrade.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::paladin
    ));
    require(loaded.units().front().kind == aoe::UnitKind::paladin);
    require(
        loaded.buildings().front().production_queue.back().kind ==
        aoe::UnitKind::paladin
    );

    aoe::Replay replay;
    replay.record(
        0,
        aoe::ResearchTechnologyCommand{
            stable, aoe::Technology::paladin,
        }
    );
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-paladin-upgrade.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(loaded_replay.commands().size() == replay.commands().size());
}

void path_routes_through_river_crossing() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.command_selected({14, 7}));

    for (int tick = 0; tick < 24; ++tick) {
        simulation.update();
    }

    require(simulation.units().front().position == aoe::TilePosition(14, 7));
}

void save_round_trip_preserves_state() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.command_selected({3, 5}));
    for (int tick = 0; tick < 7; ++tick) {
        simulation.update();
    }
    require(simulation.select_building_at({0, 10}, aoe::Player::blue));
    require(simulation.queue_unit(aoe::UnitKind::villager));

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-archaeology-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);

    require(loaded.tick_number() == simulation.tick_number());
    require(loaded.units().size() == simulation.units().size());
    require(loaded.buildings().size() == simulation.buildings().size());
    require(
        loaded.buildings().front().production_queue.front().ticks_remaining ==
        simulation.buildings().front().production_queue.front().ticks_remaining
    );
    require(
        loaded.economy(aoe::Player::blue).wood ==
        simulation.economy(aoe::Player::blue).wood
    );
    require(
        loaded.economy(aoe::Player::blue).gold ==
        simulation.economy(aoe::Player::blue).gold
    );
    require(
        loaded.economy(aoe::Player::blue).stone ==
        simulation.economy(aoe::Player::blue).stone
    );
    require(
        loaded.map().resource_amount_at({3, 5}) ==
        simulation.map().resource_amount_at({3, 5})
    );
    require(
        loaded.units().front().carried_amount ==
        simulation.units().front().carried_amount
    );
    require(
        loaded.units().front().carried_resource ==
        simulation.units().front().carried_resource
    );
    require(loaded.units().front().has_resource_target);

    for (int tick = 0; tick < 30; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(
        loaded.economy(aoe::Player::blue).wood ==
        simulation.economy(aoe::Player::blue).wood
    );
    require(
        loaded.map().resource_amount_at({3, 5}) ==
        simulation.map().resource_amount_at({3, 5})
    );
}

void town_center_produces_villager() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    require(simulation.select_building_at({0, 10}, aoe::Player::blue));
    require(simulation.queue_unit(aoe::UnitKind::villager));
    require(!simulation.queue_unit(aoe::UnitKind::knight));
    require(simulation.economy(aoe::Player::blue).food == 150);

    const std::size_t original_units = simulation.units().size();
    for (int tick = 0; tick < 124; ++tick) {
        simulation.update();
    }
    require(simulation.units().size() == original_units);
    simulation.update();
    require(simulation.units().size() == original_units + 1);
    require((
        simulation.units().back().position == aoe::TilePosition{1, 14}
    ));
}

void completed_villager_order_retries_once_on_valid_land() {
    aoe::GameMap map(12, 10);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {4, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {9, 7}
    );

    constexpr std::array<aoe::TilePosition, 16> perimeter{{
        {4, 2}, {5, 2}, {6, 2}, {7, 2},
        {8, 3}, {8, 4}, {8, 5}, {8, 6},
        {7, 7}, {6, 7}, {5, 7}, {4, 7},
        {3, 6}, {3, 5}, {3, 4}, {3, 3},
    }};
    for (const aoe::TilePosition position : perimeter) {
        simulation.add_unit(
            aoe::UnitKind::deer, aoe::Player::neutral, position
        );
    }

    const int food_before = simulation.economy(aoe::Player::red).food;
    require(simulation.queue_unit_at(
        town_center, aoe::UnitKind::villager
    ));
    const int food_after_queue =
        simulation.economy(aoe::Player::red).food;
    require(food_after_queue < food_before);
    for (int tick = 0; tick < 100; ++tick) simulation.update();

    const auto blocked_building = std::ranges::find(
        simulation.buildings(), town_center, &aoe::Building::id
    );
    require(blocked_building != simulation.buildings().end());
    require(blocked_building->production_queue.size() == 1);
    require(
        blocked_building->production_queue.front().ticks_remaining == 0
    );
    require(
        simulation.economy(aoe::Player::red).food == food_after_queue
    );
    require(std::ranges::none_of(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::villager;
        }
    ));

    auto units = simulation.units();
    units.erase(units.begin());
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    simulation.update();

    const auto villagers = std::ranges::count_if(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::villager &&
                unit.owner == aoe::Player::red;
        }
    );
    require(villagers == 1);
    require(std::ranges::any_of(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::villager &&
                unit.position == aoe::TilePosition{4, 2};
        }
    ));
    require(simulation.buildings().front().production_queue.empty());
    require(
        simulation.economy(aoe::Player::red).food == food_after_queue
    );
    for (int tick = 0; tick < 20; ++tick) simulation.update();
    require(std::ranges::count_if(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::villager;
        }
    ) == 1);
}

void villagers_repair_buildings_with_persistent_costs() {
    aoe::Simulation simulation(aoe::GameMap(14, 10));
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId house = simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {2, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {10, 6}
    );
    auto damaged = simulation.buildings();
    damaged.front().hit_points =
        aoe::rules_for(aoe::BuildingKind::house).hit_points - 100;
    simulation.replace_state(
        simulation.units(),
        std::move(damaged),
        {100, 500, 500, 500},
        simulation.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replay_first = simulation;
    aoe::Simulation replay_second = simulation;
    aoe::Replay recorded;
    recorded.record(0, aoe::MoveUnitCommand{villager, {2, 1}});
    recorded.apply_current_tick(replay_first);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-repair-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    replayed.apply_current_tick(replay_second);
    for (int tick = 0; tick < 3; ++tick) {
        replay_first.update();
        replay_second.update();
    }
    require(
        replay_first.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points - 70
    );
    require(
        replay_second.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points - 70
    );
    require(
        replay_first.economy(aoe::Player::blue).wood ==
        replay_second.economy(aoe::Player::blue).wood
    );

    require(simulation.command_unit(villager, {2, 1}));
    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
    }
    require(
        simulation.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points - 70
    );
    require(simulation.units().front().repair_target_id == house);
    require(
        simulation.units().front().repair_wood_remainder ==
        (30 * aoe::rules_for(aoe::BuildingKind::house).wood_cost) %
            (2 * aoe::rules_for(aoe::BuildingKind::house).hit_points)
    );

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-repair-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().repair_target_id == house);
    require(
        loaded.units().front().repair_wood_remainder ==
        (30 * aoe::rules_for(aoe::BuildingKind::house).wood_cost) %
            (2 * aoe::rules_for(aoe::BuildingKind::house).hit_points)
    );
    for (int tick = 0; tick < 7; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(
        simulation.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points
    );
    require(
        loaded.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points
    );
    require(simulation.economy(aoe::Player::blue).wood == 99);
    require(loaded.economy(aoe::Player::blue).wood == 99);
    require(simulation.units().front().repair_target_id == 0);

    aoe::Simulation stalled(aoe::GameMap(14, 10));
    const aoe::EntityId repairer = stalled.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    stalled.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {2, 1}
    );
    stalled.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {10, 6}
    );
    damaged = stalled.buildings();
    damaged.front().hit_points = 100;
    stalled.replace_state(
        stalled.units(),
        std::move(damaged),
        {0, 500, 500, 500},
        stalled.economy(aoe::Player::red),
        0
    );
    require(stalled.command_unit(repairer, {2, 1}));
    stalled.update();
    require(stalled.buildings().front().hit_points == 100);
    require(stalled.command_unit(repairer, {4, 1}));
    require(stalled.units().front().repair_target_id == 0);

    aoe::Simulation stone_repair(aoe::GameMap(16, 12));
    const aoe::EntityId mason = stone_repair.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {8, 5}
    );
    stone_repair.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    stone_repair.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {12, 8}
    );
    damaged = stone_repair.buildings();
    damaged.front().hit_points = 4700;
    stone_repair.replace_state(
        stone_repair.units(),
        std::move(damaged),
        {0, 500, 500, 10},
        stone_repair.economy(aoe::Player::red),
        0
    );
    require(stone_repair.command_unit(mason, {7, 7}));
    stone_repair.update();
    require(stone_repair.buildings().front().hit_points == 4710);
    require(stone_repair.units().front().repair_target_id != 0);
}

void palisade_walls_construct_block_persist_and_fall() {
    const aoe::BuildingRules& rules =
        aoe::rules_for(aoe::BuildingKind::palisade_wall);
    require(rules.hit_points == 250);
    require(rules.melee_armor == 2);
    require(rules.pierce_armor == 5);
    require(rules.wood_cost == 2);
    require(rules.minimum_age == aoe::Age::dark);
    require(rules.footprint_width == 1);
    require(rules.footprint_height == 1);

    aoe::Simulation construction(aoe::GameMap(10, 6));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    construction.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {6, 2}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {10, 0, 0, 0},
        construction.economy(aoe::Player::red),
        0
    );

    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::palisade_wall,
            {2, 1},
        }
    );
    recorded.apply_current_tick(construction);
    require(
        construction.economy(aoe::Player::blue).wood ==
        10 - rules.wood_cost
    );
    require(construction.buildings().back().kind ==
            aoe::BuildingKind::palisade_wall);
    require(!construction.buildings().back().completed());

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-palisade-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Simulation replayed(aoe::GameMap(10, 6));
    const aoe::EntityId replay_builder = replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    require(replay_builder == builder);
    replayed.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {6, 2}
    );
    replayed.replace_state(
        replayed.units(),
        replayed.buildings(),
        {10, 0, 0, 0},
        replayed.economy(aoe::Player::red),
        0
    );
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(replayed.buildings().back().kind ==
            aoe::BuildingKind::palisade_wall);

    for (int tick = 0; tick < rules.construction_ticks; ++tick) {
        construction.update();
        replayed.update();
    }
    require(construction.buildings().back().completed());
    require(replayed.buildings().back().completed());

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-palisade-test.save";
    aoe::save_game(construction, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().back().kind ==
            aoe::BuildingKind::palisade_wall);
    require(loaded.buildings().back().hit_points == rules.hit_points);

    aoe::GameMap corridor(7, 3);
    for (int x = 0; x < corridor.width(); ++x) {
        corridor.set_terrain({x, 0}, aoe::Terrain::water);
        corridor.set_terrain({x, 2}, aoe::Terrain::water);
    }
    aoe::Simulation siege(std::move(corridor));
    const aoe::EntityId infantry = siege.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::blue,
        {1, 1}
    );
    siege.add_building(
        aoe::BuildingKind::palisade_wall,
        aoe::Player::blue,
        {3, 1}
    );
    const aoe::EntityId ram = siege.add_unit(
        aoe::UnitKind::battering_ram,
        aoe::Player::red,
        {4, 1}
    );
    require(!siege.command_unit(infantry, {5, 1}));
    require(siege.command_unit(ram, {3, 1}));
    for (int tick = 0; tick < 12; ++tick) {
        siege.update();
    }
    require(siege.buildings().empty());
    require(siege.command_unit(infantry, {3, 1}));
}

void palisade_gates_open_for_friendlies_block_enemies_and_persist() {
    const aoe::BuildingRules& along_x =
        aoe::rules_for(aoe::BuildingKind::palisade_gate_x);
    const aoe::BuildingRules& along_y =
        aoe::rules_for(aoe::BuildingKind::palisade_gate_y);
    require(along_x.hit_points == 600);
    require(along_x.melee_armor == 2);
    require(along_x.pierce_armor == 6);
    require(along_x.wood_cost == 30);
    require(along_x.minimum_age == aoe::Age::dark);
    require(along_x.footprint_width == 4);
    require(along_x.footprint_height == 1);
    require(along_y.footprint_width == 1);
    require(along_y.footprint_height == 4);

    aoe::Simulation construction(aoe::GameMap(12, 7));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    construction.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 4}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {30, 0, 0, 0},
        construction.economy(aoe::Player::red),
        0
    );
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::palisade_gate_x,
            {2, 2},
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-palisade-gate-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    replayed.apply_current_tick(construction);
    require(construction.economy(aoe::Player::blue).wood == 0);
    require(construction.buildings().back().kind ==
            aoe::BuildingKind::palisade_gate_x);

    aoe::Simulation passage(aoe::GameMap(12, 8));
    for (int y : {0, 1, 6, 7}) {
        passage.add_building(
            aoe::BuildingKind::palisade_wall,
            aoe::Player::blue,
            {5, y}
        );
    }
    const aoe::EntityId gate = passage.add_building(
        aoe::BuildingKind::palisade_gate_y,
        aoe::Player::blue,
        {5, 2}
    );
    const aoe::EntityId friendly = passage.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 3}
    );
    const aoe::EntityId enemy = passage.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {9, 3}
    );
    require(passage.set_unit_stance(enemy, aoe::UnitStance::passive));
    require(passage.command_unit(friendly, {8, 3}));

    bool saw_open = false;
    bool saved_open = false;
    for (int tick = 0; tick < 20; ++tick) {
        passage.update();
        const auto found = std::ranges::find_if(
            passage.buildings(),
            [gate](const aoe::Building& building) {
                return building.id == gate;
            }
        );
        require(found != passage.buildings().end());
        saw_open = saw_open || found->gate_open;
        if (found->gate_open && !saved_open) {
            const auto save_path =
                std::filesystem::temp_directory_path() /
                "aoe-open-palisade-gate.save";
            aoe::save_game(passage, save_path);
            aoe::Simulation loaded = aoe::load_game(save_path);
            std::filesystem::remove(save_path);
            const auto loaded_gate = std::ranges::find_if(
                loaded.buildings(),
                [gate](const aoe::Building& building) {
                    return building.id == gate;
                }
            );
            require(loaded_gate != loaded.buildings().end());
            require(loaded_gate->gate_open);
            saved_open = true;
        }
    }
    require(saw_open);
    require(saved_open);
    require(passage.units().front().position == aoe::TilePosition(8, 3));
    passage.update();
    const auto closed_gate = std::ranges::find_if(
        passage.buildings(),
        [gate](const aoe::Building& building) {
            return building.id == gate;
        }
    );
    require(closed_gate != passage.buildings().end());
    require(!closed_gate->gate_open);
    require(!passage.command_unit(enemy, {2, 3}));

    require(passage.command_unit(enemy, {5, 3}));
    for (int tick = 0; tick < 12; ++tick) {
        passage.update();
    }
    const auto damaged_gate = std::ranges::find_if(
        passage.buildings(),
        [gate](const aoe::Building& building) {
            return building.id == gate;
        }
    );
    require(damaged_gate != passage.buildings().end());
    require(damaged_gate->hit_points < along_y.hit_points);
}

void gates_honor_allies_locks_and_enemy_exclusion() {
    const auto blue = *aoe::PlayerSlotId::from_index(0);
    const auto red = *aoe::PlayerSlotId::from_index(1);
    const auto green = *aoe::PlayerSlotId::from_index(2);
    const auto roster = aoe::MatchRoster::create({
        {blue, true, aoe::TeamId::none(), false,
         {{"blue", aoe::RosterControllerKind::human}}},
        {red, true, aoe::TeamId::none(), false,
         {{"red", aoe::RosterControllerKind::computer}}},
        {green, true, aoe::TeamId::none(), false,
         {{"green", aoe::RosterControllerKind::computer}}},
    });
    require(roster.has_value());
    auto diplomacy = *aoe::RosterDiplomacy::create(*roster);
    require(diplomacy.set_symmetric_stance(
        blue, green, aoe::Diplomacy::ally
    ));

    struct GateFixture {
        aoe::GameMap map;
        aoe::TilePosition gate;
        aoe::TilePosition start;
        aoe::TilePosition destination;
        aoe::TilePosition opener;
        aoe::TilePosition enemy_anchor;
    };
    const auto fixture_for = [](aoe::BuildingKind kind) {
        const bool along_x =
            kind == aoe::BuildingKind::palisade_gate_x ||
            kind == aoe::BuildingKind::stone_gate_x ||
            kind == aoe::BuildingKind::fortified_gate_x;
        if (along_x) {
            aoe::GameMap map(12, 7);
            for (int x = 0; x < map.width(); ++x) {
                map.set_terrain({x, 2}, aoe::Terrain::water);
                map.set_terrain({x, 4}, aoe::Terrain::water);
            }
            map.set_terrain({5, 2}, aoe::Terrain::grass);
            return GateFixture{
                std::move(map), {4, 3}, {1, 3}, {10, 3}, {5, 2}, {9, 0}
            };
        }
        aoe::GameMap map(7, 12);
        for (int y = 0; y < map.height(); ++y) {
            map.set_terrain({2, y}, aoe::Terrain::water);
            map.set_terrain({4, y}, aoe::Terrain::water);
        }
        map.set_terrain({2, 5}, aoe::Terrain::grass);
        return GateFixture{
            std::move(map), {3, 4}, {3, 1}, {3, 10}, {2, 5}, {0, 9}
        };
    };

    const std::array gate_kinds{
        aoe::BuildingKind::palisade_gate_x,
        aoe::BuildingKind::palisade_gate_y,
        aoe::BuildingKind::stone_gate_x,
        aoe::BuildingKind::stone_gate_y,
        aoe::BuildingKind::fortified_gate_x,
        aoe::BuildingKind::fortified_gate_y,
    };
    const auto add_anchor = [red](
        aoe::Simulation& simulation, aoe::TilePosition position
    ) {
        simulation.add_building(
            aoe::BuildingKind::house, red, position
        );
    };
    const auto gate_by_id = [](const aoe::Simulation& simulation,
                               aoe::EntityId id) -> const aoe::Building& {
        const auto found = std::ranges::find(
            simulation.buildings(), id, &aoe::Building::id
        );
        require(found != simulation.buildings().end());
        return *found;
    };
    const auto unit_by_id = [](const aoe::Simulation& simulation,
                               aoe::EntityId id) -> const aoe::Unit& {
        const auto found = std::ranges::find(
            simulation.units(), id, &aoe::Unit::id
        );
        require(found != simulation.units().end());
        return *found;
    };

    for (aoe::BuildingKind kind : gate_kinds) {
        for (aoe::PlayerSlotId admitted : {blue, green}) {
            GateFixture fixture = fixture_for(kind);
            aoe::Simulation passage(std::move(fixture.map));
            passage.replace_roster(*roster, diplomacy);
            add_anchor(passage, fixture.enemy_anchor);
            const aoe::EntityId gate = passage.add_building(
                kind, blue, fixture.gate
            );
            const aoe::EntityId unit = passage.add_unit(
                aoe::UnitKind::villager, admitted, fixture.start
            );
            require(passage.set_unit_stance(
                unit, aoe::UnitStance::passive
            ));
            require(passage.command_unit(unit, fixture.destination));
            bool opened = false;
            for (int tick = 0; tick < 80 &&
                 unit_by_id(passage, unit).position != fixture.destination;
                 ++tick) {
                passage.update();
                opened = opened || gate_by_id(passage, gate).gate_open;
            }
            require(opened);
            require(unit_by_id(passage, unit).position == fixture.destination);
        }

        for (aoe::EntityOwner excluded : {
                 aoe::entity_owner_from_slot(red),
                 aoe::EntityOwner{aoe::Player::neutral}
             }) {
            GateFixture fixture = fixture_for(kind);
            aoe::Simulation passage(std::move(fixture.map));
            passage.replace_roster(*roster, diplomacy);
            add_anchor(passage, fixture.enemy_anchor);
            passage.add_building(kind, blue, fixture.gate);
            const aoe::EntityId unit = passage.add_unit(
                aoe::UnitKind::militia, excluded, fixture.start
            );
            require(!passage.command_unit(unit, fixture.destination));
        }

        GateFixture open_fixture = fixture_for(kind);
        aoe::Simulation opened(std::move(open_fixture.map));
        opened.replace_roster(*roster, diplomacy);
        add_anchor(opened, open_fixture.enemy_anchor);
        const aoe::EntityId open_gate = opened.add_building(
            kind, blue, open_fixture.gate
        );
        opened.add_unit(
            aoe::UnitKind::villager, blue, open_fixture.opener
        );
        opened.update();
        require(gate_by_id(opened, open_gate).gate_open);
        const aoe::EntityId enemy = opened.add_unit(
            aoe::UnitKind::militia, red, open_fixture.start
        );
        require(opened.set_unit_stance(enemy, aoe::UnitStance::passive));
        require(!opened.command_unit(enemy, open_fixture.destination));

        GateFixture occupied_fixture = fixture_for(kind);
        aoe::Simulation occupied(std::move(occupied_fixture.map));
        occupied.replace_roster(*roster, diplomacy);
        add_anchor(occupied, occupied_fixture.enemy_anchor);
        const aoe::EntityId occupied_gate = occupied.add_building(
            kind, blue, occupied_fixture.gate
        );
        occupied.add_unit(
            aoe::UnitKind::militia, red, occupied_fixture.start
        );
        std::vector<aoe::Unit> units = occupied.units();
        units.front().position = occupied_fixture.gate;
        occupied.replace_state(
            std::move(units), occupied.buildings(),
            occupied.economy(blue), occupied.economy(red), 0
        );
        occupied.update();
        require(!gate_by_id(occupied, occupied_gate).gate_open);

        GateFixture lock_fixture = fixture_for(kind);
        aoe::Simulation locked(std::move(lock_fixture.map));
        locked.replace_roster(*roster, diplomacy);
        add_anchor(locked, lock_fixture.enemy_anchor);
        const aoe::EntityId locked_gate = locked.add_building(
            kind, blue, lock_fixture.gate
        );
        const aoe::EntityId owner = locked.add_unit(
            aoe::UnitKind::villager, blue, lock_fixture.start
        );
        require(aoe::execute(
            locked, aoe::SetGateLockedCommand{locked_gate, true}
        ));
        require(gate_by_id(locked, locked_gate).gate_locked);
        require(!gate_by_id(locked, locked_gate).gate_open);
        require(!locked.command_unit(owner, lock_fixture.destination));
        require(aoe::execute(
            locked, aoe::SetGateLockedCommand{locked_gate, false}
        ));
        require(locked.command_unit(owner, lock_fixture.destination));
    }

    GateFixture transition_fixture = fixture_for(
        aoe::BuildingKind::stone_gate_y
    );
    aoe::Simulation transition(std::move(transition_fixture.map));
    transition.replace_roster(*roster, diplomacy);
    add_anchor(transition, transition_fixture.enemy_anchor);
    const aoe::EntityId transition_gate = transition.add_building(
        aoe::BuildingKind::stone_gate_y, blue, transition_fixture.gate
    );
    const aoe::EntityId transitioning_ally = transition.add_unit(
        aoe::UnitKind::villager, green, transition_fixture.start
    );
    require(transition.command_unit(
        transitioning_ally, transition_fixture.destination
    ));
    transition.update();
    auto hostile = diplomacy;
    require(hostile.set_symmetric_stance(
        blue, green, aoe::Diplomacy::enemy
    ));
    transition.replace_roster(*roster, hostile);
    for (int tick = 0; tick < 20; ++tick) transition.update();
    require(!gate_by_id(transition, transition_gate).gate_open);
    require(unit_by_id(transition, transitioning_ally).position !=
            transition_fixture.destination);
    transition.replace_roster(*roster, diplomacy);
    require(transition.command_unit(
        transitioning_ally, transition_fixture.destination
    ));
    for (int tick = 0; tick < 80 &&
         unit_by_id(transition, transitioning_ally).position !=
             transition_fixture.destination;
         ++tick) {
        transition.update();
    }
    require(unit_by_id(transition, transitioning_ally).position ==
            transition_fixture.destination);

    GateFixture durable_fixture = fixture_for(
        aoe::BuildingKind::fortified_gate_x
    );
    aoe::Simulation durable(std::move(durable_fixture.map));
    durable.replace_roster(*roster, diplomacy);
    add_anchor(durable, durable_fixture.enemy_anchor);
    const aoe::EntityId durable_gate = durable.add_building(
        aoe::BuildingKind::fortified_gate_x, blue, durable_fixture.gate
    );
    aoe::Replay replay;
    replay.record(0, aoe::SetGateLockedCommand{durable_gate, true});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-gate-lock.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay decoded = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    decoded.apply_current_tick(durable);
    require(gate_by_id(durable, durable_gate).gate_locked);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-gate-lock.save";
    const std::string before_hash = aoe::deterministic_state_hash(durable);
    aoe::save_game(durable, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(gate_by_id(restored, durable_gate).gate_locked);
    require(aoe::deterministic_state_hash(restored) == before_hash);

    aoe::LockstepFrame frame;
    frame.kind = aoe::LockstepFrameKind::turn;
    frame.player = aoe::Player::blue;
    frame.scenario_digest = "gate-lock";
    frame.commands = {aoe::SetGateLockedCommand{durable_gate, false}};
    const aoe::LockstepFrame wire = aoe::decode_lockstep_frame(
        aoe::encode_lockstep_frame(frame)
    );
    require(wire.commands.size() == 1);
    require(!std::get<aoe::SetGateLockedCommand>(
        wire.commands.front()
    ).locked);

    aoe::Scenario scenario(12, 7);
    scenario.buildings.push_back({
        aoe::BuildingKind::stone_gate_x,
        aoe::EntityOwner{aoe::Player::blue},
        {4, 3},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        true,
    });
    const auto scenario_path = std::filesystem::temp_directory_path() /
        "aoe-locked-gate.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded_scenario = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded_scenario.buildings.front().gate_locked);
    const aoe::Simulation scenario_simulation =
        aoe::create_simulation(loaded_scenario);
    require(scenario_simulation.buildings().front().gate_locked);
}

void stone_gates_use_feudal_cost_armor_replay_and_persistence() {
    const aoe::BuildingRules& along_x =
        aoe::rules_for(aoe::BuildingKind::stone_gate_x);
    const aoe::BuildingRules& along_y =
        aoe::rules_for(aoe::BuildingKind::stone_gate_y);
    require(along_x.hit_points == 2750);
    require(along_x.melee_armor == 6);
    require(along_x.pierce_armor == 6);
    require(along_x.stone_cost == 30);
    require(along_x.minimum_age == aoe::Age::feudal);
    require(along_x.footprint_width == 4);
    require(along_x.footprint_height == 1);
    require(along_y.footprint_width == 1);
    require(along_y.footprint_height == 4);

    aoe::Simulation first(aoe::GameMap(12, 7));
    const aoe::EntityId builder = first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    first.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 4}
    );
    first.replace_state(
        first.units(),
        first.buildings(),
        {0, 0, 0, 30},
        first.economy(aoe::Player::red),
        0
    );
    require(!first.construct_building_at(
        builder,
        aoe::BuildingKind::stone_gate_x,
        {2, 2}
    ));
    first.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    aoe::Simulation second = first;

    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::stone_gate_x,
            {2, 2},
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-stone-gate-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    recorded.apply_current_tick(first);
    replayed.apply_current_tick(second);
    require(first.economy(aoe::Player::blue).stone == 0);
    require(first.buildings().back().kind ==
            aoe::BuildingKind::stone_gate_x);
    require(second.buildings().back().kind ==
            aoe::BuildingKind::stone_gate_x);
    for (int tick = 0;
         tick < 25 && !first.buildings().back().completed();
         ++tick) {
        first.update();
        second.update();
    }
    require(first.buildings().back().completed());
    require(second.buildings().back().completed());
    require(first.buildings().back().hit_points ==
            second.buildings().back().hit_points);
    require(first.tick_number() == second.tick_number());

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-stone-gate-test.save";
    aoe::save_game(first, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().back().kind ==
            aoe::BuildingKind::stone_gate_x);
    require(loaded.buildings().back().hit_points == 2750);

    aoe::Simulation combat(aoe::GameMap(10, 6));
    const aoe::EntityId gate = combat.add_building(
        aoe::BuildingKind::stone_gate_y,
        aoe::Player::blue,
        {3, 1}
    );
    const aoe::EntityId militia = combat.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {2, 1}
    );
    require(combat.command_unit(militia, {3, 1}));
    combat.update();
    const auto damaged = std::ranges::find_if(
        combat.buildings(),
        [gate](const aoe::Building& building) {
            return building.id == gate;
        }
    );
    require(damaged != combat.buildings().end());
    require(damaged->hit_points == 2749);
}

void watch_towers_construct_fire_upgrade_and_persist() {
    const aoe::BuildingRules& rules =
        aoe::rules_for(aoe::BuildingKind::watch_tower);
    require(rules.hit_points == 1020);
    require(rules.melee_armor == 1);
    require(rules.pierce_armor == 7);
    require(rules.stone_cost == 125);
    require(rules.minimum_age == aoe::Age::feudal);
    require(rules.attack == 5);
    require(rules.attack_range == 8);
    require(rules.minimum_attack_range == 1);
    require(rules.projectile_count == 1);

    aoe::Simulation dark(aoe::GameMap(12, 8));
    const aoe::EntityId builder = dark.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    dark.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    dark.replace_state(
        dark.units(),
        dark.buildings(),
        {25, 0, 0, 200},
        dark.economy(aoe::Player::red),
        0
    );
    require(!dark.construct_building_at(
        builder,
        aoe::BuildingKind::watch_tower,
        {4, 2}
    ));
    dark.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    dark.replace_state(
        dark.units(),
        dark.buildings(),
        {25, 0, 0, 124},
        dark.economy(aoe::Player::red),
        0
    );
    require(!dark.construct_building_at(
        builder,
        aoe::BuildingKind::watch_tower,
        {3, 2}
    ));
    dark.replace_state(
        dark.units(),
        dark.buildings(),
        {25, 0, 0, 200},
        dark.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = dark;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::watch_tower,
            {3, 2},
        }
    );
    recorded.apply_current_tick(dark);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-tower-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(dark.economy(aoe::Player::blue).stone == 75);
    require(replayed.economy(aoe::Player::blue).stone == 75);
    require(dark.buildings().back().kind ==
            aoe::BuildingKind::watch_tower);
    require(replayed.buildings().back().kind ==
            aoe::BuildingKind::watch_tower);

    aoe::Simulation defense(aoe::GameMap(20, 12));
    defense.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId target = defense.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {10, 4}
    );
    defense.update();
    require(defense.projectiles().size() == 1);
    require(defense.projectiles().front().target == target);
    require(defense.projectiles().front().damage == 5);
    require(defense.buildings().front().attack_cooldown == 10);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-tower-test.save";
    aoe::save_game(defense, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().front().kind ==
            aoe::BuildingKind::watch_tower);
    require(loaded.buildings().front().attack_cooldown == 10);
    require(loaded.projectiles().size() == 1);

    aoe::Simulation upgrades(aoe::GameMap(20, 12));
    upgrades.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {4, 4}
    );
    upgrades.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {13, 4}
    );
    upgrades.update();
    require(upgrades.projectiles().empty());
    upgrades.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fletching}
    );
    upgrades.update();
    require(upgrades.projectiles().size() == 1);
    require(upgrades.projectiles().front().damage == 6);

    aoe::Simulation dead_zone(aoe::GameMap(10, 8));
    dead_zone.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {4, 4}
    );
    dead_zone.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {5, 4}
    );
    dead_zone.update();
    require(dead_zone.projectiles().empty());
    dead_zone.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::murder_holes}
    );
    dead_zone.update();
    require(dead_zone.projectiles().size() == 1);
}

void stone_walls_gate_block_resist_and_persist() {
    const aoe::BuildingRules& rules =
        aoe::rules_for(aoe::BuildingKind::stone_wall);
    require(rules.hit_points == 1800);
    require(rules.melee_armor == 8);
    require(rules.pierce_armor == 10);
    require(rules.stone_cost == 5);
    require(rules.minimum_age == aoe::Age::feudal);
    require(rules.footprint_width == 1);

    aoe::Simulation construction(aoe::GameMap(10, 6));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 5}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {0, 0, 0, 5},
        construction.economy(aoe::Player::red),
        0
    );
    require(!construction.construct_building_at(
        builder,
        aoe::BuildingKind::stone_wall,
        {2, 1}
    ));
    construction.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {0, 0, 0, 4},
        construction.economy(aoe::Player::red),
        0
    );
    require(!construction.construct_building_at(
        builder,
        aoe::BuildingKind::stone_wall,
        {2, 1}
    ));
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {0, 0, 0, 5},
        construction.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = construction;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::stone_wall,
            {2, 1},
        }
    );
    recorded.apply_current_tick(construction);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-stone-wall.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(construction.economy(aoe::Player::blue).stone == 0);
    require(replayed.economy(aoe::Player::blue).stone == 0);
    require(construction.buildings().back().kind ==
            aoe::BuildingKind::stone_wall);
    require(replayed.buildings().back().kind ==
            aoe::BuildingKind::stone_wall);
    for (int tick = 0; tick < rules.construction_ticks; ++tick) {
        construction.update();
        replayed.update();
    }
    require(construction.buildings().back().completed());
    require(replayed.buildings().back().completed());

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-stone-wall.save";
    aoe::save_game(construction, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().back().kind ==
            aoe::BuildingKind::stone_wall);
    require(loaded.buildings().back().hit_points == 1800);

    aoe::Simulation resistance(aoe::GameMap(7, 5));
    resistance.add_building(
        aoe::BuildingKind::stone_wall,
        aoe::Player::blue,
        {3, 2}
    );
    const aoe::EntityId attacker = resistance.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {4, 2}
    );
    require(resistance.command_unit(attacker, {3, 2}));
    resistance.update();
    require(resistance.buildings().front().hit_points == 1799);

    aoe::GameMap corridor(7, 3);
    for (int x = 0; x < corridor.width(); ++x) {
        corridor.set_terrain({x, 0}, aoe::Terrain::water);
        corridor.set_terrain({x, 2}, aoe::Terrain::water);
    }
    aoe::Simulation breach(std::move(corridor));
    const aoe::EntityId pathfinder = breach.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    breach.add_building(
        aoe::BuildingKind::stone_wall,
        aoe::Player::blue,
        {3, 1}
    );
    const aoe::EntityId ram = breach.add_unit(
        aoe::UnitKind::battering_ram,
        aoe::Player::red,
        {4, 1}
    );
    require(!breach.command_unit(pathfinder, {5, 1}));
    require(breach.command_unit(ram, {3, 1}));
    for (int tick = 0; tick < 170; ++tick) {
        breach.update();
    }
    require(breach.buildings().empty());
    require(breach.command_unit(pathfinder, {3, 1}));
}

void skirmishers_train_counter_archers_and_persist() {
    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::skirmisher);
    require(rules.hit_points == 30);
    require(rules.attack == 2);
    require(rules.attack_range == 4);
    require(rules.pierce_armor == 3);
    require(rules.bonus_vs_archers == 3);
    require(rules.food_cost == 25);
    require(rules.wood_cost == 35);
    require(rules.gold_cost == 0);
    require(rules.trained_at == aoe::BuildingKind::archery_range);
    require(rules.minimum_age == aoe::Age::feudal);
    require(aoe::is_archer(aoe::UnitKind::archer));
    require(aoe::is_archer(aoe::UnitKind::skirmisher));

    aoe::Simulation production(aoe::GameMap(12, 8));
    const aoe::EntityId range = production.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {4, 2}
    );
    production.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    production.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    production.replace_state(
        production.units(),
        production.buildings(),
        {35, 25, 0, 0},
        production.economy(aoe::Player::red),
        0
    );
    require(!production.queue_unit_at(
        range,
        aoe::UnitKind::skirmisher
    ));
    production.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    production.replace_state(
        production.units(),
        production.buildings(),
        {34, 25, 0, 0},
        production.economy(aoe::Player::red),
        0
    );
    require(!production.queue_unit_at(
        range,
        aoe::UnitKind::skirmisher
    ));
    production.replace_state(
        production.units(),
        production.buildings(),
        {35, 25, 0, 0},
        production.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = production;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::QueueUnitCommand{range, aoe::UnitKind::skirmisher}
    );
    recorded.apply_current_tick(production);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-skirmisher.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(production.economy(aoe::Player::blue).wood == 0);
    require(production.economy(aoe::Player::blue).food == 0);
    require(replayed.economy(aoe::Player::blue).wood == 0);
    for (int tick = 0; tick < rules.training_ticks; ++tick) {
        production.update();
        replayed.update();
    }
    require(production.units().back().kind == aoe::UnitKind::skirmisher);
    require(replayed.units().back().kind == aoe::UnitKind::skirmisher);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-skirmisher.save";
    aoe::save_game(production, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().back().kind == aoe::UnitKind::skirmisher);
    require(loaded.units().back().hit_points == 30);

    aoe::Simulation counter(aoe::GameMap(8, 6));
    const aoe::EntityId skirmisher = counter.add_unit(
        aoe::UnitKind::skirmisher,
        aoe::Player::blue,
        {2, 2}
    );
    counter.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {4, 2}
    );
    require(counter.command_unit(skirmisher, {4, 2}));
    for (int tick = 0; tick < 6; ++tick) {
        counter.update();
    }
    require(counter.units()[1].hit_points == 25);

    aoe::Simulation resistance(aoe::GameMap(8, 6));
    const aoe::EntityId archer = resistance.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    resistance.add_unit(
        aoe::UnitKind::skirmisher,
        aoe::Player::red,
        {3, 2}
    );
    require(resistance.command_unit(archer, {3, 2}));
    for (int tick = 0; tick < 5; ++tick) {
        resistance.update();
    }
    require(resistance.units()[1].hit_points == 29);

    aoe::Simulation upgraded(aoe::GameMap(8, 6));
    upgraded.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fletching}
    );
    upgraded.add_unit(
        aoe::UnitKind::skirmisher,
        aoe::Player::blue,
        {1, 1}
    );
    require(upgraded.units().front().attack == 3);
}

void production_buildings_accept_fifteen_queued_units() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {3, 3}
    );
    simulation.replace_civilizations(
        aoe::Civilization::huns, aoe::Civilization::generic
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 7}
    );
    simulation.replace_ages(aoe::Age::dark, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {10000, 10000, 10000, 10000},
        simulation.economy(aoe::Player::red), 0
    );

    for (int order = 0; order < 15; ++order) {
        require(simulation.queue_unit_at(barracks, aoe::UnitKind::militia));
    }
    require(simulation.buildings().front().production_queue.size() == 15);
    require(!simulation.queue_unit_at(barracks, aoe::UnitKind::militia));
    require(simulation.buildings().front().production_queue.size() == 15);
}

void mangonel_automatic_targets_avoid_friendly_splash() {
    for (aoe::UnitKind kind : {
             aoe::UnitKind::mangonel,
             aoe::UnitKind::onager,
             aoe::UnitKind::siege_onager,
         }) {
        aoe::Simulation unsafe(aoe::GameMap(14, 9));
        const aoe::EntityId siege = unsafe.add_unit(
            kind, aoe::Player::blue, {1, 4}
        );
        unsafe.add_unit(
            aoe::UnitKind::archer, aoe::Player::red, {7, 4}
        );
        unsafe.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {7, 5}
        );
        require(unsafe.command_attack_move(siege, {11, 4}));
        unsafe.update();
        require(unsafe.units()[0].attack_target_id == 0);
        require(std::ranges::none_of(
            unsafe.projectiles(),
            [](const aoe::Projectile& projectile) {
                return projectile.owner == aoe::Player::blue;
            }
        ));
    }

    aoe::Simulation building_hazard(aoe::GameMap(14, 9));
    building_hazard.add_unit(
        aoe::UnitKind::onager, aoe::Player::blue, {1, 4}
    );
    building_hazard.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {7, 4}
    );
    building_hazard.add_building(
        aoe::BuildingKind::outpost, aoe::Player::blue, {7, 5}
    );
    building_hazard.update();
    require(building_hazard.units()[0].attack_target_id == 0);

    aoe::Simulation alternate(aoe::GameMap(16, 10));
    const aoe::EntityId siege = alternate.add_unit(
        aoe::UnitKind::siege_onager, aoe::Player::blue, {1, 4}
    );
    const aoe::EntityId unsafe_target = alternate.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {6, 4}
    );
    alternate.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 5}
    );
    const aoe::EntityId safe_target = alternate.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {9, 4}
    );
    alternate.update();
    require(alternate.units()[0].attack_target_id == safe_target);
    require(alternate.units()[0].attack_target_id != unsafe_target);
    require(alternate.units()[0].attack_target_auto);

    aoe::Simulation explicit_attack(aoe::GameMap(12, 8));
    const aoe::EntityId explicit_siege = explicit_attack.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {1, 3}
    );
    explicit_attack.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {6, 3}
    );
    const aoe::EntityId explicit_friendly = explicit_attack.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 4}
    );
    const int friendly_hp = explicit_attack.units()[2].hit_points;
    require(explicit_attack.command_unit(explicit_siege, {6, 3}));
    require(!explicit_attack.units()[0].attack_target_auto);
    for (int tick = 0; tick < 12; ++tick) explicit_attack.update();
    const auto explicit_friendly_after = std::ranges::find_if(
        explicit_attack.units(),
        [explicit_friendly](const aoe::Unit& unit) {
            return unit.id == explicit_friendly;
        }
    );
    require(
        explicit_friendly_after == explicit_attack.units().end() ||
        explicit_friendly_after->hit_points < friendly_hp
    );

    aoe::Simulation attack_ground(aoe::GameMap(12, 8));
    const aoe::EntityId ground_siege = attack_ground.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {1, 3}
    );
    const aoe::EntityId ground_friendly = attack_ground.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 4}
    );
    attack_ground.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {11, 7}
    );
    const int ground_friendly_hp = attack_ground.units()[1].hit_points;
    require(attack_ground.command_attack_ground(ground_siege, {6, 3}));
    for (int tick = 0; tick < 12; ++tick) attack_ground.update();
    const auto ground_friendly_after = std::ranges::find_if(
        attack_ground.units(),
        [ground_friendly](const aoe::Unit& unit) {
            return unit.id == ground_friendly;
        }
    );
    require(
        ground_friendly_after == attack_ground.units().end() ||
        ground_friendly_after->hit_points < ground_friendly_hp
    );

    auto make_diplomacy = [] {
        aoe::Simulation simulation(aoe::GameMap(12, 8));
        simulation.add_unit(
            aoe::UnitKind::onager, aoe::Player::blue, {1, 3}
        );
        simulation.add_unit(
            aoe::UnitKind::archer, aoe::Player::red, {7, 3}
        );
        return simulation;
    };
    aoe::Replay diplomacy;
    diplomacy.record(0, aoe::SetDiplomacyCommand{
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    });
    diplomacy.record(1, aoe::SetDiplomacyCommand{
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::enemy
    });
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-safe-mangonel-auto.replay";
    aoe::save_replay(diplomacy, replay_path);
    aoe::Replay loaded_diplomacy = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first = make_diplomacy();
    aoe::Simulation second = make_diplomacy();
    diplomacy.apply_current_tick(first);
    loaded_diplomacy.apply_current_tick(second);
    first.update();
    second.update();
    require(first.units()[0].attack_target_id == 0);
    require(second.units()[0].attack_target_id == 0);
    diplomacy.apply_current_tick(first);
    loaded_diplomacy.apply_current_tick(second);
    first.update();
    second.update();
    require(first.units()[0].attack_target_id == first.units()[1].id);
    require(
        first.units()[0].attack_target_id ==
        second.units()[0].attack_target_id
    );
}

void mangonels_train_splash_friendlies_and_persist() {
    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::mangonel);
    require(rules.hit_points == 50);
    require(rules.attack == 40);
    require(rules.attack_range == 7);
    require(rules.minimum_attack_range == 3);
    require(rules.splash_radius == 1);
    require(rules.damage_class == aoe::DamageClass::melee);
    require(rules.pierce_armor == 6);
    require(rules.wood_cost == 160);
    require(rules.gold_cost == 135);
    require(rules.trained_at == aoe::BuildingKind::siege_workshop);
    require(rules.minimum_age == aoe::Age::castle);

    aoe::Simulation production(aoe::GameMap(14, 9));
    const aoe::EntityId workshop = production.add_building(
        aoe::BuildingKind::siege_workshop,
        aoe::Player::blue,
        {5, 4}
    );
    production.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    production.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 8}
    );
    production.replace_ages(aoe::Age::castle, aoe::Age::dark);
    production.replace_state(
        production.units(),
        production.buildings(),
        {159, 0, 135, 0},
        production.economy(aoe::Player::red),
        0
    );
    require(!production.queue_unit_at(
        workshop,
        aoe::UnitKind::mangonel
    ));
    production.replace_state(
        production.units(),
        production.buildings(),
        {160, 0, 135, 0},
        production.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = production;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::QueueUnitCommand{workshop, aoe::UnitKind::mangonel}
    );
    recorded.apply_current_tick(production);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-mangonel.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(production.economy(aoe::Player::blue).wood == 0);
    require(production.economy(aoe::Player::blue).gold == 0);
    require(replayed.economy(aoe::Player::blue).wood == 0);
    for (int tick = 0; tick < rules.training_ticks; ++tick) {
        production.update();
        replayed.update();
    }
    require(production.units().back().kind == aoe::UnitKind::mangonel);
    require(replayed.units().back().kind == aoe::UnitKind::mangonel);

    aoe::Simulation dead_zone(aoe::GameMap(10, 6));
    const aoe::EntityId close_mangonel = dead_zone.add_unit(
        aoe::UnitKind::mangonel,
        aoe::Player::blue,
        {1, 2}
    );
    dead_zone.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {3, 2}
    );
    require(dead_zone.command_unit(close_mangonel, {3, 2}));
    dead_zone.update();
    require(std::ranges::none_of(
        dead_zone.projectiles(),
        [](const aoe::Projectile& projectile) {
            return projectile.owner == aoe::Player::blue;
        }
    ));
    require(!dead_zone.units().front().moving);
    require(dead_zone.units().front().attack_cooldown == 0);

    aoe::Simulation splash(aoe::GameMap(12, 8));
    const aoe::EntityId mangonel = splash.add_unit(
        aoe::UnitKind::mangonel,
        aoe::Player::blue,
        {1, 3}
    );
    splash.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {6, 3}
    );
    splash.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {6, 4}
    );
    splash.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {6, 2}
    );
    require(splash.command_unit(mangonel, {6, 3}));
    splash.update();
    const auto mangonel_shot = std::ranges::find_if(
        splash.projectiles(),
        [](const aoe::Projectile& projectile) {
            return projectile.splash_radius == 1;
        }
    );
    require(mangonel_shot != splash.projectiles().end());
    require(std::ranges::count_if(
        splash.projectiles(),
        [](const aoe::Projectile& projectile) {
            return projectile.splash_radius == 1;
        }
    ) == 1);
    require(
        mangonel_shot->damage_class ==
        aoe::DamageClass::melee
    );
    const std::size_t projectile_count = splash.projectiles().size();

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-mangonel.save";
    aoe::save_game(splash, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().kind == aoe::UnitKind::mangonel);
    require(loaded.projectiles().size() == projectile_count);
    require(std::ranges::count_if(
        loaded.projectiles(),
        [](const aoe::Projectile& projectile) {
            return projectile.splash_radius == 1;
        }
    ) == 1);
    for (int tick = 0; tick < 4; ++tick) {
        splash.update();
        loaded.update();
    }
    require(splash.units().size() == 1);
    require(loaded.units().size() == 1);
    require(splash.units().front().kind == aoe::UnitKind::mangonel);
    require(loaded.units().front().kind == aoe::UnitKind::mangonel);
}

void mangonel_attack_ground_moves_fires_splashes_and_replays() {
    aoe::Simulation simulation(aoe::GameMap(15, 8));
    const aoe::EntityId mangonel = simulation.add_unit(
        aoe::UnitKind::mangonel,
        aoe::Player::blue,
        {1, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {10, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {10, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {10, 2}
    );
    std::vector<aoe::Unit> passive_units = simulation.units();
    for (aoe::Unit& unit : passive_units) {
        if (unit.id != mangonel) {
            unit.stance = aoe::UnitStance::passive;
        }
    }
    simulation.replace_state(
        std::move(passive_units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(!simulation.command_attack_ground(mangonel, {3, 3}));

    aoe::Simulation replayed = simulation;
    aoe::Replay replay;
    replay.record(
        0,
        aoe::AttackGroundCommand{mangonel, {10, 3}}
    );
    replay.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-attack-ground.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.units().front().attacking_ground);
    require(replayed.units().front().attacking_ground);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-attack-ground.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().attacking_ground);
    require(
        loaded.units().front().attack_ground_target ==
        aoe::TilePosition(10, 3)
    );

    const auto ground_shot = [](const aoe::Simulation& current) {
        return std::ranges::find_if(
            current.projectiles(),
            [](const aoe::Projectile& projectile) {
                return projectile.target == 0 &&
                    projectile.splash_radius > 0;
            }
        );
    };
    for (int tick = 0;
         tick < 20 &&
         ground_shot(simulation) == simulation.projectiles().end();
         ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    const auto shot = ground_shot(simulation);
    require(shot != simulation.projectiles().end());
    require(
        shot->target == 0 &&
        shot->destination ==
            aoe::TilePosition(10, 3)
    );
    require(!simulation.units().front().attacking_ground);
    const auto replayed_shot = ground_shot(replayed);
    const auto loaded_shot = ground_shot(loaded);
    require(replayed_shot != replayed.projectiles().end());
    require(loaded_shot != loaded.projectiles().end());
    require(
        replayed_shot->destination == shot->destination
    );
    require(
        loaded_shot->destination == shot->destination
    );

    bool splash_impact_seen = false;
    for (int tick = 0; tick < 6; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
        if (!simulation.impact_effects().empty()) {
            splash_impact_seen = true;
            require(simulation.impact_effects().front().splash);
            require(
                simulation.impact_effects().front().position ==
                aoe::TilePosition(10, 3)
            );
            require(
                replayed.impact_effects().front().ticks_remaining ==
                simulation.impact_effects().front().ticks_remaining
            );
            require(
                loaded.impact_effects().front().ticks_remaining ==
                simulation.impact_effects().front().ticks_remaining
            );
        }
    }
    require(splash_impact_seen);
    require(simulation.units().size() == 1);
    require(replayed.units().size() == 1);
    require(loaded.units().size() == 1);
    require(simulation.units().front().kind == aoe::UnitKind::mangonel);
}

void man_at_arms_research_upgrades_line_and_persists() {
    const aoe::UnitRules& unit_rules =
        aoe::rules_for(aoe::UnitKind::man_at_arms);
    const aoe::TechnologyRules& technology_rules =
        aoe::rules_for(aoe::Technology::man_at_arms);
    require(unit_rules.hit_points == 45);
    require(unit_rules.attack == 6);
    require(unit_rules.food_cost == 60);
    require(unit_rules.gold_cost == 20);
    require(unit_rules.trained_at == aoe::BuildingKind::barracks);
    require(technology_rules.researched_at == aoe::BuildingKind::barracks);
    require(technology_rules.minimum_age == aoe::Age::feudal);
    require(technology_rules.food_cost == 100);
    require(technology_rules.gold_cost == 40);

    aoe::Simulation simulation(aoe::GameMap(14, 9));
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId second_barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId militia = simulation.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::blue,
        {5, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 8}
    );
    auto damaged = simulation.units();
    damaged.front().hit_points = 30;
    simulation.replace_state(
        std::move(damaged),
        simulation.buildings(),
        {0, 99, 60, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(!simulation.research_technology_at(
        barracks,
        aoe::Technology::man_at_arms
    ));
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 160, 60, 0},
        simulation.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = simulation;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ResearchTechnologyCommand{
            barracks,
            aoe::Technology::man_at_arms,
        }
    );
    recorded.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-man-at-arms.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.economy(aoe::Player::blue).food == 60);
    require(simulation.economy(aoe::Player::blue).gold == 20);
    require(replayed.economy(aoe::Player::blue).food == 60);

    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        replayed.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-man-at-arms.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.buildings().front().technology_research_ticks_remaining ==
        simulation.buildings().front().technology_research_ticks_remaining
    );
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.queue_unit_at(
        second_barracks,
        aoe::UnitKind::militia
    ));
    require(replayed.queue_unit_at(
        second_barracks,
        aoe::UnitKind::militia
    ));
    require(loaded.queue_unit_at(
        second_barracks,
        aoe::UnitKind::militia
    ));
    for (int tick = 0; tick < 10; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::man_at_arms
    ));
    require(replayed.has_technology(
        aoe::Player::blue,
        aoe::Technology::man_at_arms
    ));
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::man_at_arms
    ));
    const auto upgraded = std::ranges::find_if(
        simulation.units(),
        [militia](const aoe::Unit& unit) { return unit.id == militia; }
    );
    require(upgraded != simulation.units().end());
    require(upgraded->kind == aoe::UnitKind::man_at_arms);
    require(upgraded->hit_points == 35);
    require(upgraded->attack == 6);
    require(
        simulation.buildings()[1].production_queue.front().kind ==
        aoe::UnitKind::man_at_arms
    );
    simulation.update();
    simulation.update();
    require(simulation.units().back().kind ==
            aoe::UnitKind::man_at_arms);

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 60, 20, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        barracks,
        aoe::UnitKind::militia
    ));
    require(
        simulation.buildings().front().production_queue.front().kind ==
        aoe::UnitKind::man_at_arms
    );
    for (int tick = 0; tick < unit_rules.training_ticks; ++tick) {
        simulation.update();
    }
    require(simulation.units().back().kind ==
            aoe::UnitKind::man_at_arms);
}

void long_swordsman_research_upgrades_militia_line_and_persists() {
    const aoe::UnitRules& unit_rules =
        aoe::rules_for(aoe::UnitKind::long_swordsman);
    const aoe::TechnologyRules& technology_rules =
        aoe::rules_for(aoe::Technology::long_swordsman);
    require(unit_rules.hit_points == 55);
    require(unit_rules.attack == 9);
    require(unit_rules.pierce_armor == 1);
    require(unit_rules.bonus_vs_buildings == 3);
    require(unit_rules.food_cost == 60);
    require(unit_rules.gold_cost == 20);
    require(unit_rules.minimum_age == aoe::Age::castle);
    require(technology_rules.researched_at == aoe::BuildingKind::barracks);
    require(technology_rules.minimum_age == aoe::Age::castle);
    require(technology_rules.food_cost == 200);
    require(technology_rules.gold_cost == 65);
    require(technology_rules.research_ticks == 23);

    aoe::Simulation simulation(aoe::GameMap(14, 9));
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId second_barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId swordsman = simulation.add_unit(
        aoe::UnitKind::man_at_arms,
        aoe::Player::blue,
        {5, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 8}
    );
    auto units = simulation.units();
    units.front().hit_points = 31;
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        {0, 400, 150, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(!simulation.research_technology_at(
        barracks,
        aoe::Technology::long_swordsman
    ));
    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::man_at_arms}
    );

    aoe::Simulation replayed = simulation;
    aoe::Replay replay;
    replay.record(
        0,
        aoe::ResearchTechnologyCommand{
            barracks,
            aoe::Technology::long_swordsman,
        }
    );
    replay.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-long-swordsman.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.economy(aoe::Player::blue).food == 200);
    require(simulation.economy(aoe::Player::blue).gold == 85);

    for (int tick = 0; tick < 7; ++tick) {
        simulation.update();
        replayed.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-long-swordsman.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.buildings().front().technology_research_ticks_remaining ==
        simulation.buildings().front().technology_research_ticks_remaining
    );
    for (int tick = 7; tick < technology_rules.research_ticks; ++tick) {
        if (tick == 15) {
            require(simulation.queue_unit_at(
                second_barracks,
                aoe::UnitKind::militia
            ));
            require(replayed.queue_unit_at(
                second_barracks,
                aoe::UnitKind::militia
            ));
            require(loaded.queue_unit_at(
                second_barracks,
                aoe::UnitKind::militia
            ));
        }
        simulation.update();
        replayed.update();
        loaded.update();
    }
    for (aoe::Simulation* candidate : {&simulation, &replayed, &loaded}) {
        require(candidate->has_technology(
            aoe::Player::blue,
            aoe::Technology::long_swordsman
        ));
        const auto upgraded = std::ranges::find_if(
            candidate->units(),
            [swordsman](const aoe::Unit& unit) {
                return unit.id == swordsman;
            }
        );
        require(upgraded != candidate->units().end());
        require(upgraded->kind == aoe::UnitKind::long_swordsman);
        require(upgraded->hit_points == 41);
        require(upgraded->attack == 9);
        require(
            candidate->buildings()[1].production_queue.front().kind ==
            aoe::UnitKind::long_swordsman
        );
    }

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 60, 20, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(barracks, aoe::UnitKind::militia));
    require(
        simulation.buildings().front().production_queue.front().kind ==
        aoe::UnitKind::long_swordsman
    );
    for (int tick = 0; tick < unit_rules.training_ticks; ++tick) {
        simulation.update();
    }
    require(
        simulation.units().back().kind == aoe::UnitKind::long_swordsman
    );
}

void crossbowman_research_upgrades_archer_line_and_persists() {
    const aoe::UnitRules& archer_rules =
        aoe::rules_for(aoe::UnitKind::archer);
    const aoe::UnitRules& crossbow_rules =
        aoe::rules_for(aoe::UnitKind::crossbowman);
    const aoe::TechnologyRules& technology_rules =
        aoe::rules_for(aoe::Technology::crossbowman);
    require(archer_rules.hit_points == 30);
    require(archer_rules.wood_cost == 25);
    require(archer_rules.food_cost == 0);
    require(archer_rules.gold_cost == 45);
    require(crossbow_rules.hit_points == 35);
    require(crossbow_rules.attack == 5);
    require(crossbow_rules.attack_range == 5);
    require(crossbow_rules.training_ticks == 11);
    require(technology_rules.researched_at ==
            aoe::BuildingKind::archery_range);
    require(technology_rules.minimum_age == aoe::Age::castle);
    require(technology_rules.food_cost == 125);
    require(technology_rules.gold_cost == 75);
    require(aoe::is_archer(aoe::UnitKind::crossbowman));

    aoe::Simulation simulation(aoe::GameMap(16, 10));
    const aoe::EntityId first_range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId second_range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {5, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 9}
    );
    auto damaged = simulation.units();
    damaged.front().hit_points = 20;
    simulation.replace_state(
        std::move(damaged),
        simulation.buildings(),
        {25, 125, 120, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    aoe::Simulation replayed = simulation;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ResearchTechnologyCommand{
            first_range,
            aoe::Technology::crossbowman,
        }
    );
    recorded.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-crossbowman.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.economy(aoe::Player::blue).food == 0);
    require(simulation.economy(aoe::Player::blue).gold == 45);

    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        replayed.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-crossbowman.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.queue_unit_at(
        second_range,
        aoe::UnitKind::archer
    ));
    require(replayed.queue_unit_at(
        second_range,
        aoe::UnitKind::archer
    ));
    require(loaded.queue_unit_at(
        second_range,
        aoe::UnitKind::archer
    ));
    for (int tick = 8; tick < technology_rules.research_ticks; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::crossbowman
    ));
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::crossbowman
    ));
    const auto upgraded = std::ranges::find_if(
        simulation.units(),
        [archer](const aoe::Unit& unit) { return unit.id == archer; }
    );
    require(upgraded != simulation.units().end());
    require(upgraded->kind == aoe::UnitKind::crossbowman);
    require(upgraded->hit_points == 25);
    require(upgraded->attack == 5);
    require(
        simulation.buildings()[1].production_queue.front().kind ==
        aoe::UnitKind::crossbowman
    );
    while (!simulation.buildings()[1].production_queue.empty()) {
        simulation.update();
    }
    require(simulation.units().back().kind ==
            aoe::UnitKind::crossbowman);

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {25, 0, 45, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        first_range,
        aoe::UnitKind::archer
    ));
    require(
        simulation.buildings().front().production_queue.front().kind ==
        aoe::UnitKind::crossbowman
    );
    require(
        simulation.buildings().front().production_queue.front()
            .ticks_remaining == crossbow_rules.training_ticks
    );
}

void pikeman_research_upgrades_spear_line_and_persists() {
    const aoe::UnitRules& spear_rules =
        aoe::rules_for(aoe::UnitKind::spearman);
    const aoe::UnitRules& pike_rules =
        aoe::rules_for(aoe::UnitKind::pikeman);
    const aoe::TechnologyRules& technology_rules =
        aoe::rules_for(aoe::Technology::pikeman);
    require(spear_rules.food_cost == 35);
    require(spear_rules.wood_cost == 25);
    require(pike_rules.hit_points == 55);
    require(pike_rules.attack == 4);
    require(pike_rules.bonus_vs_cavalry == 22);
    require(pike_rules.food_cost == 35);
    require(pike_rules.wood_cost == 25);
    require(technology_rules.researched_at == aoe::BuildingKind::barracks);
    require(technology_rules.minimum_age == aoe::Age::castle);
    require(technology_rules.food_cost == 215);
    require(technology_rules.gold_cost == 90);

    aoe::Simulation simulation(aoe::GameMap(16, 10));
    const aoe::EntityId first_barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId second_barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId spearman = simulation.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {5, 4}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 9}
    );
    auto damaged = simulation.units();
    damaged.front().hit_points = 35;
    simulation.replace_state(
        std::move(damaged),
        simulation.buildings(),
        {25, 250, 90, 0},
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);

    aoe::Simulation replayed = simulation;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ResearchTechnologyCommand{
            first_barracks,
            aoe::Technology::pikeman,
        }
    );
    recorded.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-pikeman.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.economy(aoe::Player::blue).food == 35);
    require(simulation.economy(aoe::Player::blue).gold == 0);

    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        replayed.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-pikeman.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    for (int tick = 0; tick < 8; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.queue_unit_at(
        second_barracks,
        aoe::UnitKind::spearman
    ));
    require(replayed.queue_unit_at(
        second_barracks,
        aoe::UnitKind::spearman
    ));
    require(loaded.queue_unit_at(
        second_barracks,
        aoe::UnitKind::spearman
    ));
    for (int tick = 13; tick < technology_rules.research_ticks; ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::pikeman
    ));
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::pikeman
    ));
    const auto upgraded = std::ranges::find_if(
        simulation.units(),
        [spearman](const aoe::Unit& unit) { return unit.id == spearman; }
    );
    require(upgraded != simulation.units().end());
    require(upgraded->kind == aoe::UnitKind::pikeman);
    require(upgraded->hit_points == 45);
    require(upgraded->attack == 4);
    require(
        simulation.buildings()[1].production_queue.front().kind ==
        aoe::UnitKind::pikeman
    );
    simulation.update();
    simulation.update();
    require(simulation.units().back().kind == aoe::UnitKind::pikeman);

    aoe::Simulation counter(aoe::GameMap(8, 6));
    const aoe::EntityId pikeman = counter.add_unit(
        aoe::UnitKind::pikeman,
        aoe::Player::blue,
        {2, 2}
    );
    counter.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {3, 2}
    );
    require(counter.command_unit(pikeman, {3, 2}));
    counter.update();
    require(counter.units()[1].hit_points == 76);
}

void named_rules_define_balance_values() {
    const aoe::UnitRules& villager =
        aoe::rules_for(aoe::UnitKind::villager);
    const aoe::BuildingRules& barracks =
        aoe::rules_for(aoe::BuildingKind::barracks);
    const aoe::UnitRules& archer =
        aoe::rules_for(aoe::UnitKind::archer);
    const aoe::BuildingRules& house =
        aoe::rules_for(aoe::BuildingKind::house);

    require(villager.food_cost == 50);
    require(villager.trained_at == aoe::BuildingKind::town_center);
    require(barracks.wood_cost == 175);
    require(archer.attack_range == 4);
    require(archer.trained_at == aoe::BuildingKind::archery_range);
    require(archer.gold_cost == 45);
    require(archer.damage_class == aoe::DamageClass::pierce);
    require(house.population_support == 5);
    require(villager.movement_interval_ticks == 2);
    require(
        aoe::rules_for(aoe::UnitKind::scout_cavalry)
            .movement_interval_ticks == 1
    );
    require(
        aoe::can_train(
            aoe::BuildingKind::stable,
            aoe::UnitKind::knight
        )
    );
    const aoe::UnitRules& knight =
        aoe::rules_for(aoe::UnitKind::knight);
    require(knight.hit_points == 100);
    require(knight.attack == 10);
    require(knight.melee_armor == 2);
    require(knight.pierce_armor == 2);
    require(knight.food_cost == 60);
    require(knight.gold_cost == 75);
    require(knight.training_ticks == 12);
    require(knight.vision_range == 4);
    require(!aoe::can_train(
        aoe::BuildingKind::barracks,
        aoe::UnitKind::knight
    ));
    require(aoe::can_train(
        aoe::BuildingKind::barracks,
        aoe::UnitKind::militia
    ));
    require(
        aoe::rules_for(aoe::UnitKind::spearman).bonus_vs_cavalry ==
        15
    );
    require(aoe::rules_for(aoe::BuildingKind::blacksmith).wood_cost == 150);
    require(aoe::rules_for(aoe::BuildingKind::castle).stone_cost == 650);
    require(aoe::rules_for(aoe::BuildingKind::castle).hit_points == 4800);
    require(aoe::rules_for(aoe::BuildingKind::castle).melee_armor == 8);
    require(aoe::rules_for(aoe::BuildingKind::castle).pierce_armor == 11);
    require(aoe::rules_for(aoe::BuildingKind::castle).vision_range == 11);
    require(
        aoe::rules_for(aoe::BuildingKind::castle).population_support == 20
    );
    require(aoe::rules_for(aoe::BuildingKind::castle).footprint_width == 4);
    require(aoe::rules_for(aoe::BuildingKind::castle).footprint_height == 4);
    require(
        aoe::rules_for(aoe::BuildingKind::castle).minimum_attack_range == 1
    );
    require(aoe::rules_for(aoe::BuildingKind::university).wood_cost == 200);
    require(
        aoe::rules_for(aoe::BuildingKind::university).footprint_width == 3
    );
    require(
        aoe::rules_for(aoe::Technology::murder_holes).researched_at ==
        aoe::BuildingKind::university
    );
    require(aoe::rules_for(aoe::Technology::murder_holes).food_cost == 200);
    require(aoe::rules_for(aoe::Technology::murder_holes).stone_cost == 200);
    require(
        aoe::rules_for(aoe::BuildingKind::siege_workshop).wood_cost == 200
    );
    require(
        aoe::rules_for(aoe::UnitKind::battering_ram).wood_cost == 160
    );
    require(
        aoe::rules_for(aoe::UnitKind::battering_ram)
            .bonus_vs_buildings == 125
    );
    require(
        aoe::rules_for(aoe::UnitKind::battering_ram).pierce_armor == 180
    );
    require(
        aoe::rules_for(aoe::BuildingKind::castle).projectile_count == 5
    );
    require(
        aoe::rules_for(aoe::Technology::fletching).researched_at ==
        aoe::BuildingKind::blacksmith
    );
    require(
        aoe::rules_for(aoe::Technology::forging).researched_at ==
        aoe::BuildingKind::blacksmith
    );
}

void scout_cavalry_receives_original_automatic_age_bonuses() {
    const aoe::UnitRules& rules =
        aoe::rules_for(aoe::UnitKind::scout_cavalry);
    require(rules.hit_points == 45);
    require(rules.attack == 3);
    require(rules.melee_armor == 0);
    require(rules.pierce_armor == 2);
    require(rules.food_cost == 80);
    require(rules.gold_cost == 0);
    require(rules.training_ticks == 12);
    require(rules.vision_range == 4);

    aoe::Simulation simulation(aoe::GameMap(30, 24));
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {10, 10}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 20}
    );
    require(simulation.units().front().attack == 3);
    require(simulation.effective_unit_vision_range(
        simulation.units().front()
    ) == 4);
    require(simulation.is_visible(aoe::Player::blue, {14, 10}));
    require(!simulation.is_visible(aoe::Player::blue, {15, 10}));

    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(simulation.units().front().attack == 5);
    require(simulation.effective_unit_vision_range(
        simulation.units().front()
    ) == 6);
    require(simulation.is_visible(aoe::Player::blue, {16, 10}));
    require(!simulation.is_visible(aoe::Player::blue, {17, 10}));

    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(simulation.units().front().attack == 5);
    require(simulation.effective_unit_vision_range(
        simulation.units().front()
    ) == 8);
    require(simulation.is_visible(aoe::Player::blue, {18, 10}));
    require(!simulation.is_visible(aoe::Player::blue, {19, 10}));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-scout-age-bonuses.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.age(aoe::Player::blue) == aoe::Age::castle);
    require(loaded.units().front().attack == 5);
    require(loaded.effective_unit_vision_range(
        loaded.units().front()
    ) == 8);

    loaded.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(loaded.units().front().attack == 5);
    require(loaded.effective_unit_vision_range(
        loaded.units().front()
    ) == 10);
    require(loaded.is_visible(aoe::Player::blue, {20, 10}));
    require(!loaded.is_visible(aoe::Player::blue, {21, 10}));
}

void scout_cavalry_uses_exact_persisted_age_movement_rates() {
    const auto make_simulation = [](aoe::Age age, bool husbandry) {
        aoe::Simulation simulation(aoe::GameMap(100, 4));
        simulation.replace_ages(age, aoe::Age::dark);
        if (husbandry) {
            simulation.replace_technologies(
                aoe::Player::blue,
                {aoe::Technology::husbandry}
            );
        }
        simulation.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::blue,
            {1, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {99, 3}
        );
        require(simulation.command_unit(1, {90, 1}));
        return simulation;
    };

    aoe::Simulation dark = make_simulation(aoe::Age::dark, false);
    aoe::Simulation feudal = make_simulation(aoe::Age::feudal, false);
    aoe::Simulation castle_husbandry =
        make_simulation(aoe::Age::castle, true);

    for (int tick = 0; tick < 17; ++tick) {
        dark.update();
        feudal.update();
        castle_husbandry.update();
    }
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-scout-speed.save";
    aoe::save_game(castle_husbandry, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.units().front().movement_speed_remainder ==
        castle_husbandry.units().front().movement_speed_remainder
    );

    for (int tick = 17; tick < 64; ++tick) {
        dark.update();
        feudal.update();
        castle_husbandry.update();
        loaded.update();
    }
    require(dark.units().front().position == aoe::TilePosition(49, 1));
    require(feudal.units().front().position == aoe::TilePosition(63, 1));
    require(
        castle_husbandry.units().front().position ==
        aoe::TilePosition(69, 1)
    );
    require(
        loaded.units().front().position ==
        castle_husbandry.units().front().position
    );
    require(
        loaded.units().front().movement_speed_remainder ==
        castle_husbandry.units().front().movement_speed_remainder
    );

    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{1, {90, 1}});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-scout-speed.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);

    aoe::Simulation first(aoe::GameMap(100, 4));
    first.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    first.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::husbandry}
    );
    first.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {1, 1}
    );
    first.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {99, 3}
    );
    aoe::Simulation second = first;
    for (int tick = 0; tick < 64; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().front().position == aoe::TilePosition(69, 1));
    require(
        first.units().front().position ==
        second.units().front().position
    );
    require(
        first.units().front().movement_speed_remainder ==
        second.units().front().movement_speed_remainder
    );
}

void military_training_charges_food_and_gold_atomically() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    require(simulation.queue_unit_at(stable, aoe::UnitKind::knight));
    require(simulation.economy(aoe::Player::blue).food == 140);
    require(simulation.economy(aoe::Player::blue).gold == 125);
    require(simulation.queue_unit_at(range, aoe::UnitKind::archer));
    require(simulation.economy(aoe::Player::blue).wood == 75);
    require(simulation.economy(aoe::Player::blue).food == 140);
    require(simulation.economy(aoe::Player::blue).gold == 80);

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {100, 1000, 44, 0},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    const int food_before =
        simulation.economy(aoe::Player::blue).food;
    require(!simulation.queue_unit_at(range, aoe::UnitKind::archer));
    require(simulation.economy(aoe::Player::blue).food == food_before);
    require(simulation.economy(aoe::Player::blue).gold == 44);
}

void ages_gate_content_and_progress_deterministically() {
    aoe::Simulation simulation(aoe::GameMap(20, 14));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {16, 10}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {200, 4000, 4000, 200},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.age(aoe::Player::blue) == aoe::Age::dark);
    const int wood_before =
        simulation.economy(aoe::Player::blue).wood;
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::archery_range,
        {5, 2}
    ));
    require(simulation.economy(aoe::Player::blue).wood == wood_before);

    require(!simulation.advance_age_at(town_center));
    simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {6, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::blue,
        {7, 1}
    );
    require(simulation.advance_age_at(town_center));
    require(simulation.economy(aoe::Player::blue).food == 3500);
    const aoe::EntityId second_town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {10, 0}
    );
    require(!simulation.advance_age_at(second_town_center));
    require(simulation.economy(aoe::Player::blue).food == 3500);
    require(!simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-age-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.age(aoe::Player::blue) == aoe::Age::dark);
    require(
        loaded.buildings().front().age_research_ticks_remaining ==
        simulation.buildings().front().age_research_ticks_remaining
    );
    for (int tick = 0; tick < 15; ++tick) {
        loaded.update();
    }
    require(loaded.age(aoe::Player::blue) == aoe::Age::feudal);

    require(loaded.construct_building_at(
        builder,
        aoe::BuildingKind::archery_range,
        {5, 2}
    ));
    loaded.add_building(
        aoe::BuildingKind::blacksmith,
        aoe::Player::blue,
        {7, 2}
    );
    require(!loaded.advance_age_at(town_center));
    for (int tick = 0;
         tick <
            aoe::rules_for(
                aoe::BuildingKind::archery_range
            ).construction_ticks;
         ++tick) {
        loaded.update();
    }

    aoe::Replay recorded;
    recorded.record(
        loaded.tick_number(),
        aoe::AdvanceAgeCommand{town_center}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-age-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replay_copy = loaded;
    recorded.apply_current_tick(loaded);
    replayed.apply_current_tick(replay_copy);
    require(
        loaded.economy(aoe::Player::blue).food ==
        replay_copy.economy(aoe::Player::blue).food
    );
    require(
        loaded.economy(aoe::Player::blue).gold ==
        replay_copy.economy(aoe::Player::blue).gold
    );
    for (int tick = 0; tick < 30; ++tick) {
        loaded.update();
        replay_copy.update();
    }
    require(loaded.age(aoe::Player::blue) == aoe::Age::castle);
    require(replay_copy.age(aoe::Player::blue) == aoe::Age::castle);

    loaded.add_building(
        aoe::BuildingKind::stable,
        aoe::Player::blue,
        {8, 2}
    );
    const auto stable = std::ranges::find_if(
        loaded.buildings(),
        [](const aoe::Building& building) {
            return building.kind == aoe::BuildingKind::stable;
        }
    );
    require(stable != loaded.buildings().end());
    require(loaded.queue_unit_at(stable->id, aoe::UnitKind::knight));
    loaded.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 7}
    );
    require(loaded.advance_age_at(town_center));
    for (int tick = 0; tick < 40; ++tick) {
        loaded.update();
    }
    require(loaded.age(aoe::Player::blue) == aoe::Age::imperial);
    require(!loaded.advance_age_at(town_center));
}

void archery_range_trains_archer() {
    aoe::Simulation simulation(aoe::GameMap(6, 6));
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    const aoe::EntityId range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {4, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {5, 5}
    );
    require(simulation.queue_unit_at(range, aoe::UnitKind::archer));
    const std::size_t original_units = simulation.units().size();
    for (int tick = 0;
         tick < aoe::rules_for(aoe::UnitKind::archer).training_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.units().size() == original_units + 1);
    require(simulation.units().back().kind == aoe::UnitKind::archer);
}

void blacksmith_construction_requires_feudal_age() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 500},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::blacksmith,
        {3, 2}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 500);
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::blacksmith,
        {3, 2}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 350);
    require(
        simulation.buildings().back().kind ==
        aoe::BuildingKind::blacksmith
    );
    for (int tick = 0;
         tick <
            aoe::rules_for(
                aoe::BuildingKind::blacksmith
            ).construction_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.buildings().back().completed());
}

void castle_uses_atomic_stone_cost_and_persists() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 649},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::castle,
        {3, 2}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 500);
    require(simulation.economy(aoe::Player::blue).stone == 649);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 700},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::castle,
        {3, 2}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 500);
    require(simulation.economy(aoe::Player::blue).stone == 50);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-castle-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().back().kind == aoe::BuildingKind::castle);
    require(
        loaded.buildings().back().construction_ticks_remaining ==
        simulation.buildings().back().construction_ticks_remaining
    );
    for (int tick = 0; tick < 35; ++tick) {
        loaded.update();
    }
    require(loaded.buildings().back().completed());

    aoe::Simulation first(aoe::GameMap(8, 8));
    first.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const aoe::EntityId replay_builder = first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 2}
    );
    first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    first.replace_state(
        first.units(),
        first.buildings(),
        {500, 500, 500, 700},
        first.economy(aoe::Player::red),
        0
    );
    aoe::Simulation second = first;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ConstructBuildingCommand{
            replay_builder,
            aoe::BuildingKind::castle,
            {3, 2},
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-castle-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    recorded.apply_current_tick(first);
    replayed.apply_current_tick(second);
    require(first.buildings().back().kind == aoe::BuildingKind::castle);
    require(second.buildings().back().kind == aoe::BuildingKind::castle);
    require(first.economy(aoe::Player::blue).stone == 50);
    require(second.economy(aoe::Player::blue).stone == 50);
}

void castle_defense_fires_delayed_persistent_arrows() {
    aoe::Simulation simulation(aoe::GameMap(20, 20));
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {5, 5}
    );
    const aoe::EntityId target = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {11, 5}
    );

    simulation.update();
    require(simulation.projectiles().size() == 5);
    require(simulation.projectiles().front().target == target);
    require(simulation.projectiles().front().visual_lane == -2);
    require(simulation.projectiles().back().visual_lane == 2);
    require(simulation.projectiles().front().damage == 11);
    require(
        simulation.projectiles().front().damage_class ==
        aoe::DamageClass::pierce
    );
    require(simulation.units().front().hit_points == 100);
    require(simulation.buildings().front().attack_cooldown == 5);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-castle-arrow.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().front().attack_cooldown == 5);
    require(loaded.projectiles().size() == 5);
    require(loaded.projectiles().front().visual_lane == -2);
    require(loaded.projectiles().back().visual_lane == 2);

    simulation.update();
    loaded.update();
    require(simulation.units().front().hit_points == 55);
    require(loaded.units().front().hit_points == 55);

    simulation.update();
    simulation.update();
    simulation.update();
    require(simulation.projectiles().empty());
    require(simulation.buildings().front().attack_cooldown == 1);
    simulation.update();
    require(simulation.projectiles().empty());
    require(simulation.buildings().front().attack_cooldown == 0);

    aoe::Simulation distant(aoe::GameMap(20, 20));
    distant.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {1, 1}
    );
    distant.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {15, 15}
    );
    distant.update();
    require(distant.projectiles().empty());
    require(distant.buildings().front().attack_cooldown == 0);
}

void fletching_upgrades_castle_attack_and_range() {
    aoe::Simulation simulation(aoe::GameMap(24, 14));
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId target = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {16, 5}
    );
    simulation.update();
    require(simulation.projectiles().empty());

    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fletching}
    );
    simulation.update();
    require(simulation.projectiles().size() == 5);
    require(simulation.projectiles().front().target == target);
    require(simulation.projectiles().front().damage == 12);

    const auto path =
        std::filesystem::temp_directory_path() /
        "aoe-castle-fletching.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::fletching
    ));
    require(loaded.projectiles().front().damage == 12);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(simulation.units().front().hit_points == 50);
    require(loaded.units().front().hit_points == 50);
}

void castle_defense_targets_enemy_buildings() {
    aoe::Simulation simulation(aoe::GameMap(16, 16));
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId house = simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 4}
    );

    simulation.update();
    require(simulation.projectiles().size() == 5);
    require(simulation.projectiles().front().target == house);
    require(simulation.projectiles().front().target_is_building);
    require(
        simulation.buildings()[1].hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points
    );
    simulation.update();
    simulation.update();
    require(
        simulation.buildings()[1].hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points -
            (aoe::rules_for(aoe::BuildingKind::castle).attack -
             aoe::rules_for(aoe::BuildingKind::house).pierce_armor) *
                aoe::rules_for(aoe::BuildingKind::castle).projectile_count
    );
    simulation.update();
    simulation.update();
    simulation.update();
    require(simulation.projectiles().size() == 5);
}

void castle_minimum_range_protects_adjacent_attackers() {
    aoe::Simulation simulation(aoe::GameMap(16, 12));
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId adjacent = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {8, 5}
    );
    const aoe::EntityId farther = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {10, 5}
    );
    simulation.update();
    require(simulation.projectiles().size() == 5);
    require(simulation.projectiles().front().target == farther);
    require(simulation.projectiles().front().target != adjacent);

    aoe::Simulation dead_zone(aoe::GameMap(12, 12));
    dead_zone.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    dead_zone.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {8, 5}
    );
    dead_zone.update();
    require(dead_zone.projectiles().empty());
    require(dead_zone.buildings().front().attack_cooldown == 0);
}

void university_researches_persistent_murder_holes() {
    aoe::Simulation simulation(aoe::GameMap(24, 16));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {20, 12}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 500},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::university,
        {2, 1}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 300);
    for (int tick = 0;
         tick < aoe::rules_for(
             aoe::BuildingKind::university
         ).construction_ticks;
         ++tick) {
        simulation.update();
    }
    const aoe::EntityId university = simulation.buildings().back().id;
    require(simulation.buildings().back().completed());

    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {8, 4}
    );
    const aoe::EntityId adjacent = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {12, 5}
    );
    simulation.update();
    require(simulation.projectiles().empty());

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {300, 500, 500,
         aoe::rules_for(aoe::Technology::murder_holes).stone_cost - 1},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(!simulation.research_technology_at(
        university,
        aoe::Technology::murder_holes
    ));
    require(simulation.economy(aoe::Player::blue).food == 500);
    require(
        simulation.economy(aoe::Player::blue).stone ==
        aoe::rules_for(aoe::Technology::murder_holes).stone_cost - 1
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {300, 500, 500,
         aoe::rules_for(aoe::Technology::murder_holes).stone_cost},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );

    aoe::Simulation replay_first = simulation;
    aoe::Simulation replay_second = simulation;
    aoe::Replay recorded;
    recorded.record(
        simulation.tick_number(),
        aoe::ResearchTechnologyCommand{
            university,
            aoe::Technology::murder_holes,
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-murder-holes-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    recorded.apply_current_tick(replay_first);
    replayed.apply_current_tick(replay_second);
    for (int tick = 0; tick < 18; ++tick) {
        replay_first.update();
        replay_second.update();
    }
    require(replay_first.has_technology(
        aoe::Player::blue,
        aoe::Technology::murder_holes
    ));
    require(replay_second.has_technology(
        aoe::Player::blue,
        aoe::Technology::murder_holes
    ));
    require(
        replay_first.economy(aoe::Player::blue).stone ==
        replay_second.economy(aoe::Player::blue).stone
    );

    require(simulation.research_technology_at(
        university,
        aoe::Technology::murder_holes
    ));
    require(simulation.economy(aoe::Player::blue).food == 300);
    require(simulation.economy(aoe::Player::blue).stone == 0);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }

    const auto path =
        std::filesystem::temp_directory_path() /
        "aoe-murder-holes-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.buildings()[1].kind == aoe::BuildingKind::university);
    require(
        loaded.buildings()[1].technology_research_ticks_remaining ==
        simulation.buildings()[1].technology_research_ticks_remaining
    );
    for (int tick = 0; tick < 13; ++tick) {
        loaded.update();
    }
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::murder_holes
    ));
    loaded.update();
    require(loaded.projectiles().size() == 5);
    require(loaded.projectiles().front().target == adjacent);
}

void siege_workshop_trains_persistent_battering_rams() {
    aoe::Simulation simulation(aoe::GameMap(24, 16));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {20, 12}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 500},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::siege_workshop,
        {2, 1}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 500);
    simulation.add_building(
        aoe::BuildingKind::blacksmith,
        aoe::Player::blue,
        {5, 5}
    );
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::siege_workshop,
        {2, 1}
    ));
    require(simulation.economy(aoe::Player::blue).wood == 300);
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
    }
    const aoe::EntityId workshop = simulation.buildings().back().id;
    require(simulation.buildings().back().completed());

    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {159, 500, 75, 500},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(!simulation.queue_unit_at(
        workshop,
        aoe::UnitKind::battering_ram
    ));
    require(simulation.economy(aoe::Player::blue).wood == 159);
    require(simulation.economy(aoe::Player::blue).gold == 75);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {160, 500, 75, 500},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );

    aoe::Simulation replay_first = simulation;
    aoe::Simulation replay_second = simulation;
    aoe::Replay recorded;
    recorded.record(
        simulation.tick_number(),
        aoe::QueueUnitCommand{
            workshop,
            aoe::UnitKind::battering_ram,
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-ram-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    recorded.apply_current_tick(replay_first);
    replayed.apply_current_tick(replay_second);
    require(
        replay_first.buildings().back().production_queue.front().kind ==
        aoe::UnitKind::battering_ram
    );
    require(
        replay_second.buildings().back().production_queue.front().kind ==
        aoe::UnitKind::battering_ram
    );

    require(simulation.queue_unit_at(
        workshop,
        aoe::UnitKind::battering_ram
    ));
    require(simulation.economy(aoe::Player::blue).wood == 0);
    require(simulation.economy(aoe::Player::blue).gold == 0);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-ram-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.buildings().back().kind ==
        aoe::BuildingKind::siege_workshop
    );
    require(
        loaded.buildings().back().production_queue.front().kind ==
        aoe::UnitKind::battering_ram
    );
    for (int tick = 0; tick < 13; ++tick) {
        loaded.update();
    }
    require(loaded.units().back().kind == aoe::UnitKind::battering_ram);
    require(loaded.units().back().hit_points == 175);
}

void battering_ram_counters_buildings_and_resists_arrows() {
    aoe::Simulation siege(aoe::GameMap(14, 10));
    const aoe::EntityId ram = siege.add_unit(
        aoe::UnitKind::battering_ram,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId house = siege.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {5, 4}
    );
    require(siege.command_unit(ram, {5, 4}));
    siege.update();
    require(siege.buildings().front().id == house);
    require(
        siege.buildings().front().hit_points ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points -
            aoe::rules_for(aoe::UnitKind::battering_ram).attack -
            aoe::rules_for(aoe::UnitKind::battering_ram).bonus_vs_buildings +
            aoe::rules_for(aoe::BuildingKind::house).melee_armor
    );

    aoe::Simulation arrows(aoe::GameMap(14, 10));
    const aoe::EntityId armored_ram = arrows.add_unit(
        aoe::UnitKind::battering_ram,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId archer = arrows.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::red,
        {8, 4}
    );
    require(!arrows.command_unit(armored_ram, {8, 4}));
    require(arrows.command_unit(archer, {4, 4}));
    arrows.update();
    arrows.update();
    arrows.update();
    arrows.update();
    arrows.update();
    require(arrows.units().front().id == armored_ram);
    require(arrows.units().front().hit_points == 174);
}

void university_and_workshop_unlock_imperial_age() {
    aoe::Simulation simulation(aoe::GameMap(24, 16));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::university,
        aoe::Player::blue,
        {2, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::siege_workshop,
        aoe::Player::blue,
        {6, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {20, 12}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 1000, 800, 500},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.advance_age_at(town_center));
    for (int tick = 0; tick < 40; ++tick) {
        simulation.update();
    }
    require(simulation.age(aoe::Player::blue) == aoe::Age::imperial);
}

void castle_provides_original_population_support() {
    aoe::Simulation simulation(aoe::GameMap(12, 12));
    require(simulation.population_capacity(aoe::Player::blue) == 0);
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {11, 11}
    );
    require(simulation.population_capacity(aoe::Player::blue) == 20);
    require(simulation.buildings().front().hit_points == 4800);
}

void castle_footprint_blocks_placement_and_routes_units() {
    aoe::Simulation simulation(aoe::GameMap(12, 12));
    simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {8, 8}
    );

    bool unit_rejected{};
    try {
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {7, 7}
        );
    } catch (const std::invalid_argument&) {
        unit_rejected = true;
    }
    require(unit_rejected);

    bool building_rejected{};
    try {
        simulation.add_building(
            aoe::BuildingKind::house,
            aoe::Player::blue,
            {6, 6}
        );
    } catch (const std::invalid_argument&) {
        building_rejected = true;
    }
    require(building_rejected);
    require(simulation.select_building_at({7, 7}, aoe::Player::blue));

    require(simulation.command_unit(villager, {9, 5}));
    for (int tick = 0; tick < 40; ++tick) {
        simulation.update();
    }
    require(simulation.units().front().position == aoe::TilePosition(9, 5));
    for (const aoe::TilePosition step : simulation.units().front().path) {
        require(step.x < 4 || step.x > 7 || step.y < 4 || step.y > 7);
    }
}

void castle_footprint_drives_range_vision_and_projectile_edges() {
    aoe::Simulation defense(aoe::GameMap(20, 12));
    defense.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId target = defense.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 5}
    );
    require(defense.is_visible(aoe::Player::blue, {18, 5}));
    require(!defense.is_visible(aoe::Player::blue, {19, 5}));
    defense.update();
    require(defense.projectiles().size() == 5);
    require(defense.projectiles().front().target == target);
    require(
        defense.projectiles().front().origin ==
        aoe::TilePosition(7, 5)
    );

    aoe::Simulation assault(aoe::GameMap(14, 12));
    const aoe::EntityId castle = assault.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::red,
        {4, 4}
    );
    const aoe::EntityId archer = assault.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {10, 5}
    );
    require(assault.command_unit(archer, {7, 5}));
    assault.update();
    assault.update();
    assault.update();
    const auto arrow = std::ranges::find_if(
        assault.projectiles(),
        [castle](const aoe::Projectile& projectile) {
            return projectile.owner == aoe::Player::blue &&
                projectile.target == castle;
        }
    );
    require(arrow != assault.projectiles().end());
    require(arrow->destination == aoe::TilePosition(7, 5));
}

void explicit_building_attack_persists_until_target_enters_vision() {
    aoe::Simulation simulation(aoe::GameMap(30, 12));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {22, 4}
    );
    const int initial_hit_points = simulation.buildings()[0].hit_points;
    require(!simulation.is_building_visible(
        aoe::Player::blue,
        simulation.buildings()[0]
    ));
    require(simulation.command_unit(archer, {22, 4}));

    simulation.update();
    require(simulation.units()[0].attack_target_id ==
            simulation.buildings()[0].id);
    require(simulation.units()[0].attack_target_is_building);
    require(!simulation.units()[0].attack_target_auto);
    for (int tick = 0; tick < 100; ++tick) simulation.update();

    require(simulation.buildings()[0].hit_points < initial_hit_points);
}

void houses_raise_population_cap_only_after_completion() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {0, 4}
    );
    simulation.add_unit(aoe::UnitKind::villager, aoe::Player::blue, {1, 4});
    simulation.add_unit(aoe::UnitKind::villager, aoe::Player::blue, {2, 4});
    simulation.add_unit(aoe::UnitKind::knight, aoe::Player::blue, {3, 4});
    simulation.add_unit(aoe::UnitKind::archer, aoe::Player::blue, {4, 4});
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );

    require(simulation.population(aoe::Player::blue) == 5);
    require(simulation.population_capacity(aoe::Player::blue) == 5);
    require(!simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::house,
        {0, 5}
    ));
    require(simulation.population_capacity(aoe::Player::blue) == 5);
    for (int tick = 0;
         tick < aoe::rules_for(aoe::BuildingKind::house).construction_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.population_capacity(aoe::Player::blue) == 10);
    require(simulation.buildings().back().builder_id == 0);
    require(simulation.buildings().back().builder_ids.empty());
    const auto path =
        std::filesystem::temp_directory_path() / "aoe-population-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.population(aoe::Player::blue) == 5);
    require(loaded.population_capacity(aoe::Player::blue) == 10);
    require(loaded.buildings().back().kind == aoe::BuildingKind::house);
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {6, 2}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {100, 1000},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    for (int order = 0; order < 4; ++order) {
        require(simulation.queue_unit_at(
            town_center,
            aoe::UnitKind::villager
        ));
    }
    require(!simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
}

void housing_loss_stalls_completed_production() {
    aoe::Simulation simulation(aoe::GameMap(10, 6));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {7, 2}
    );
    for (int x = 0; x < 6; ++x) {
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {x + 1, 4}
        );
    }
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {8, 2}
    );
    std::vector<aoe::Building> weakened = simulation.buildings();
    weakened.back().hit_points = 1;
    simulation.replace_state(
        simulation.units(),
        std::move(weakened),
        {100, 1000},
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
    require(simulation.command_unit(attacker, {7, 2}));
    simulation.update();
    require(simulation.population_capacity(aoe::Player::blue) == 5);

    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
    }
    require(simulation.population(aoe::Player::blue) == 6);
    require(
        simulation.buildings().front().production_queue.size() == 1
    );
    require(
        simulation.buildings().front()
            .production_queue.front().ticks_remaining == 0
    );
}

void archer_projectile_has_delayed_persisted_impact() {
    aoe::Simulation simulation(aoe::GameMap(8, 5));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {4, 2}
    );
    require(
        simulation.set_unit_stance(knight, aoe::UnitStance::passive)
    );
    require(simulation.command_unit(archer, {4, 2}));
    for (int tick = 0; tick < 3; ++tick) simulation.update();
    require(simulation.units().front().position == aoe::TilePosition(1, 2));
    require(simulation.units().back().hit_points == 100);
    require(simulation.projectiles().size() == 1);
    require(
        simulation.projectiles().front().source_entity_id == archer
    );

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-projectile-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.projectiles().size() == 1);
    require(
        loaded.projectiles().front().ticks_remaining ==
        simulation.projectiles().front().ticks_remaining
    );
    require(
        loaded.projectiles().front().damage_class ==
        aoe::DamageClass::pierce
    );
    require(loaded.projectiles().front().source_entity_id == archer);

    simulation.update();
    loaded.update();
    require(simulation.units().back().hit_points == 100);
    require(loaded.units().back().hit_points == 100);
    simulation.update();
    loaded.update();
    require(simulation.units().back().hit_points == 98);
    require(loaded.units().back().hit_points == 98);
    require(simulation.projectiles().empty());
    require(loaded.projectiles().empty());
    require(simulation.impact_effects().size() == 1);
    require(loaded.impact_effects().size() == 1);
    require(!simulation.impact_effects().front().splash);
    require(
        simulation.impact_effects().front().source_entity_id == archer
    );
    require(
        simulation.impact_effects().front().position ==
        aoe::TilePosition(4, 2)
    );

    const auto impact_path =
        std::filesystem::temp_directory_path() / "aoe-impact-test.save";
    aoe::save_game(simulation, impact_path);
    aoe::Simulation impact_loaded = aoe::load_game(impact_path);
    std::filesystem::remove(impact_path);
    require(impact_loaded.impact_effects().size() == 1);
    require(
        impact_loaded.impact_effects().front().ticks_remaining ==
        simulation.impact_effects().front().ticks_remaining
    );
    require(
        impact_loaded.impact_effects().front().source_entity_id == archer
    );
    require(simulation.set_unit_stance(archer, aoe::UnitStance::passive));
    require(impact_loaded.set_unit_stance(archer, aoe::UnitStance::passive));
    require(simulation.stop_unit(archer));
    require(impact_loaded.stop_unit(archer));
    for (int tick = 0; tick < 4; ++tick) {
        simulation.update();
        impact_loaded.update();
    }
    require(simulation.impact_effects().empty());
    require(impact_loaded.impact_effects().empty());
}

void direct_projectiles_respect_diplomacy_at_impact() {
    aoe::Simulation simulation(aoe::GameMap(8, 5));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {4, 2}
    );
    require(simulation.set_unit_stance(knight, aoe::UnitStance::passive));
    require(simulation.command_unit(archer, {4, 2}));
    for (int tick = 0; tick < 3; ++tick) simulation.update();
    require(simulation.projectiles().size() == 1);
    require(simulation.set_diplomacy(
        aoe::Player::blue,
        aoe::Player::red,
        aoe::Diplomacy::neutral
    ));
    for (int tick = 0; tick < 3; ++tick) simulation.update();
    require(simulation.units().back().hit_points == 100);
    require(simulation.projectiles().empty());
}

void radial_combat_ranges_use_collision_box_borders() {
    aoe::Simulation unit_range(aoe::GameMap(12, 10));
    const auto archer = unit_range.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    const auto diagonal = unit_range.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {4, 3}
    );
    require(unit_range.set_unit_stance(
        diagonal, aoe::UnitStance::passive
    ));
    require(unit_range.command_unit(archer, {4, 3}));
    for (int tick = 0; tick < 3; ++tick) unit_range.update();
    require(unit_range.projectiles().size() == 1);
    require(unit_range.units().front().position == aoe::TilePosition{1, 1});

    aoe::Simulation building_range(aoe::GameMap(16, 12));
    const auto building_archer = building_range.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    const auto castle = building_range.add_building(
        aoe::BuildingKind::castle, aoe::Player::red, {4, 3}
    );
    building_range.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {14, 10}
    );
    require(building_range.command_unit(building_archer, {4, 3}));
    for (int tick = 0; tick < 3; ++tick) building_range.update();
    require(std::ranges::any_of(
        building_range.projectiles(),
        [castle](const aoe::Projectile& projectile) {
            return projectile.owner == aoe::Player::blue &&
                projectile.target == castle;
        }
    ));

    aoe::Simulation defense(aoe::GameMap(18, 14));
    defense.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {1, 1}
    );
    const auto target = defense.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {7, 5}
    );
    require(defense.set_unit_stance(target, aoe::UnitStance::passive));
    defense.update();
    require(defense.projectiles().size() == 1);
    require(defense.projectiles().front().target == target);

    aoe::Simulation building_defense(aoe::GameMap(18, 14));
    building_defense.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {1, 1}
    );
    const auto enemy_castle = building_defense.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::red,
        {7, 5}
    );
    building_defense.update();
    require(std::ranges::any_of(
        building_defense.projectiles(),
        [enemy_castle](const aoe::Projectile& projectile) {
            return projectile.owner == aoe::Player::blue &&
                projectile.target == enemy_castle;
        }
    ));
}

void radial_minimum_ranges_and_religious_ranges_are_circular() {
    aoe::Simulation ground(aoe::GameMap(14, 12));
    const auto mangonel = ground.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {1, 1}
    );
    ground.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {13, 11}
    );
    require(!ground.command_attack_ground(mangonel, {3, 3}));
    require(ground.command_attack_ground(mangonel, {6, 5}));
    ground.update();
    require(ground.projectiles().size() == 1);

    aoe::Simulation religion(aoe::GameMap(12, 10));
    const auto monk = religion.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {1, 1}
    );
    const auto enemy = religion.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {4, 3}
    );
    require(religion.command_convert(monk, enemy));

    aoe::Simulation healing(aoe::GameMap(12, 10));
    const auto healer = healing.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {1, 1}
    );
    const auto patient = healing.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {4, 3}
    );
    healing.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 8}
    );
    auto wounded = healing.units();
    wounded[1].hit_points = 90;
    healing.replace_state(
        std::move(wounded),
        healing.buildings(),
        healing.economy(aoe::Player::blue),
        healing.economy(aoe::Player::red),
        0
    );
    require(healing.command_heal(healer, patient));
    healing.update();
    require(healing.units()[1].hit_points == 91);
}

void radial_acquisition_prefers_true_nearest_target() {
    aoe::Simulation simulation(aoe::GameMap(12, 10));
    const auto archer = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    const auto radial_nearest = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {4, 4}
    );
    const auto manhattan_nearest = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {6, 1}
    );
    require(simulation.set_unit_stance(
        radial_nearest, aoe::UnitStance::passive
    ));
    require(simulation.set_unit_stance(
        manhattan_nearest, aoe::UnitStance::passive
    ));
    simulation.update();
    const auto attacker = std::ranges::find(
        simulation.units(), archer, &aoe::Unit::id
    );
    require(attacker != simulation.units().end());
    require(attacker->attack_target_id == radial_nearest);
}

void projectile_travel_uses_radial_distance_and_bounded_speed_rounding() {
    const auto launch_mangonel = [](aoe::TilePosition target_position) {
        aoe::Simulation simulation(aoe::GameMap(14, 12));
        const auto mangonel = simulation.add_unit(
            aoe::UnitKind::mangonel, aoe::Player::blue, {1, 1}
        );
        const auto target = simulation.add_unit(
            aoe::UnitKind::knight,
            aoe::Player::red,
            target_position
        );
        require(simulation.set_unit_stance(
            target, aoe::UnitStance::passive
        ));
        require(simulation.command_unit(mangonel, target_position));
        simulation.update();
        require(simulation.projectiles().size() == 1);
        return simulation;
    };
    aoe::Simulation cardinal = launch_mangonel({6, 1});
    aoe::Simulation diagonal = launch_mangonel({4, 5});
    require(cardinal.projectiles().front().total_ticks == 3);
    require(
        diagonal.projectiles().front().total_ticks ==
        cardinal.projectiles().front().total_ticks
    );

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-radial-projectile.save";
    aoe::save_game(diagonal, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        restored.projectiles().front().total_ticks ==
        diagonal.projectiles().front().total_ticks
    );
    require(
        restored.projectiles().front().ticks_remaining ==
        diagonal.projectiles().front().ticks_remaining
    );

    aoe::Simulation building_target(aoe::GameMap(16, 12));
    const auto archer = building_target.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    const auto castle = building_target.add_building(
        aoe::BuildingKind::castle, aoe::Player::red, {4, 3}
    );
    building_target.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {14, 10}
    );
    require(building_target.command_unit(archer, {4, 3}));
    for (int tick = 0; tick < 3; ++tick) building_target.update();
    const auto archer_shot = std::ranges::find_if(
        building_target.projectiles(),
        [castle](const aoe::Projectile& projectile) {
            return projectile.owner == aoe::Player::blue &&
                projectile.target == castle;
        }
    );
    require(archer_shot != building_target.projectiles().end());
    require(archer_shot->origin == aoe::TilePosition{1, 1});
    require(archer_shot->destination == aoe::TilePosition{4, 3});
    require(archer_shot->total_ticks == 2);

    aoe::Simulation bombard(aoe::GameMap(14, 10));
    bombard.add_building(
        aoe::BuildingKind::bombard_tower, aoe::Player::blue, {1, 1}
    );
    const auto bombard_target = bombard.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {7, 1}
    );
    require(bombard.set_unit_stance(
        bombard_target, aoe::UnitStance::passive
    ));
    bombard.update();
    require(bombard.projectiles().size() == 1);
    require(bombard.projectiles().front().projectile_speed_tenths == 30);
    require(bombard.projectiles().front().total_ticks == 10);

    aoe::Simulation replayed(aoe::GameMap(14, 12));
    const auto replay_mangonel = replayed.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {1, 1}
    );
    const auto replay_target = replayed.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {4, 5}
    );
    require(replayed.set_unit_stance(
        replay_target, aoe::UnitStance::passive
    ));
    aoe::Replay replay;
    replay.record(
        0,
        aoe::MoveUnitCommand{replay_mangonel, {4, 5}}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-radial-projectile.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    replayed.update();
    require(replayed.projectiles().front().total_ticks == 3);

    aoe::Simulation minimum(aoe::GameMap(8, 6));
    const auto close_archer = minimum.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    const auto close_target = minimum.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {2, 1}
    );
    require(minimum.set_unit_stance(
        close_target, aoe::UnitStance::passive
    ));
    require(minimum.command_unit(close_archer, {2, 1}));
    for (int tick = 0; tick < 3; ++tick) minimum.update();
    require(minimum.projectiles().front().total_ticks == 1);
}

void archer_projectile_tracks_moving_target_deterministically() {
    aoe::Simulation simulation(aoe::GameMap(10, 5));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::red,
        {4, 2}
    );
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::ballistics}
    );
    require(
        simulation.set_unit_stance(scout, aoe::UnitStance::passive)
    );
    require(simulation.command_unit(scout, {7, 2}));
    require(simulation.command_unit(archer, {4, 2}));
    const int scout_hit_points = simulation.units().back().hit_points;
    for (int tick = 0; tick < 3; ++tick) simulation.update();
    require(simulation.units().back().position == aoe::TilePosition(6, 2));
    require(simulation.projectiles().size() == 1);
    require(
        simulation.projectiles().front().destination ==
        aoe::TilePosition(6, 2)
    );

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-tracking-projectile.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);

    simulation.update();
    loaded.update();
    require(
        simulation.projectiles().front().destination ==
        aoe::TilePosition(7, 2)
    );
    require(
        loaded.projectiles().front().destination ==
        simulation.projectiles().front().destination
    );
    simulation.update();
    loaded.update();
    require(simulation.projectiles().empty());
    require(loaded.projectiles().empty());
    require(simulation.units().back().position == aoe::TilePosition(7, 2));
    require(simulation.units().back().hit_points < scout_hit_points);
    require(
        loaded.units().back().hit_points ==
        simulation.units().back().hit_points
    );
    require(
        simulation.impact_effects().front().position ==
        aoe::TilePosition(7, 2)
    );
}

void armor_reduces_damage_but_preserves_minimum_hit() {
    aoe::Simulation simulation(aoe::GameMap(6, 6));
    const aoe::EntityId blue = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {2, 1}
    );
    require(simulation.command_unit(blue, {2, 1}));
    simulation.update();
    require(simulation.units().back().hit_points == 92);

    aoe::Simulation minimum(aoe::GameMap(6, 5));
    const aoe::EntityId villager = minimum.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    minimum.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {2, 1}
    );
    require(minimum.command_unit(villager, {2, 1}));
    minimum.update();
    require(minimum.buildings().front().hit_points == 2399);
}

void unit_death_effect_collapses_persists_and_expires() {
    aoe::Simulation simulation(aoe::GameMap(10, 6));
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId victim = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {2, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {8, 4}
    );
    std::vector<aoe::Unit> units = simulation.units();
    for (aoe::Unit& unit : units) {
        unit.stance = aoe::UnitStance::passive;
        if (unit.id == victim) {
            unit.hit_points = 1;
        }
    }
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.command_unit(knight, {2, 1}));
    simulation.update();
    require(simulation.units().size() == 2);
    require(simulation.death_effects().size() == 1);
    require(simulation.rubble_effects().empty());
    require(
        simulation.death_effects().front().position ==
        aoe::TilePosition(2, 1)
    );
    require(
        simulation.death_effects().front().kind ==
        aoe::UnitKind::villager
    );
    require(
        simulation.death_effects().front().owner == aoe::Player::red
    );
    require(
        simulation.death_effects().front().entity_id == victim
    );

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-unit-death-effect.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.death_effects().size() == 1);
    require(
        loaded.death_effects().front().ticks_remaining ==
        simulation.death_effects().front().ticks_remaining
    );
    require(loaded.death_effects().front().entity_id == victim);

    require(simulation.stop_unit(knight));
    require(loaded.stop_unit(knight));
    for (int tick = 0; tick < 18; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(simulation.death_effects().empty());
    require(loaded.death_effects().empty());
}

void destroyed_building_leaves_persistent_nonblocking_rubble() {
    aoe::Simulation simulation(aoe::GameMap(12, 7));
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId destroyed_house = simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {2, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 4}
    );
    std::vector<aoe::Unit> units = simulation.units();
    units.front().stance = aoe::UnitStance::passive;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings.front().hit_points = 1;
    simulation.replace_state(
        std::move(units),
        std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.command_unit(knight, {2, 1}));
    simulation.update();
    require(simulation.buildings().size() == 1);
    require(simulation.rubble_effects().size() == 1);
    require(
        simulation.rubble_effects().front().position ==
        aoe::TilePosition(2, 1)
    );
    require(
        simulation.rubble_effects().front().kind ==
        aoe::BuildingKind::house
    );
    require(
        simulation.rubble_effects().front().entity_id ==
        destroyed_house
    );
    require(simulation.command_unit(knight, {2, 1}));

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-building-rubble.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.rubble_effects().size() == 1);
    require(
        loaded.rubble_effects().front().ticks_remaining ==
        simulation.rubble_effects().front().ticks_remaining
    );
    require(
        loaded.rubble_effects().front().entity_id == destroyed_house
    );
    require(loaded.command_unit(knight, {2, 1}));

    require(simulation.stop_unit(knight));
    require(loaded.stop_unit(knight));
    for (int tick = 0; tick < 30; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(simulation.rubble_effects().empty());
    require(loaded.rubble_effects().empty());
}

void villager_constructs_stable_and_trains_knight() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {300, 200, 200, 100},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.construct_building(aoe::BuildingKind::stable, {2, 6}));
    require(simulation.economy(aoe::Player::blue).wood == 125);
    require(simulation.select_building_at({2, 6}, aoe::Player::blue));
    require(!simulation.queue_unit(aoe::UnitKind::knight));
    require(!simulation.buildings().back().completed());

    for (int tick = 0;
         tick < aoe::rules_for(aoe::BuildingKind::stable).construction_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.buildings().back().completed());
    require(
        simulation.buildings().back().hit_points ==
        aoe::rules_for(aoe::BuildingKind::stable).hit_points
    );
    require(simulation.queue_unit(aoe::UnitKind::knight));

    const std::size_t original_units = simulation.units().size();
    for (int tick = 0;
         tick < aoe::rules_for(aoe::UnitKind::knight).training_ticks;
         ++tick) {
        simulation.update();
    }
    require(simulation.units().size() == original_units + 1);
    require(simulation.units().back().kind == aoe::UnitKind::knight);
}

void save_round_trip_preserves_active_construction() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {aoe::rules_for(aoe::BuildingKind::barracks).wood_cost, 200, 200, 200},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.select_unit_at({2, 7}, aoe::Player::blue));
    require(simulation.construct_building(
        aoe::BuildingKind::barracks,
        {2, 6}
    ));
    simulation.update();
    simulation.update();

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-construction-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);

    require(
        loaded.buildings().back().construction_ticks_remaining ==
        simulation.buildings().back().construction_ticks_remaining
    );
    require(
        loaded.buildings().back().builder_id ==
        simulation.buildings().back().builder_id
    );
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(loaded.buildings().back().completed());
    require(
        loaded.buildings().back().hit_points ==
        simulation.buildings().back().hit_points
    );
}

void construction_pauses_and_resumes_with_builder() {
    aoe::Simulation simulation(aoe::GameMap(6, 6));
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {5, 5}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {aoe::rules_for(aoe::BuildingKind::barracks).wood_cost, 200, 200, 200},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.construct_building_at(
        builder,
        aoe::BuildingKind::barracks,
        {2, 1}
    ));
    simulation.update();
    simulation.update();

    require(simulation.command_unit(builder, {1, 4}));
    require(simulation.buildings().back().builder_ids.empty());
    simulation.update();
    const int paused_progress =
        simulation.buildings().back().construction_ticks_remaining;
    simulation.update();
    simulation.update();
    require(
        simulation.buildings().back().construction_ticks_remaining ==
        paused_progress
    );

    require(simulation.command_unit(builder, {1, 1}));
    for (int tick = 0; tick < 8; ++tick) {
        simulation.update();
    }
    require(
        simulation.buildings().back().construction_ticks_remaining ==
        paused_progress
    );
    require(simulation.buildings().back().builder_ids.empty());

    const auto path =
        std::filesystem::temp_directory_path() /
        "aoe-detached-builder-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation detached = aoe::load_game(path);
    std::filesystem::remove(path);
    require(detached.buildings().back().builder_ids.empty());
    detached.update();
    require(
        detached.buildings().back().construction_ticks_remaining ==
        paused_progress
    );

    require(simulation.command_unit(builder, {2, 1}));
    require(simulation.buildings().back().builder_ids.size() == 1);
    for (int tick = 0; tick < 20; ++tick) {
        simulation.update();
    }
    require(simulation.buildings().back().completed());
}

void multiple_builders_use_original_diminishing_returns_and_persist() {
    aoe::Simulation simulation(aoe::GameMap(10, 8));
    const aoe::EntityId first = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId second = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 2}
    );
    const aoe::EntityId third = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 7}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {aoe::rules_for(aoe::BuildingKind::barracks).wood_cost, 200, 200, 200},
        simulation.economy(aoe::Player::red),
        0
    );
    require(simulation.construct_building_at(
        first,
        aoe::BuildingKind::barracks,
        {2, 2}
    ));
    require(simulation.command_unit(second, {2, 2}));
    require(simulation.command_unit(third, {2, 2}));
    require(simulation.buildings().back().builder_ids.size() == 3);

    const int original_remaining =
        simulation.buildings().back().construction_ticks_remaining;
    simulation.update();
    require(
        simulation.buildings().back().construction_ticks_remaining ==
        original_remaining - 1
    );
    require(
        simulation.buildings().back().construction_work_remainder == 2
    );

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-builders-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.buildings().back().builder_ids.size() == 3);
    require(
        loaded.buildings().back().construction_work_remainder == 2
    );

    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        loaded.update();
    }
    require(
        simulation.buildings().back().construction_ticks_remaining ==
        original_remaining - 10
    );
    require(
        loaded.buildings().back().construction_ticks_remaining ==
        simulation.buildings().back().construction_ticks_remaining
    );

    require(simulation.stop_unit(third));
    require(simulation.buildings().back().builder_ids.size() == 2);
    const int before_two_builders =
        simulation.buildings().back().construction_ticks_remaining;
    simulation.update();
    simulation.update();
    simulation.update();
    require(
        simulation.buildings().back().construction_ticks_remaining <
        before_two_builders
    );
}

void computer_player_moves_and_attacks() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    aoe::ComputerPlayer computer(aoe::Player::red);
    require(simulation.select_building_at({0, 10}, aoe::Player::blue));
    const auto player_selection = simulation.selected_building();

    int original_blue_health = 0;
    aoe::TilePosition original_red_scout{};
    for (const aoe::Unit& unit : simulation.units()) {
        if (unit.owner == aoe::Player::blue) {
            original_blue_health += unit.hit_points;
        } else if (unit.kind == aoe::UnitKind::scout_cavalry) {
            original_red_scout = unit.position;
        }
    }

    for (int tick = 0; tick < 80; ++tick) {
        simulation.update();
        computer.update(simulation);
    }

    int remaining_blue_health = 0;
    bool red_scout_moved = false;
    for (const aoe::Unit& unit : simulation.units()) {
        if (unit.owner == aoe::Player::blue) {
            remaining_blue_health += unit.hit_points;
        } else if (
            unit.kind == aoe::UnitKind::scout_cavalry &&
            unit.position != original_red_scout
        ) {
            red_scout_moved = true;
        }
    }
    require(red_scout_moved);
    require(remaining_blue_health <= original_blue_health);
    require(simulation.selected_building() == player_selection);
}

void computer_player_preserves_equal_distance_target_across_save() {
    aoe::Simulation simulation(aoe::GameMap(12, 10));
    for (aoe::TilePosition position : {
             aoe::TilePosition{5, 5},
             aoe::TilePosition{5, 6},
             aoe::TilePosition{5, 7},
         }) {
        simulation.add_unit(
            aoe::UnitKind::knight, aoe::Player::red, position
        );
    }
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 5}
    );
    const aoe::EntityId previous_target = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 5}
    );
    std::vector<aoe::Unit> units = simulation.units();
    for (aoe::Unit& unit : units) {
        unit.stance = aoe::UnitStance::passive;
    }
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        0
    );

    aoe::ComputerPlayer computer(
        aoe::Player::red, aoe::ComputerDifficulty::expert
    );
    aoe::ComputerPlayerState state = computer.state();
    state.last_target_id = previous_target;
    computer.restore_state(state);
    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
    }

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-ai-target.save";
    const auto ai_path =
        std::filesystem::temp_directory_path() / "aoe-ai-target.state";
    aoe::save_game(simulation, save_path);
    aoe::save_computer_player(computer, ai_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    aoe::ComputerPlayer loaded_computer =
        aoe::load_computer_player(ai_path);
    std::filesystem::remove(save_path);
    std::filesystem::remove(ai_path);

    computer.update(simulation);
    loaded_computer.update(loaded);
    require(loaded_computer.status().target == computer.status().target);
    require(
        loaded_computer.state().last_target_id ==
        computer.state().last_target_id
    );
    require(loaded_computer.state().last_target_id == previous_target);
}

void computer_player_scouts_without_omniscient_targeting() {
    aoe::Simulation simulation(aoe::GameMap(30, 5));
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId red = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {28, 2}
    );
    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    computer.update(simulation);

    const auto found = std::ranges::find_if(
        simulation.units(),
        [red](const aoe::Unit& unit) { return unit.id == red; }
    );
    require(found != simulation.units().end());
    require(found->moving);
    require(found->destination != aoe::TilePosition(1, 2));
}

void computer_player_gathers_and_trains_without_cheats() {
    aoe::GameMap map(20, 10);
    map.set_terrain({15, 3}, aoe::Terrain::forest);
    map.set_resource_amount({15, 3}, 100);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {10, 0}
    );
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {10, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::red,
        {16, 6}
    );
    simulation.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::red,
        {17, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {14, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 8}
    );
    std::vector<aoe::Unit> units = simulation.units();
    for (aoe::Unit& unit : units) {
        unit.stance = aoe::UnitStance::passive;
    }
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        {500, 500, 500, 500},
        {500, 500, 500, 500},
        0
    );

    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    computer.update(simulation);

    const auto red_villager = std::ranges::find_if(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.owner == aoe::Player::red &&
                unit.kind == aoe::UnitKind::villager;
        }
    );
    require(red_villager != simulation.units().end());
    require(red_villager->has_resource_target);
    require(red_villager->resource_target == aoe::TilePosition(15, 3));

    const auto tc = std::ranges::find_if(
        simulation.buildings(),
        [town_center](const aoe::Building& building) {
            return building.id == town_center;
        }
    );
    const auto military = std::ranges::find_if(
        simulation.buildings(),
        [barracks](const aoe::Building& building) {
            return building.id == barracks;
        }
    );
    require(tc != simulation.buildings().end());
    require(military != simulation.buildings().end());
    require(tc->production_queue.size() == 1);
    require(
        tc->production_queue.front().kind == aoe::UnitKind::villager
    );
    require(military->production_queue.size() == 1);
    require(
        military->production_queue.front().kind ==
        aoe::UnitKind::militia
    );
    require(
        simulation.economy(aoe::Player::red).food ==
        500 - aoe::rules_for(aoe::UnitKind::villager).food_cost -
            aoe::rules_for(aoe::UnitKind::militia).food_cost
    );
}

void computer_player_repeats_housing_before_population_block() {
    aoe::Simulation simulation(aoe::GameMap(30, 15));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {10, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {10, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 10}
    );
    for (int index = 0; index < 8; ++index) {
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {16 + index % 4, 2 + index / 4 * 4}
        );
    }
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 12}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 500},
        {500, 500, 500, 500},
        0
    );

    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    computer.update(simulation);

    int houses = 0;
    int foundations = 0;
    for (const aoe::Building& building : simulation.buildings()) {
        if (building.owner == aoe::Player::red &&
            building.kind == aoe::BuildingKind::house) {
            ++houses;
            foundations += building.completed() ? 0 : 1;
        }
    }
    require(houses == 2);
    require(foundations == 1);
    require(
        simulation.economy(aoe::Player::red).wood ==
        500 - aoe::rules_for(aoe::BuildingKind::house).wood_cost
    );
}

void computer_player_advances_and_builds_age_prerequisites_normally() {
    aoe::Simulation simulation(aoe::GameMap(40, 20));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {27, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {27, 6}
    );
    simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::red,
        {33, 6}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    for (const aoe::TilePosition position : {
             aoe::TilePosition{24, 5},
             aoe::TilePosition{24, 10},
             aoe::TilePosition{32, 12},
             aoe::TilePosition{37, 10},
         }) {
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            position
        );
    }
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 4}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {3000, 3000, 3000, 3000},
        {3000, 3000, 3000, 3000},
        0
    );

    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0;
         tick < 250 && simulation.age(aoe::Player::red) < aoe::Age::castle;
         ++tick) {
        simulation.update();
        computer.update(simulation);
    }

    require(simulation.age(aoe::Player::red) == aoe::Age::castle);
    const auto completed_red = [&](aoe::BuildingKind kind) {
        return std::ranges::any_of(
            simulation.buildings(),
            [kind](const aoe::Building& building) {
                return building.owner == aoe::Player::red &&
                    building.kind == kind && building.completed();
            }
        );
    };
    require(completed_red(aoe::BuildingKind::blacksmith));
    require(completed_red(aoe::BuildingKind::archery_range) ||
        completed_red(aoe::BuildingKind::stable));
    require(
        simulation.economy(aoe::Player::red).food <=
        3000 - aoe::rules_for(aoe::Age::feudal).food_cost -
            aoe::rules_for(aoe::Age::castle).food_cost
    );
    require(
        simulation.economy(aoe::Player::red).gold <=
        3000 - aoe::rules_for(aoe::Age::castle).gold_cost
    );
}

void computer_player_builds_harvests_and_reseeds_farms() {
    aoe::Simulation simulation(aoe::GameMap(24, 12));
    simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {12, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::red,
        {12, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::red,
        {19, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 5}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {16, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {5, 5}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {500, 500, 500, 500},
        {500, 100, 500, 500},
        0
    );

    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    computer.update(simulation);

    const auto farm_foundation = std::ranges::find_if(
        simulation.buildings(),
        [](const aoe::Building& building) {
            return building.owner == aoe::Player::red &&
                building.kind == aoe::BuildingKind::farm;
        }
    );
    require(farm_foundation != simulation.buildings().end());
    require(!farm_foundation->completed());
    const aoe::EntityId farm = farm_foundation->id;

    for (int tick = 0; tick < 15; ++tick) {
        simulation.update();
        computer.update(simulation);
    }
    const auto worker = std::ranges::find_if(
        simulation.units(),
        [villager](const aoe::Unit& unit) {
            return unit.id == villager;
        }
    );
    require(worker != simulation.units().end());
    require(worker->resource_building_id == farm);
    require(worker->carried_resource == aoe::ResourceKind::food);

    std::vector<aoe::Building> exhausted = simulation.buildings();
    const auto farm_to_exhaust = std::ranges::find_if(
        exhausted,
        [farm](const aoe::Building& building) {
            return building.id == farm;
        }
    );
    require(farm_to_exhaust != exhausted.end());
    farm_to_exhaust->resource_amount = 1;
    simulation.replace_state(
        simulation.units(),
        std::move(exhausted),
        simulation.economy(aoe::Player::blue),
        {
            simulation.economy(aoe::Player::red).wood + 60,
            simulation.economy(aoe::Player::red).food,
            simulation.economy(aoe::Player::red).gold,
            simulation.economy(aoe::Player::red).stone,
        },
        simulation.tick_number()
    );
    const int wood_before_reseed =
        simulation.economy(aoe::Player::red).wood;
    const auto mill = std::ranges::find_if(
        simulation.buildings(),
        [](const aoe::Building& building) {
            return building.owner == aoe::Player::red &&
                building.kind == aoe::BuildingKind::mill;
        }
    );
    require(mill != simulation.buildings().end());
    require(simulation.reseed_farm(mill->id));
    require(simulation.farm_reseed_queue(aoe::Player::red) == 1);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
        computer.update(simulation);
    }

    const auto reseeded = std::ranges::find_if(
        simulation.buildings(),
        [farm](const aoe::Building& building) {
            return building.id == farm;
        }
    );
    require(reseeded != simulation.buildings().end());
    require(reseeded->resource_amount > 0);
    require(
        simulation.economy(aoe::Player::red).wood ==
        wood_before_reseed -
            aoe::rules_for(aoe::BuildingKind::farm).wood_cost
    );
}

void computer_player_targets_and_destroys_final_enemy_building() {
    aoe::Simulation simulation(aoe::GameMap(14, 7));
    const aoe::EntityId target = simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {10, 4}
    );
    const aoe::EntityId ram = simulation.add_unit(
        aoe::UnitKind::battering_ram,
        aoe::Player::red,
        {4, 1}
    );
    for (int index = 0; index < 9; ++index) {
        simulation.add_unit(
            aoe::UnitKind::battering_ram,
            aoe::Player::red,
            {4 + index % 3, 2 + index / 3}
        );
    }
    std::vector<aoe::Unit> units = simulation.units();
    units.front().stance = aoe::UnitStance::passive;
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        0
    );
    simulation.replace_ages(aoe::Age::dark, aoe::Age::feudal);

    aoe::ComputerPlayer computer(
        aoe::Player::red, aoe::ComputerDifficulty::hardest
    );
    for (int tick = 0; tick < 6; ++tick) {
        simulation.update();
        computer.update(simulation);
    }

    const auto attacker = std::ranges::find_if(
        simulation.units(),
        [ram](const aoe::Unit& unit) {
            return unit.id == ram;
        }
    );
    require(attacker != simulation.units().end());
    require(attacker->attack_moving || attacker->attack_target_id == target);

    const auto& ram_rules = aoe::rules_for(aoe::UnitKind::battering_ram);
    const auto& target_rules = aoe::rules_for(aoe::BuildingKind::house);
    const int ram_damage =
        ram_rules.attack + ram_rules.bonus_vs_buildings -
        target_rules.melee_armor;
    const int maximum_attack_ticks =
        ((target_rules.hit_points + ram_damage - 1) / ram_damage) *
            ram_rules.attack_interval_ticks +
        30;
    for (int tick = 0;
         tick < maximum_attack_ticks &&
         simulation.outcome() == aoe::MatchOutcome::ongoing;
         ++tick) {
        simulation.update();
        computer.update(simulation);
    }
    require(simulation.outcome() == aoe::MatchOutcome::red_victory);
    require(simulation.buildings().size() == 1);
    require(simulation.buildings().front().owner == aoe::Player::red);
}

void large_buildings_reveal_and_target_from_any_footprint_tile() {
    aoe::Simulation simulation(aoe::GameMap(20, 12));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {6, 1}
    );
    const aoe::EntityId militia = simulation.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::blue,
        {12, 4}
    );
    std::vector<aoe::Unit> units = simulation.units();
    units.front().stance = aoe::UnitStance::passive;
    simulation.replace_state(
        std::move(units),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        0
    );

    require(!simulation.is_visible(aoe::Player::blue, {6, 1}));
    require(simulation.is_visible(aoe::Player::blue, {9, 4}));
    require(simulation.is_building_visible(
        aoe::Player::blue,
        simulation.buildings().front()
    ));

    aoe::ComputerPlayer computer(aoe::Player::blue);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    computer.update(simulation);

    const auto attacker = std::ranges::find_if(
        simulation.units(),
        [militia](const aoe::Unit& unit) {
            return unit.id == militia;
        }
    );
    require(attacker != simulation.units().end());
    require(attacker->attack_target_id == town_center);
    require(attacker->attack_target_is_building);
}

void match_ends_after_last_enemy_is_destroyed() {
    aoe::Simulation simulation(aoe::GameMap(5, 5));
    const aoe::EntityId blue =
        simulation.add_unit(
            aoe::UnitKind::knight,
            aoe::Player::blue,
            {1, 1}
        );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {2, 1}
    );

    require(simulation.command_unit(blue, {2, 1}));
    for (int tick = 0;
         tick < 100 &&
         simulation.outcome() == aoe::MatchOutcome::ongoing;
         ++tick) {
        simulation.update();
    }

    require(simulation.outcome() == aoe::MatchOutcome::blue_victory);
    const auto final_tick = simulation.tick_number();
    require(!simulation.command_unit(blue, {3, 1}));
    simulation.update();
    require(simulation.tick_number() == final_tick);
}

void resignation_immediately_awards_the_opponent_victory() {
    for (const auto [resigning_player, expected_outcome] : {
             std::pair{
                 aoe::Player::blue,
                 aoe::MatchOutcome::red_victory,
             },
             std::pair{
                 aoe::Player::red,
                 aoe::MatchOutcome::blue_victory,
             },
         }) {
        aoe::Simulation simulation(aoe::GameMap(5, 5));
        const aoe::EntityId blue = simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {1, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {3, 3}
        );

        require(simulation.select_unit_at({1, 1}, aoe::Player::blue));
        require(simulation.resign(resigning_player));
        require(simulation.outcome() == expected_outcome);
        require(!simulation.selected_unit().has_value());
        require(simulation.selected_units().empty());

        const auto final_tick = simulation.tick_number();
        require(!simulation.command_unit(blue, {2, 1}));
        require(!simulation.resign(resigning_player));
        simulation.update();
        require(simulation.tick_number() == final_tick);
    }
}

void units_can_destroy_enemy_buildings() {
    aoe::Simulation simulation(aoe::GameMap(5, 5));
    const aoe::EntityId blue =
        simulation.add_unit(
            aoe::UnitKind::knight,
            aoe::Player::blue,
            {1, 1}
        );
    simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {2, 1}
    );

    for (
        int attack = 0;
        attack < 2000 &&
        simulation.outcome() == aoe::MatchOutcome::ongoing;
        ++attack
    ) {
        require(simulation.command_unit(blue, {2, 1}));
        simulation.update();
    }

    require(simulation.buildings().empty());
    require(simulation.outcome() == aoe::MatchOutcome::blue_victory);
}

void group_attackers_repath_and_focus_target() {
    aoe::Simulation simulation(aoe::GameMap(6, 6));
    const aoe::EntityId first = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId second = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {3, 2}
    );

    require(simulation.command_unit(first, {3, 2}));
    require(simulation.command_unit(second, {3, 2}));
    const auto& knight_rules = aoe::rules_for(aoe::UnitKind::knight);
    const auto& target_rules = aoe::rules_for(aoe::BuildingKind::barracks);
    const int two_knight_damage =
        2 * (knight_rules.attack - target_rules.melee_armor);
    const int maximum_attack_ticks =
        ((target_rules.hit_points + two_knight_damage - 1) /
         two_knight_damage) *
            knight_rules.attack_interval_ticks +
        30;
    for (int tick = 0;
         tick < maximum_attack_ticks &&
         simulation.outcome() == aoe::MatchOutcome::ongoing;
         ++tick) {
        simulation.update();
    }

    require(simulation.buildings().empty());
    require(simulation.outcome() == aoe::MatchOutcome::blue_victory);
}

void attack_rate_obeys_unit_cooldown() {
    aoe::Simulation simulation(aoe::GameMap(5, 5));
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {2, 1}
    );
    require(simulation.command_unit(knight, {2, 1}));

    simulation.update();
    const int after_first_hit = simulation.units().back().hit_points;
    require(after_first_hit == 15);
    const auto path =
        std::filesystem::temp_directory_path() / "aoe-cooldown-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(
        loaded.units().front().attack_cooldown ==
        simulation.units().front().attack_cooldown
    );
    simulation.update();
    simulation.update();
    simulation.update();
    loaded.update();
    loaded.update();
    loaded.update();
    require(simulation.units().back().hit_points == after_first_hit);
    require(loaded.units().back().hit_points == after_first_hit);
    simulation.update();
    loaded.update();
    require(simulation.units().back().hit_points == 5);
    require(loaded.units().back().hit_points == 5);
}

void ranged_release_waits_for_dat_frame_and_binds_target() {
    aoe::Simulation simulation(aoe::GameMap(7, 7));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId first_target = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {4, 2}
    );
    const aoe::EntityId second_target = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {2, 4}
    );
    require(simulation.set_unit_stance(
        first_target, aoe::UnitStance::passive
    ));
    require(simulation.set_unit_stance(
        second_target, aoe::UnitStance::passive
    ));
    require(simulation.command_unit(archer, {4, 2}));

    simulation.update();
    require(simulation.projectiles().empty());
    require(simulation.units().front().attack_release_ticks_remaining == 2);
    require(simulation.units().front().attack_animation_started);
    require(simulation.units().front().animation_state == 3);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-attack-release-frame-test.save";
    const std::string hash = aoe::deterministic_state_hash(simulation);
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(aoe::deterministic_state_hash(loaded) == hash);
    require(
        loaded.units().front().attack_release_action_key ==
        simulation.units().front().attack_release_action_key
    );
    require(
        loaded.units().front().attack_animation_start_tick ==
        simulation.units().front().attack_animation_start_tick
    );
    require(
        loaded.units().front().animation_state_start_tick ==
        simulation.units().front().animation_state_start_tick
    );

    require(simulation.command_unit(archer, {2, 4}));
    simulation.update();
    require(simulation.projectiles().empty());
    require(simulation.units().front().attack_release_ticks_remaining == 2);
    simulation.update();
    require(simulation.projectiles().empty());
    simulation.update();
    require(simulation.projectiles().size() == 1);
    require(simulation.projectiles().front().target == second_target);

    loaded.update();
    require(loaded.projectiles().empty());
    loaded.update();
    require(loaded.projectiles().size() == 1);
}

std::string state_fingerprint(const aoe::Simulation& simulation);

void military_auto_acquires_only_visible_targets() {
    aoe::Simulation visible_match(aoe::GameMap(12, 5));
    visible_match.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId visible_enemy = visible_match.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::red,
        {5, 1}
    );
    visible_match.update();
    require(
        visible_match.units().front().attack_target_id ==
        visible_enemy
    );
    require(visible_match.units().front().moving);

    aoe::Simulation hidden_match(aoe::GameMap(12, 5));
    hidden_match.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {1, 1}
    );
    hidden_match.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::red,
        {8, 1}
    );
    hidden_match.update();
    require(hidden_match.units().front().attack_target_id == 0);
    require(!hidden_match.units().front().moving);
}

void moving_target_pursuit_survives_save_and_replay() {
    aoe::Simulation simulation(aoe::GameMap(14, 5));
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId target = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {4, 1}
    );
    require(simulation.command_unit(attacker, {4, 1}));
    require(simulation.command_unit(target, {10, 1}));
    simulation.update();
    require(simulation.units().front().attack_target_id == target);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-pursuit-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().attack_target_id == target);
    require(!loaded.units().front().attack_target_is_building);
    for (int tick = 0; tick < 14; ++tick) {
        loaded.update();
    }
    require(loaded.units().back().hit_points < 25);

    aoe::Simulation first(aoe::GameMap(14, 5));
    const aoe::EntityId replay_attacker = first.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1}
    );
    const aoe::EntityId replay_target = first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {4, 1}
    );
    aoe::Simulation second = first;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::MoveUnitCommand{replay_attacker, {4, 1}}
    );
    recorded.record(
        0,
        aoe::MoveUnitCommand{replay_target, {10, 1}}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-pursuit-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    for (int tick = 0; tick < 15; ++tick) {
        recorded.apply_current_tick(first);
        replayed.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().back().hit_points < 25);
    require(
        first.units().front().attack_target_id ==
        second.units().front().attack_target_id
    );
    require(state_fingerprint(first) == state_fingerprint(second));
}

std::string state_fingerprint(const aoe::Simulation& simulation) {
    std::ostringstream output;
    output << simulation.tick_number() << ':'
           << simulation.economy(aoe::Player::blue).wood << ':'
           << simulation.economy(aoe::Player::blue).food;
    for (const aoe::Unit& unit : simulation.units()) {
        output << "|u" << unit.id << ',' << unit.position.x << ','
               << unit.position.y << ',' << unit.hit_points << ','
               << unit.attack_target_id << ','
               << unit.attack_target_is_building << ','
               << unit.attack_moving << ','
               << unit.attack_move_destination.x << ','
               << unit.attack_move_destination.y << ','
               << unit.patrolling << ','
               << unit.patrol_origin.x << ','
               << unit.patrol_origin.y << ','
               << unit.patrol_destination.x << ','
               << unit.patrol_destination.y << ','
               << unit.guard_target_id << ','
               << unit.guard_target_is_building << ','
               << unit.waypoints.size() << ','
               << static_cast<int>(unit.stance) << ','
               << unit.stance_anchor.x << ','
               << unit.stance_anchor.y << ','
               << unit.attack_target_auto;
    }
    for (const aoe::Building& building : simulation.buildings()) {
        output << "|b" << building.id << ','
               << static_cast<int>(building.kind) << ','
               << building.position.x << ',' << building.position.y << ','
               << building.production_queue.size() << ','
               << building.has_rally_point << ','
               << building.rally_point.x << ',' << building.rally_point.y;
    }
    return output.str();
}

void replay_round_trip_reproduces_state() {
    aoe::Replay recorded;
    recorded.record(0, aoe::MoveUnitCommand{1, {5, 7}});
    recorded.record(2, aoe::QueueUnitCommand{5, aoe::UnitKind::villager});
    recorded.record(
        3,
        aoe::ConstructBuildingCommand{
            1,
            aoe::BuildingKind::barracks,
            {4, 6},
        }
    );

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-replay-test.txt";
    aoe::save_replay(recorded, path);
    aoe::Replay loaded = aoe::load_replay(path);
    std::filesystem::remove(path);

    aoe::Simulation first = aoe::Simulation::create_demo();
    aoe::Simulation second = aoe::Simulation::create_demo();
    for (aoe::Simulation* simulation : {&first, &second}) {
        simulation->replace_state(
            simulation->units(),
            simulation->buildings(),
            {aoe::rules_for(aoe::BuildingKind::barracks).wood_cost, 200, 200, 200},
            simulation->economy(aoe::Player::red),
            0
        );
    }
    for (int tick = 0; tick < 20; ++tick) {
        recorded.apply_current_tick(first);
        loaded.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(state_fingerprint(first) == state_fingerprint(second));
    require(first.buildings().size() == 3);
    require(first.units().size() == 5);
}

void stable_owns_cavalry_production_and_persists_it() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId stable = simulation.add_building(
        aoe::BuildingKind::stable,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    require(!simulation.queue_unit_at(
        stable,
        aoe::UnitKind::scout_cavalry
    ));
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(!simulation.queue_unit_at(
        barracks,
        aoe::UnitKind::scout_cavalry
    ));
    require(!simulation.queue_unit_at(stable, aoe::UnitKind::knight));
    require(simulation.queue_unit_at(
        stable,
        aoe::UnitKind::scout_cavalry
    ));
    require(simulation.economy(aoe::Player::blue).food == 120);
    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
    }

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-stable-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const auto loaded_stable = std::ranges::find_if(
        loaded.buildings(),
        [](const aoe::Building& building) {
            return building.kind == aoe::BuildingKind::stable;
        }
    );
    require(loaded_stable != loaded.buildings().end());
    require(loaded_stable->production_queue.size() == 1);
    require(
        loaded_stable->production_queue.front().kind ==
        aoe::UnitKind::scout_cavalry
    );
    for (int tick = 0; tick < 11; ++tick) {
        loaded.update();
    }
    require(loaded.units().back().kind == aoe::UnitKind::scout_cavalry);
    require(loaded.units().back().hit_points == 45);
    require(
        loaded.effective_unit_vision_range(loaded.units().back()) == 6
    );

    loaded.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(loaded.queue_unit_at(stable, aoe::UnitKind::knight));

    aoe::Scenario scenario(8, 8);
    scenario.blue_age = aoe::Age::feudal;
    scenario.units.push_back({
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {2, 3},
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::stable,
        aoe::Player::blue,
        {2, 2},
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::blacksmith,
        aoe::Player::blue,
        {4, 2},
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-stable-test.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(
        scenario_copy.units.front().kind ==
        aoe::UnitKind::scout_cavalry
    );
    require(
        scenario_copy.buildings.front().kind ==
        aoe::BuildingKind::stable
    );
    require(
        scenario_copy.buildings.back().kind ==
        aoe::BuildingKind::blacksmith
    );
}

void scout_training_replay_is_deterministic() {
    aoe::Simulation first(aoe::GameMap(8, 8));
    first.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    first.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId stable = first.add_building(
        aoe::BuildingKind::stable,
        aoe::Player::blue,
        {4, 2}
    );
    first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    aoe::Simulation second = first;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::QueueUnitCommand{stable, aoe::UnitKind::scout_cavalry}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-scout-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    for (int tick = 0; tick < 14; ++tick) {
        recorded.apply_current_tick(first);
        replayed.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().back().kind == aoe::UnitKind::scout_cavalry);
    require(state_fingerprint(first) == state_fingerprint(second));
}

void barracks_trains_age_gated_infantry_and_persists_it() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {4, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    require(!simulation.queue_unit_at(
        barracks,
        aoe::UnitKind::spearman
    ));
    require(!simulation.queue_unit_at(
        barracks,
        aoe::UnitKind::scout_cavalry
    ));
    require(simulation.queue_unit_at(barracks, aoe::UnitKind::militia));
    require(simulation.economy(aoe::Player::blue).food == 140);
    require(simulation.economy(aoe::Player::blue).gold == 180);
    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
    }

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-infantry-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.buildings()[1].production_queue.front().kind ==
        aoe::UnitKind::militia
    );
    for (int tick = 0; tick < 9; ++tick) {
        loaded.update();
    }
    require(loaded.units().back().kind == aoe::UnitKind::militia);

    loaded.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(loaded.queue_unit_at(barracks, aoe::UnitKind::spearman));
    require(loaded.economy(aoe::Player::blue).food == 105);
    for (int tick = 0; tick < 12; ++tick) {
        loaded.update();
    }
    require(loaded.units().back().kind == aoe::UnitKind::spearman);

    aoe::Scenario scenario(8, 8);
    scenario.units.push_back({
        aoe::UnitKind::militia,
        aoe::Player::blue,
        {2, 3},
    });
    scenario.units.push_back({
        aoe::UnitKind::spearman,
        aoe::Player::red,
        {3, 3},
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-infantry-test.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[0].kind == aoe::UnitKind::militia);
    require(scenario_copy.units[1].kind == aoe::UnitKind::spearman);
}

void spearman_bonus_damage_counters_cavalry() {
    aoe::Simulation cavalry_match(aoe::GameMap(6, 6));
    const aoe::EntityId cavalry_spearman = cavalry_match.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {2, 2}
    );
    cavalry_match.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::red,
        {3, 2}
    );
    require(cavalry_match.command_unit(cavalry_spearman, {3, 2}));
    cavalry_match.update();
    require(cavalry_match.units().back().hit_points == 27);

    aoe::Simulation infantry_match(aoe::GameMap(6, 6));
    const aoe::EntityId infantry_spearman = infantry_match.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {2, 2}
    );
    infantry_match.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {3, 2}
    );
    require(infantry_match.command_unit(infantry_spearman, {3, 2}));
    infantry_match.update();
    require(infantry_match.units().back().hit_points == 37);
}

void spearman_training_replay_is_deterministic() {
    aoe::Simulation first(aoe::GameMap(8, 8));
    first.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    first.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId barracks = first.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {4, 2}
    );
    first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    aoe::Simulation second = first;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::QueueUnitCommand{barracks, aoe::UnitKind::spearman}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-spearman-test.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    for (int tick = 0; tick < 12; ++tick) {
        recorded.apply_current_tick(first);
        replayed.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().back().kind == aoe::UnitKind::spearman);
    require(second.units().back().kind == aoe::UnitKind::spearman);
    require(state_fingerprint(first) == state_fingerprint(second));
}

void loom_upgrades_villager_survivability_and_persists() {
    const aoe::TechnologyRules& loom =
        aoe::rules_for(aoe::Technology::loom);
    require(loom.researched_at == aoe::BuildingKind::town_center);
    require(loom.minimum_age == aoe::Age::dark);
    require(loom.gold_cost == 50);
    require(loom.wood_cost == 0);
    require(loom.food_cost == 0);

    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {5, 2}
    );
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 5}
    );
    require(simulation.set_unit_stance(
        attacker,
        aoe::UnitStance::passive
    ));
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {0, 0, 50, 0},
        simulation.economy(aoe::Player::red),
        0
    );

    require(simulation.research_technology_at(
        town_center,
        aoe::Technology::loom
    ));
    require(simulation.economy(aoe::Player::blue).gold == 0);
    for (int tick = 1; tick < loom.research_ticks; ++tick) {
        simulation.update();
    }
    require(!simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::loom
    ));
    require(simulation.units().front().hit_points == 25);
    simulation.update();
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::loom
    ));
    require(simulation.units().front().hit_points == 40);
    require(simulation.maximum_hit_points(simulation.units().front()) == 40);
    require(simulation.melee_armor(simulation.units().front()) == 1);
    require(simulation.pierce_armor(simulation.units().front()) == 2);

    require(simulation.command_unit(attacker, {5, 2}));
    simulation.update();
    require(simulation.units().front().hit_points == 37);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-loom-test.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::loom
    ));
    require(loaded.units().front().id == villager);
    require(loaded.units().front().hit_points == 37);
    require(loaded.melee_armor(loaded.units().front()) == 1);
    require(loaded.pierce_armor(loaded.units().front()) == 2);

    const aoe::EntityId future = loaded.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {5, 4}
    );
    const auto produced = std::ranges::find_if(
        loaded.units(),
        [future](const aoe::Unit& unit) { return unit.id == future; }
    );
    require(produced != loaded.units().end());
    require(produced->hit_points == 40);
}

void technologies_research_persist_and_apply_to_all_units() {
    aoe::GameMap map(16, 10);
    map.set_terrain({3, 6}, aoe::Terrain::forest);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId range = simulation.add_building(
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {5, 1}
    );
    const aoe::EntityId barracks = simulation.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::blue,
        {7, 1}
    );
    const aoe::EntityId blacksmith = simulation.add_building(
        aoe::BuildingKind::blacksmith,
        aoe::Player::blue,
        {9, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {12, 6}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 6}
    );
    simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {5, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::blue,
        {7, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {8, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {1000, 1000, 1000, 1000},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.research_technology_at(
        town_center,
        aoe::Technology::wheelbarrow
    ));
    require(simulation.economy(aoe::Player::blue).food == 1000);
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);

    require(simulation.research_technology_at(
        town_center,
        aoe::Technology::wheelbarrow
    ));
    require(!simulation.research_technology_at(
        range,
        aoe::Technology::fletching
    ));
    require(!simulation.research_technology_at(
        barracks,
        aoe::Technology::forging
    ));
    require(simulation.research_technology_at(
        blacksmith,
        aoe::Technology::fletching
    ));
    require(!simulation.research_technology_at(
        blacksmith,
        aoe::Technology::forging
    ));
    require(simulation.economy(aoe::Player::blue).wood == 950);
    require(simulation.economy(aoe::Player::blue).food == 725);
    require(simulation.economy(aoe::Player::blue).gold == 950);
    require(!simulation.research_technology_at(
        town_center,
        aoe::Technology::fletching
    ));
    require(simulation.economy(aoe::Player::blue).food == 725);

    for (int tick = 0; tick < 5; ++tick) {
        simulation.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-technology-test.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const auto loaded_blacksmith = std::ranges::find_if(
        loaded.buildings(),
        [blacksmith](const aoe::Building& building) {
            return building.id == blacksmith;
        }
    );
    require(loaded_blacksmith != loaded.buildings().end());
    require(
        loaded_blacksmith->technology_research_ticks_remaining ==
        10
    );
    for (int tick = 0; tick < 10; ++tick) {
        loaded.update();
    }
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::wheelbarrow
    ));
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::fletching
    ));
    require(!loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::forging
    ));
    require(loaded.research_technology_at(
        blacksmith,
        aoe::Technology::forging
    ));
    require(loaded.economy(aoe::Player::blue).food == 575);
    for (int tick = 0; tick < 15; ++tick) {
        loaded.update();
    }
    require(loaded.has_technology(
        aoe::Player::blue,
        aoe::Technology::forging
    ));
    require(loaded.units()[0].attack == 3);
    require(loaded.units()[1].attack == 5);
    require(loaded.units()[2].attack == 11);
    require(loaded.units()[3].attack == 6);
    const aoe::EntityId future_archer = loaded.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {9, 3}
    );
    require(loaded.units().back().id == future_archer);
    require(loaded.units().back().attack == 5);

    const int wood_before = loaded.economy(aoe::Player::blue).wood;
    require(loaded.command_unit(villager, {3, 6}));
    for (int tick = 0;
         tick < 60 &&
         loaded.economy(aoe::Player::blue).wood == wood_before;
         ++tick) {
        loaded.update();
    }
    require(loaded.economy(aoe::Player::blue).wood == wood_before + 12);
}

void technology_replay_command_reproduces_research() {
    aoe::Simulation first(aoe::GameMap(8, 8));
    first.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    const aoe::EntityId blacksmith = first.add_building(
        aoe::BuildingKind::blacksmith,
        aoe::Player::blue,
        {2, 2}
    );
    first.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 7}
    );
    first.replace_state(
        first.units(),
        first.buildings(),
        {500, 500, 500, 500},
        first.economy(aoe::Player::red),
        0
    );
    aoe::Simulation second = first;
    aoe::Replay recorded;
    recorded.record(
        0,
        aoe::ResearchTechnologyCommand{
            blacksmith,
            aoe::Technology::forging,
        }
    );
    const auto path =
        std::filesystem::temp_directory_path() / "aoe-tech-test.replay";
    aoe::save_replay(recorded, path);
    aoe::Replay loaded = aoe::load_replay(path);
    std::filesystem::remove(path);
    for (int tick = 0; tick < 15; ++tick) {
        recorded.apply_current_tick(first);
        loaded.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.has_technology(
        aoe::Player::blue,
        aoe::Technology::forging
    ));
    require(second.has_technology(
        aoe::Player::blue,
        aoe::Technology::forging
    ));
    require(
        first.economy(aoe::Player::blue).food ==
        second.economy(aoe::Player::blue).food
    );
}

void scenario_resource_round_trip() {
    const aoe::Scenario scenario = aoe::generate_random_map({
        aoe::RandomMapKind::rivers,
        aoe::RandomMapSize::giant,
        9173,
    });
    require(scenario.map.width() == 240);
    require(scenario.map.height() == 240);
    require(!scenario.units.empty());
    require(!scenario.buildings.empty());
    require(aoe::validate_random_map(
        scenario, aoe::RandomMapKind::rivers
    ).valid);
    aoe::Scenario extended = scenario;
    extended.strict_trigger_syntax = false;
    extended.blue_age = aoe::Age::castle;
    extended.red_age = aoe::Age::feudal;
    extended.blue_technologies = {
        aoe::Technology::wheelbarrow,
        aoe::Technology::fletching,
    };
    extended.red_technologies = {aoe::Technology::forging};
    extended.objectives = {
        {
            2, aoe::Player::red, false, true,
            "Protect the eastern ford.",
        },
        {
            1, aoe::Player::blue, true, false,
            "Destroy the enemy's \"watch post\".",
        },
    };
    extended.triggers = {
        {
            9, 10, true, false,
            "elapsed_ticks >= 120",
            "show_message objective_1",
        },
        {
            3, 50, false, true,
            "blue_units_in_area 4 4 8 8",
            "set_objective_visible 2",
        },
    };
    extended.map.set_terrain({8, 2}, aoe::Terrain::grass);
    extended.units.push_back({
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {8, 2},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::archery_range,
        aoe::Player::blue,
        {8, 6},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {9, 3},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::mill,
        aoe::Player::blue,
        {10, 3},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::lumber_camp,
        aoe::Player::blue,
        {10, 4},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::mining_camp,
        aoe::Player::blue,
        {10, 5},
    });
    extended.buildings.push_back({
        aoe::BuildingKind::farm,
        aoe::Player::blue,
        {9, 5},
    });
    extended.buildings.back().resource_amount = 80;
    extended.buildings[extended.buildings.size() - 6].hit_points = 120;
    const auto copy =
        std::filesystem::temp_directory_path() / "aoe-scenario-test.txt";
    aoe::save_scenario(extended, copy);
    const aoe::Scenario loaded = aoe::load_scenario(copy);
    std::filesystem::remove(copy);

    require(loaded.units.back().kind == aoe::UnitKind::archer);
    require(loaded.blue_age == aoe::Age::castle);
    require(loaded.red_age == aoe::Age::feudal);
    require(loaded.blue_technologies == extended.blue_technologies);
    require(loaded.red_technologies == extended.red_technologies);
    require(loaded.objectives.size() == 2);
    require(loaded.objectives[0].id == 1);
    require(
        loaded.objectives[0].description ==
        "Destroy the enemy's \"watch post\"."
    );
    require(loaded.objectives[1].id == 2);
    require(loaded.objectives[1].hidden);
    require(!loaded.objectives[1].required);
    require(loaded.triggers.size() == 2);
    require(loaded.triggers[0].id == 3);
    require(loaded.triggers[0].priority == 50);
    require(!loaded.triggers[0].enabled);
    require(loaded.triggers[0].looping);
    require(loaded.triggers[1].id == 9);
    require(
        loaded.triggers[1].conditions ==
        std::vector<std::string>{"elapsed_ticks >= 120"}
    );
    require(
        loaded.buildings[loaded.buildings.size() - 6].kind ==
        aoe::BuildingKind::archery_range
    );
    require(
        loaded.buildings[loaded.buildings.size() - 6].hit_points ==
        120
    );
    require(
        loaded.buildings[loaded.buildings.size() - 5].kind ==
        aoe::BuildingKind::house
    );
    require(
        loaded.buildings[loaded.buildings.size() - 4].kind ==
        aoe::BuildingKind::mill
    );
    require(
        loaded.buildings[loaded.buildings.size() - 3].kind ==
        aoe::BuildingKind::lumber_camp
    );
    require(
        loaded.buildings[loaded.buildings.size() - 2].kind ==
        aoe::BuildingKind::mining_camp
    );
    require(loaded.buildings.back().kind == aoe::BuildingKind::farm);
    require(loaded.buildings.back().resource_amount == 80);
    aoe::Simulation first = aoe::create_simulation(extended);
    aoe::Simulation second = aoe::create_simulation(loaded);
    require(second.buildings().back().resource_amount == 80);
    require(second.has_technology(
        aoe::Player::blue,
        aoe::Technology::fletching
    ));
    require(second.units().back().attack == 5);
    require(
        second.buildings()[second.buildings().size() - 6].hit_points ==
        120
    );
    require(state_fingerprint(first) == state_fingerprint(second));

    const auto corrupt =
        std::filesystem::temp_directory_path() /
        "aoe-corrupt-trigger.scenario";
    {
        std::ofstream output(corrupt);
        output << "AOE-ARCHAEOLOGY-SCENARIO 61\n"
               << "map 8 8\n"
               << "objective 1 blue 1 0 \"First\"\n"
               << "objective 1 red 0 0 \"Duplicate\"\n";
    }
    bool corrupt_rejected = false;
    try {
        (void)aoe::load_scenario(corrupt);
    } catch (const std::runtime_error&) {
        corrupt_rejected = true;
    }
    std::filesystem::remove(corrupt);
    require(corrupt_rejected);
}

void generated_startup_map_uses_original_default_and_ticks() {
    // Default settings use original Normal preset and must still simulate.
    const aoe::Scenario scenario = aoe::generate_random_map({});
    require(scenario.map.width() == 200);
    require(scenario.map.height() == 200);
    require(
        static_cast<long long>(scenario.map.width()) *
            scenario.map.height() ==
        40000
    );
    for (const aoe::UnitPlacement& unit : scenario.units) {
        require(scenario.map.contains(unit.position));
    }
    for (const aoe::BuildingPlacement& building : scenario.buildings) {
        const aoe::BuildingRules& rules = aoe::rules_for(building.kind);
        require(scenario.map.contains({
            building.position.x + rules.footprint_width - 1,
            building.position.y + rules.footprint_height - 1,
        }));
    }
    aoe::Simulation simulation = aoe::create_simulation(scenario);
    require(simulation.map().width() == 200);
    require(simulation.map().height() == 200);
    const std::uint64_t before = simulation.tick_number();
    for (int tick = 0; tick < 8; ++tick) {
        simulation.update();
    }
    require(simulation.tick_number() == before + 8);
    require(simulation.units().size() == scenario.units.size());
    require(simulation.buildings().size() == scenario.buildings.size());
    const auto blue_start = std::ranges::find_if(
        scenario.units,
        [](const aoe::UnitPlacement& unit) {
            return unit.owner == aoe::Player::blue;
        }
    );
    const auto red_start = std::ranges::find_if(
        scenario.units,
        [](const aoe::UnitPlacement& unit) {
            return unit.owner == aoe::Player::red;
        }
    );
    require(blue_start != scenario.units.end());
    require(red_start != scenario.units.end());
    require(simulation.is_explored(
        aoe::Player::blue, blue_start->position
    ));
    require(simulation.is_explored(
        aoe::Player::red, red_start->position
    ));
}

void town_center_garrison_shelters_fires_and_persists() {
    const aoe::BuildingRules& rules =
        aoe::rules_for(aoe::BuildingKind::town_center);
    require(rules.hit_points == 2400);
    require(rules.wood_cost == 275);
    require(rules.stone_cost == 100);
    require(rules.footprint_width == 4);
    require(rules.footprint_height == 4);
    require(rules.vision_range == 8);
    require(rules.minimum_age == aoe::Age::castle);
    require(rules.attack == 5);
    require(rules.attack_interval_ticks == 10);
    require(rules.attack_range == 6);
    require(rules.damage_class == aoe::DamageClass::pierce);

    aoe::Simulation simulation(aoe::GameMap(12, 9));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::militia,
        aoe::Player::red,
        {8, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {8, 5}
    );

    require(simulation.command_unit(villager, {2, 2}));
    simulation.update();
    require(simulation.garrison_count(town_center) == 1);
    require(!simulation.select_unit_at({2, 2}, aoe::Player::blue));
    require(simulation.projectiles().size() == 1);
    require(simulation.projectiles().front().damage == 5);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-garrison.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.garrison_count(town_center) == 1);
    require(loaded.projectiles().size() == 1);

    for (int tick = 0; tick < 3; ++tick) {
        simulation.update();
        loaded.update();
    }
    const auto target = std::ranges::find_if(
        simulation.units(),
        [attacker](const aoe::Unit& unit) { return unit.id == attacker; }
    );
    require(target != simulation.units().end());
    require(target->hit_points == 36);
    const auto loaded_target = std::ranges::find_if(
        loaded.units(),
        [attacker](const aoe::Unit& unit) { return unit.id == attacker; }
    );
    require(loaded_target != loaded.units().end());
    require(loaded_target->hit_points == target->hit_points);

    aoe::Replay replay;
    replay.record(
        simulation.tick_number(),
        aoe::UngarrisonCommand{town_center}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-ungarrison.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(simulation);
    require(simulation.garrison_count(town_center) == 0);
    const auto released = std::ranges::find_if(
        simulation.units(),
        [villager](const aoe::Unit& unit) { return unit.id == villager; }
    );
    require(released != simulation.units().end());
    require(released->garrisoned_in == 0);
    require(released->position != aoe::TilePosition{2, 2});

    aoe::Simulation capacity(aoe::GameMap(20, 10));
    const aoe::EntityId shelter = capacity.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {5, 5}
    );
    for (int index = 0; index < 16; ++index) {
        capacity.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {index, 0}
        );
    }
    auto occupants = capacity.units();
    for (int index = 0; index < 15; ++index) {
        occupants[index].garrisoned_in = shelter;
        occupants[index].position = {5, 5};
    }
    const aoe::EntityId overflow = occupants.back().id;
    capacity.replace_state(
        std::move(occupants),
        capacity.buildings(),
        capacity.economy(aoe::Player::blue),
        capacity.economy(aoe::Player::red),
        0
    );
    require(capacity.garrison_count(shelter) == 15);
    require(!capacity.command_unit(overflow, {5, 5}));

    aoe::Scenario scenario(10, 8);
    scenario.units.push_back({
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {0, 0},
        aoe::TilePosition{4, 4},
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {4, 4},
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-garrison.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(from_scenario.garrison_count(
        from_scenario.buildings().front().id
    ) == 1);
}

void defensive_garrisons_use_dat_capacities_and_bounded_domains() {
    const auto capacity_is = [](aoe::BuildingKind kind, int expected) {
        aoe::Simulation simulation(aoe::GameMap(40, 12));
        const aoe::EntityId shelter = simulation.add_building(
            kind, aoe::Player::blue, {20, 5}
        );
        for (int index = 0; index <= expected; ++index) {
            simulation.add_unit(
                aoe::UnitKind::villager,
                aoe::Player::blue,
                {index, 0}
            );
        }
        auto occupants = simulation.units();
        for (int index = 0; index < expected; ++index) {
            occupants[static_cast<std::size_t>(index)].garrisoned_in =
                shelter;
            occupants[static_cast<std::size_t>(index)].position = {20, 5};
        }
        const aoe::EntityId overflow = occupants.back().id;
        simulation.replace_state(
            std::move(occupants),
            simulation.buildings(),
            simulation.economy(aoe::Player::blue),
            simulation.economy(aoe::Player::red),
            0
        );
        require(simulation.garrison_count(shelter) == expected);
        require(!simulation.command_unit(overflow, {20, 5}));
    };
    capacity_is(aoe::BuildingKind::town_center, 15);
    capacity_is(aoe::BuildingKind::castle, 20);
    capacity_is(aoe::BuildingKind::watch_tower, 5);
    capacity_is(aoe::BuildingKind::bombard_tower, 5);

    aoe::GameMap map(24, 12);
    map.set_terrain({1, 1}, aoe::Terrain::water);
    aoe::Simulation simulation(std::move(map));
    const aoe::EntityId tower = simulation.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId castle = simulation.add_building(
        aoe::BuildingKind::castle,
        aoe::Player::blue,
        {12, 4}
    );
    const aoe::EntityId unfinished = simulation.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {20, 4}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {0, 7}
    );
    auto buildings = simulation.buildings();
    const auto unfinished_building = std::ranges::find(
        buildings, unfinished, &aoe::Building::id
    );
    require(unfinished_building != buildings.end());
    unfinished_building->construction_ticks_remaining = 10;
    simulation.replace_state(
        simulation.units(),
        std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        0
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 4}
    );
    const aoe::EntityId monk = simulation.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {4, 3}
    );
    const aoe::EntityId knight = simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {11, 4}
    );
    const aoe::EntityId ship = simulation.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::blue, {1, 1}
    );
    const aoe::EntityId enemy = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {0, 4}
    );
    const aoe::EntityId unfinished_guest = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {19, 4}
    );

    require(simulation.command_unit(villager, {4, 4}));
    require(simulation.command_unit(monk, {4, 4}));
    require(!simulation.command_unit(knight, {4, 4}));
    require(simulation.command_unit(knight, {12, 4}));
    require(!simulation.command_unit(ship, {12, 4}));
    require(simulation.command_unit(enemy, {4, 4}));
    require(simulation.command_unit(unfinished_guest, {20, 4}));
    for (int tick = 0; tick < 3; ++tick) simulation.update();
    require(simulation.garrison_count(tower) == 2);
    require(simulation.garrison_count(castle) == 1);
    const auto incomplete_unit = std::ranges::find(
        simulation.units(), unfinished_guest, &aoe::Unit::id
    );
    require(incomplete_unit != simulation.units().end());
    require(incomplete_unit->garrisoned_in == 0);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-defensive-garrisons.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.garrison_count(tower) == 2);
    require(loaded.garrison_count(castle) == 1);
    require(loaded.ungarrison_at(tower));
    require(loaded.garrison_count(tower) == 0);

    aoe::Simulation replayed(aoe::GameMap(10, 8));
    const aoe::EntityId replay_tower = replayed.add_building(
        aoe::BuildingKind::watch_tower,
        aoe::Player::blue,
        {4, 4}
    );
    const aoe::EntityId replay_villager = replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 4}
    );
    aoe::Replay replay;
    replay.record(
        replayed.tick_number(),
        aoe::MoveUnitCommand{replay_villager, {4, 4}}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-defensive-garrisons.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    replayed.update();
    require(replayed.garrison_count(replay_tower) == 1);
}

void scenario_garrisons_use_authoritative_domains_and_capacity() {
    aoe::Scenario scenario(32, 12);
    scenario.blue_technologies = {
        aoe::Technology::guard_tower,
        aoe::Technology::keep,
    };
    scenario.buildings = {
        {aoe::BuildingKind::town_center, aoe::Player::blue, {2, 2}},
        {aoe::BuildingKind::castle, aoe::Player::blue, {10, 2}},
        {aoe::BuildingKind::watch_tower, aoe::Player::blue, {20, 2}},
        {aoe::BuildingKind::bombard_tower, aoe::Player::blue, {25, 2}},
    };
    scenario.units = {
        {aoe::UnitKind::villager, aoe::Player::blue, {0, 0}, {{2, 2}}},
        {aoe::UnitKind::knight, aoe::Player::blue, {1, 0}, {{10, 2}}},
        {aoe::UnitKind::archer, aoe::Player::blue, {2, 0}, {{20, 2}}},
        {aoe::UnitKind::villager, aoe::Player::blue, {3, 0}, {{25, 2}}},
    };
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-scenario-garrison-v61.scenario";
    aoe::save_scenario(scenario, path);
    const aoe::Scenario parsed = aoe::load_scenario(path);
    std::filesystem::remove(path);
    const aoe::Simulation restored = aoe::create_simulation(parsed);
    for (const aoe::Building& building : restored.buildings()) {
        require(restored.garrison_count(building.id) == 1);
    }

    const auto rejected = [](aoe::UnitKind kind, aoe::BuildingKind shelter) {
        aoe::Scenario invalid(12, 8);
        invalid.buildings.push_back({
            shelter, aoe::Player::blue, {5, 3}
        });
        invalid.units.push_back({
            kind, aoe::Player::blue, {0, 0}, aoe::TilePosition{5, 3}
        });
        bool threw = false;
        try {
            (void)aoe::create_simulation(invalid);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        require(threw);
    };
    rejected(aoe::UnitKind::transport_ship, aoe::BuildingKind::town_center);
    rejected(aoe::UnitKind::mangonel, aoe::BuildingKind::castle);
    rejected(aoe::UnitKind::knight, aoe::BuildingKind::town_center);

    aoe::Scenario overflow(20, 10);
    overflow.buildings.push_back({
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {10, 4}
    });
    for (int index = 0; index < 6; ++index) {
        overflow.units.push_back({
            aoe::UnitKind::archer, aoe::Player::blue, {index, 0},
            aoe::TilePosition{10, 4}
        });
    }
    bool overflow_threw = false;
    try {
        (void)aoe::create_simulation(overflow);
    } catch (const std::invalid_argument&) {
        overflow_threw = true;
    }
    require(overflow_threw);
}

void garrison_healing_maps_dat_rates_to_deterministic_ticks() {
    aoe::Simulation simulation(aoe::GameMap(32, 14));
    const auto town_center = simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {2, 2}
    );
    const auto castle = simulation.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {10, 2}
    );
    const auto tower = simulation.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {20, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {26, 8}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {1, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 0}
    );
    auto occupants = simulation.units();
    occupants[0].garrisoned_in = town_center;
    occupants[0].position = {2, 2};
    occupants[0].hit_points = 20;
    occupants[1].garrisoned_in = castle;
    occupants[1].position = {10, 2};
    occupants[1].hit_points = 80;
    occupants[2].garrisoned_in = tower;
    occupants[2].position = {20, 2};
    occupants[2].hit_points = 20;
    simulation.replace_state(
        std::move(occupants),
        simulation.buildings(),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        0
    );
    for (int tick = 0; tick < 24; ++tick) simulation.update();
    require(simulation.units()[0].hit_points == 20);
    require(simulation.units()[1].hit_points == 80);
    require(simulation.units()[2].hit_points == 20);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-garrison-healing.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    simulation.update();
    loaded.update();
    require(simulation.units()[1].hit_points == 81);
    require(loaded.units()[1].hit_points == 81);
    require(simulation.units()[0].hit_points == 20);
    require(simulation.units()[2].hit_points == 20);

    for (int tick = 0; tick < 25; ++tick) simulation.update();
    require(simulation.units()[0].hit_points == 21);
    require(simulation.units()[1].hit_points == 82);
    require(simulation.units()[2].hit_points == 21);

    for (int tick = 0; tick < 24; ++tick) loaded.update();
    aoe::Replay replay;
    replay.record(loaded.tick_number(), aoe::UngarrisonCommand{tower});
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-garrison-healing.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(loaded);
    loaded.update();
    require(loaded.units()[2].garrisoned_in == 0);
    require(loaded.units()[2].hit_points == 20);

    aoe::Simulation destruction(aoe::GameMap(12, 8));
    const auto doomed = destruction.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {4, 3}
    );
    destruction.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 1}
    );
    destruction.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 6}
    );
    auto doomed_units = destruction.units();
    doomed_units[0].garrisoned_in = doomed;
    doomed_units[0].position = {4, 3};
    doomed_units[0].hit_points = 20;
    auto doomed_buildings = destruction.buildings();
    doomed_buildings[0].hit_points = 0;
    destruction.replace_state(
        std::move(doomed_units),
        std::move(doomed_buildings),
        destruction.economy(aoe::Player::blue),
        destruction.economy(aoe::Player::red),
        49
    );
    destruction.update();
    require(destruction.units()[0].garrisoned_in == 0);
    require(destruction.units()[0].hit_points == 20);
}

void garrisoned_archers_and_villagers_add_bounded_defensive_volleys() {
    require(aoe::garrison_volley_projectile_count(
        aoe::BuildingKind::town_center, 0, 15
    ) == 10);
    require(aoe::garrison_volley_projectile_count(
        aoe::BuildingKind::castle, 5, 20
    ) == 20);
    require(aoe::garrison_volley_projectile_count(
        aoe::BuildingKind::watch_tower, 1, 5
    ) == 5);
    require(aoe::garrison_volley_projectile_count(
        aoe::BuildingKind::bombard_tower, 1, 5
    ) == 1);

    const auto volley_size = [](
        aoe::BuildingKind kind,
        std::initializer_list<aoe::UnitKind> occupants
    ) {
        aoe::Simulation simulation(aoe::GameMap(18, 12));
        const auto shelter = simulation.add_building(
            kind, aoe::Player::blue, {2, 2}
        );
        const auto target = simulation.add_unit(
            aoe::UnitKind::knight, aoe::Player::red, {9, 3}
        );
        require(simulation.set_unit_stance(
            target, aoe::UnitStance::passive
        ));
        int x = 0;
        for (const aoe::UnitKind occupant : occupants) {
            simulation.add_unit(occupant, aoe::Player::blue, {x++, 10});
        }
        auto units = simulation.units();
        for (std::size_t index = 1; index < units.size(); ++index) {
            units[index].garrisoned_in = shelter;
            units[index].position = {2, 2};
        }
        simulation.replace_state(
            std::move(units),
            simulation.buildings(),
            simulation.economy(aoe::Player::blue),
            simulation.economy(aoe::Player::red),
            0
        );
        const auto save_path =
            std::filesystem::temp_directory_path() /
            "aoe-garrison-volley.save";
        aoe::save_game(simulation, save_path);
        aoe::Simulation loaded = aoe::load_game(save_path);
        std::filesystem::remove(save_path);
        simulation.update();
        loaded.update();
        const auto blue_projectiles = [](const aoe::Simulation& value) {
            return std::ranges::count_if(
                value.projectiles(),
                [](const aoe::Projectile& projectile) {
                    return projectile.owner == aoe::Player::blue;
                }
            );
        };
        require(blue_projectiles(simulation) == blue_projectiles(loaded));
        return blue_projectiles(simulation);
    };

    require(volley_size(
        aoe::BuildingKind::town_center,
        {aoe::UnitKind::villager, aoe::UnitKind::archer,
         aoe::UnitKind::militia}
    ) == 2);
    require(volley_size(
        aoe::BuildingKind::castle,
        {aoe::UnitKind::villager, aoe::UnitKind::archer,
         aoe::UnitKind::cavalry_archer, aoe::UnitKind::militia}
    ) == 8);
    require(volley_size(
        aoe::BuildingKind::watch_tower,
        {aoe::UnitKind::villager, aoe::UnitKind::archer,
         aoe::UnitKind::militia}
    ) == 3);
    require(volley_size(
        aoe::BuildingKind::watch_tower,
        {aoe::UnitKind::archer, aoe::UnitKind::archer,
         aoe::UnitKind::archer, aoe::UnitKind::archer,
         aoe::UnitKind::archer}
    ) == 5);
    require(volley_size(
        aoe::BuildingKind::bombard_tower,
        {aoe::UnitKind::villager, aoe::UnitKind::archer}
    ) == 1);
}

void town_center_constructs_with_original_footprint_and_cost() {
    aoe::Simulation simulation(aoe::GameMap(14, 12));
    const aoe::EntityId builder = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 5}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 11}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {8, 0}
    );
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {275, 0, 0, 100},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::town_center,
        {2, 4}
    ));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::dark);
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {275, 0, 0, 99},
        simulation.economy(aoe::Player::red),
        0
    );
    require(!simulation.construct_building_at(
        builder,
        aoe::BuildingKind::town_center,
        {2, 4}
    ));
    simulation.replace_state(
        simulation.units(),
        simulation.buildings(),
        {275, 0, 0, 100},
        simulation.economy(aoe::Player::red),
        0
    );

    aoe::Simulation replayed = simulation;
    aoe::Replay replay;
    replay.record(
        0,
        aoe::ConstructBuildingCommand{
            builder,
            aoe::BuildingKind::town_center,
            {2, 4},
        }
    );
    replay.apply_current_tick(simulation);
    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-town-center-build.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(simulation.economy(aoe::Player::blue).wood == 0);
    require(simulation.economy(aoe::Player::blue).stone == 0);
    require(!simulation.buildings().back().completed());

    bool footprint_blocked = false;
    try {
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {5, 7}
        );
    } catch (const std::invalid_argument&) {
        footprint_blocked = true;
    }
    require(footprint_blocked);

    for (int tick = 0; tick < 8; ++tick) {
        simulation.update();
        replayed.update();
    }
    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-town-center-build.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.buildings().back().construction_ticks_remaining ==
        simulation.buildings().back().construction_ticks_remaining
    );
    for (int tick = 8;
         tick < aoe::rules_for(
             aoe::BuildingKind::town_center
         ).construction_ticks;
         ++tick) {
        simulation.update();
        replayed.update();
        loaded.update();
    }
    for (const aoe::Simulation* candidate : {
             &simulation,
             &replayed,
             &loaded,
         }) {
        require(candidate->buildings().back().completed());
        require(candidate->buildings().back().hit_points == 2400);
        require(candidate->population_capacity(aoe::Player::blue) == 10);
    }

    aoe::Simulation nomad(aoe::GameMap(9, 8));
    const aoe::EntityId nomad_builder = nomad.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 4}
    );
    nomad.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {8, 7}
    );
    nomad.replace_state(
        nomad.units(),
        nomad.buildings(),
        {275, 0, 0, 100},
        nomad.economy(aoe::Player::red),
        0
    );
    require(nomad.construct_building_at(
        nomad_builder,
        aoe::BuildingKind::town_center,
        {2, 3}
    ));
}

void building_rally_points_route_spawned_units_and_persist() {
    aoe::GameMap map(16, 10);
    map.set_terrain({8, 4}, aoe::Terrain::forest);
    aoe::Simulation simulation(map);
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 9}
    );

    aoe::Replay replay;
    replay.record(0, aoe::SetRallyPointCommand{town_center, {8, 4}});
    replay.record(
        0,
        aoe::QueueUnitCommand{town_center, aoe::UnitKind::villager}
    );
    replay.apply_current_tick(simulation);
    require(simulation.buildings().front().has_rally_point);
    require(
        simulation.buildings().front().rally_point ==
        aoe::TilePosition(8, 4)
    );

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-rally.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.buildings().front().has_rally_point);
    require(
        loaded.buildings().front().rally_point ==
        aoe::TilePosition(8, 4)
    );

    for (int tick = 0;
         tick < 1000 &&
         !loaded.buildings().front().production_queue.empty();
         ++tick) {
        loaded.update();
    }
    require(loaded.buildings().front().production_queue.empty());
    const auto trained = std::ranges::find_if(
        loaded.units(),
        [](const aoe::Unit& unit) {
            return unit.owner == aoe::Player::blue &&
                   unit.kind == aoe::UnitKind::villager;
        }
    );
    require(trained != loaded.units().end());
    require(trained->has_resource_target);
    require(trained->resource_target == aoe::TilePosition(8, 4));

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-rally.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(map);
    replayed.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {15, 9}
    );
    loaded_replay.apply_current_tick(replayed);
    require(replayed.buildings().front().has_rally_point);
    require(replayed.buildings().front().production_queue.size() == 1);

    aoe::Scenario scenario(16, 10);
    scenario.buildings.push_back({
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0},
        aoe::TilePosition{8, 4},
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-rally.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(from_scenario.buildings().front().has_rally_point);
    require(
        from_scenario.buildings().front().rally_point ==
        aoe::TilePosition(8, 4)
    );
}

void production_cancellation_refunds_last_order_and_preserves_progress() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const aoe::EntityId town_center = simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    require(simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
    require(simulation.queue_unit_at(
        town_center,
        aoe::UnitKind::villager
    ));
    for (int tick = 0; tick < 7; ++tick) {
        simulation.update();
    }
    const int active_progress =
        simulation.buildings().front().production_queue.front()
            .ticks_remaining;
    const int food_before =
        simulation.economy(aoe::Player::blue).food;
    aoe::Simulation replayed = simulation;

    aoe::Replay replay;
    replay.record(
        simulation.tick_number(),
        aoe::CancelProductionCommand{town_center}
    );
    replay.apply_current_tick(simulation);
    require(simulation.buildings().front().production_queue.size() == 1);
    require(
        simulation.buildings().front().production_queue.front()
            .ticks_remaining == active_progress
    );
    require(
        simulation.economy(aoe::Player::blue).food ==
        food_before + aoe::rules_for(aoe::UnitKind::villager).food_cost
    );

    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-cancel-production.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(state_fingerprint(replayed) == state_fingerprint(simulation));

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-cancel-production.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const int loaded_food = loaded.economy(aoe::Player::blue).food;
    require(loaded.cancel_production_at(town_center));
    require(loaded.buildings().front().production_queue.empty());
    require(
        loaded.economy(aoe::Player::blue).food ==
        loaded_food + aoe::rules_for(aoe::UnitKind::villager).food_cost
    );
    require(!loaded.cancel_production_at(town_center));
}

void stop_command_clears_all_orders_and_replays_for_groups() {
    aoe::GameMap map(14, 9);
    map.set_terrain({8, 1}, aoe::Terrain::forest);
    aoe::Simulation simulation(map);
    const aoe::EntityId gatherer = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {7, 1}
    );
    const aoe::EntityId attacker = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 4}
    );
    const aoe::EntityId mover = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 7}
    );
    const aoe::EntityId repairer = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {3, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {10, 4}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {4, 0}
    );
    std::vector<aoe::Building> damaged = simulation.buildings();
    damaged.front().hit_points = 100;
    simulation.replace_state(
        simulation.units(),
        std::move(damaged),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );
    require(simulation.command_unit(gatherer, {8, 1}));
    require(simulation.command_unit(attacker, {10, 4}));
    require(simulation.command_unit(mover, {10, 7}));
    require(simulation.command_unit(repairer, {4, 0}));
    require(simulation.units()[0].has_resource_target);
    require(simulation.units()[1].attack_target_id != 0);
    require(simulation.units()[2].moving);
    require(simulation.units()[3].repair_target_id != 0);

    aoe::Simulation replayed = simulation;
    aoe::Replay replay;
    for (aoe::EntityId unit : {gatherer, attacker, mover, repairer}) {
        replay.record(
            simulation.tick_number(),
            aoe::StopUnitCommand{unit}
        );
    }
    replay.apply_current_tick(simulation);
    for (std::size_t index = 0; index < 4; ++index) {
        const aoe::Unit& unit = simulation.units()[index];
        require(!unit.moving);
        require(unit.path.empty());
        require(unit.destination == unit.position);
        require(unit.attack_target_id == 0);
        require(unit.repair_target_id == 0);
        require(!unit.has_resource_target);
        require(!unit.returning_resource);
        require(unit.garrison_target_id == 0);
    }

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-stop.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    for (std::size_t index = 0; index < 4; ++index) {
        require(!replayed.units()[index].moving);
        require(replayed.units()[index].path.empty());
        require(replayed.units()[index].attack_target_id == 0);
        require(!replayed.units()[index].has_resource_target);
    }

    aoe::Simulation shelter(aoe::GameMap(10, 8));
    const aoe::EntityId town_center = shelter.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId refugee = shelter.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 1}
    );
    shelter.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 7}
    );
    require(shelter.command_unit(refugee, {0, 0}));
    require(shelter.units().front().garrison_target_id == town_center);
    require(shelter.stop_unit(refugee));
    require(shelter.units().front().garrison_target_id == 0);

    aoe::Simulation construction(aoe::GameMap(12, 8));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 4}
    );
    construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {500, 500, 500, 500},
        construction.economy(aoe::Player::red),
        0
    );
    require(construction.construct_building_at(
        builder,
        aoe::BuildingKind::barracks,
        {2, 3}
    ));
    require(construction.buildings().front().builder_id == builder);
    const int progress =
        construction.buildings().front().construction_ticks_remaining;
    require(construction.stop_unit(builder));
    require(construction.buildings().front().builder_id == 0);
    for (int tick = 0; tick < 5; ++tick) {
        construction.update();
    }
    require(
        construction.buildings().front().construction_ticks_remaining ==
        progress
    );
}

void attack_move_engages_visible_enemies_then_resumes_destination() {
    aoe::Simulation simulation(aoe::GameMap(30, 10));
    const aoe::EntityId archer = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId enemy = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );

    aoe::Replay replay;
    replay.record(0, aoe::AttackMoveCommand{archer, {14, 2}});
    replay.apply_current_tick(simulation);
    require(simulation.units().front().attack_moving);
    require(
        simulation.units().front().attack_move_destination ==
        aoe::TilePosition(14, 2)
    );
    simulation.update();
    require(simulation.units().front().attack_target_id == enemy);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-attack-move.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().attack_moving);
    require(
        loaded.units().front().attack_move_destination ==
        aoe::TilePosition(14, 2)
    );
    for (int tick = 0; tick < 300; ++tick) {
        loaded.update();
        if (loaded.units().front().position ==
                aoe::TilePosition(14, 2) &&
            !loaded.units().front().attack_moving) {
            break;
        }
    }
    require(
        std::ranges::none_of(
            loaded.units(),
            [enemy](const aoe::Unit& unit) { return unit.id == enemy; }
        )
    );
    require(
        loaded.units().front().position == aoe::TilePosition(14, 2)
    );
    require(!loaded.units().front().attack_moving);

    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-attack-move.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(30, 10));
    replayed.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {6, 2}
    );
    replayed.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );
    loaded_replay.apply_current_tick(replayed);
    require(replayed.units().front().attack_moving);
    require(
        replayed.units().front().attack_move_destination ==
        aoe::TilePosition(14, 2)
    );
    require(replayed.command_unit(archer, {3, 3}));
    require(!replayed.units().front().attack_moving);

    aoe::Scenario scenario(20, 8);
    scenario.units.push_back({
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2},
        std::nullopt,
        aoe::TilePosition{12, 2},
        std::nullopt,
        std::nullopt,
        false,
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 5},
        std::nullopt,
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-attack-move.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(from_scenario.units().front().attack_moving);
    require(
        from_scenario.units().front().attack_move_destination ==
        aoe::TilePosition(12, 2)
    );
}

void patrol_engages_enemies_and_loops_between_endpoints() {
    aoe::Simulation simulation(aoe::GameMap(30, 10));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId enemy = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {6, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );

    aoe::Replay replay;
    replay.record(0, aoe::PatrolCommand{scout, {12, 2}});
    replay.apply_current_tick(simulation);
    require(simulation.units().front().patrolling);
    require(
        simulation.units().front().patrol_origin ==
        aoe::TilePosition(2, 2)
    );
    require(
        simulation.units().front().patrol_destination ==
        aoe::TilePosition(12, 2)
    );
    simulation.update();
    require(simulation.units().front().attack_target_id == enemy);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-patrol.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().patrolling);

    bool reached_far_endpoint = false;
    bool returned_to_origin = false;
    for (int tick = 0; tick < 400; ++tick) {
        loaded.update();
        if (loaded.units().front().position ==
            aoe::TilePosition(12, 2)) {
            reached_far_endpoint = true;
        }
        if (reached_far_endpoint &&
            loaded.units().front().position ==
                aoe::TilePosition(2, 2)) {
            returned_to_origin = true;
            break;
        }
    }
    require(
        std::ranges::none_of(
            loaded.units(),
            [enemy](const aoe::Unit& unit) { return unit.id == enemy; }
        )
    );
    require(reached_far_endpoint);
    require(returned_to_origin);
    require(loaded.units().front().patrolling);

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-patrol.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(30, 10));
    replayed.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {2, 2}
    );
    replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {6, 2}
    );
    replayed.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );
    loaded_replay.apply_current_tick(replayed);
    require(replayed.units().front().patrolling);
    require(replayed.stop_unit(scout));
    require(!replayed.units().front().patrolling);

    aoe::Scenario scenario(20, 8);
    scenario.units.push_back({
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {2, 2},
        std::nullopt,
        std::nullopt,
        aoe::TilePosition{12, 2},
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 5},
        std::nullopt,
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-patrol.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(from_scenario.units().front().patrolling);
    require(
        from_scenario.units().front().patrol_destination ==
        aoe::TilePosition(12, 2)
    );
}

void guard_follows_protects_and_returns_to_friendly_target() {
    aoe::Simulation simulation(aoe::GameMap(30, 10));
    const aoe::EntityId guard = simulation.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId protected_scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId enemy = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {12, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );

    aoe::Replay replay;
    replay.record(
        0,
        aoe::GuardCommand{guard, protected_scout, false}
    );
    replay.apply_current_tick(simulation);
    require(simulation.units()[0].guard_target_id == protected_scout);
    require(!simulation.units()[0].guard_target_is_building);
    require(simulation.command_unit(protected_scout, {10, 2}));
    for (int tick = 0; tick < 300; ++tick) {
        simulation.update();
        const bool enemy_alive = std::ranges::any_of(
            simulation.units(),
            [enemy](const aoe::Unit& unit) { return unit.id == enemy; }
        );
        if (!enemy_alive &&
            std::abs(
                simulation.units()[0].position.x -
                simulation.units()[1].position.x
            ) +
                std::abs(
                    simulation.units()[0].position.y -
                    simulation.units()[1].position.y
                ) <= 2) {
            break;
        }
    }
    require(
        std::ranges::none_of(
            simulation.units(),
            [enemy](const aoe::Unit& unit) { return unit.id == enemy; }
        )
    );
    require(simulation.units()[0].guard_target_id == protected_scout);
    require(
        std::abs(
            simulation.units()[0].position.x -
            simulation.units()[1].position.x
        ) +
            std::abs(
                simulation.units()[0].position.y -
                simulation.units()[1].position.y
            ) <= 2
    );

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-guard.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units()[0].guard_target_id == protected_scout);
    require(!loaded.units()[0].guard_target_is_building);

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-guard.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(30, 10));
    replayed.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    replayed.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {4, 2}
    );
    replayed.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {12, 2}
    );
    replayed.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {27, 7}
    );
    loaded_replay.apply_current_tick(replayed);
    require(replayed.units()[0].guard_target_id == protected_scout);
    require(replayed.stop_unit(guard));
    require(replayed.units()[0].guard_target_id == 0);

    aoe::Simulation building_guard(aoe::GameMap(14, 8));
    const aoe::EntityId spearman = building_guard.add_unit(
        aoe::UnitKind::spearman,
        aoe::Player::blue,
        {7, 3}
    );
    const aoe::EntityId house = building_guard.add_building(
        aoe::BuildingKind::house,
        aoe::Player::blue,
        {2, 2}
    );
    building_guard.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 7}
    );
    require(building_guard.command_guard(spearman, house, true));
    require(building_guard.units().front().guard_target_id == house);
    require(building_guard.units().front().guard_target_is_building);

    aoe::Scenario scenario(20, 8);
    scenario.units.push_back({
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        aoe::TilePosition{4, 2},
        false,
    });
    scenario.units.push_back({
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {4, 2},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        false,
    });
    scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 5},
        std::nullopt,
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-guard.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(
        from_scenario.units()[0].guard_target_id ==
        from_scenario.units()[1].id
    );
    require(!from_scenario.units()[0].guard_target_is_building);
}

void queued_waypoints_run_multi_leg_routes_and_continue_after_combat() {
    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const aoe::EntityId scout = simulation.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 7}
    );
    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{scout, {4, 1}});
    replay.record(0, aoe::QueueWaypointCommand{scout, {4, 4}});
    replay.record(0, aoe::QueueWaypointCommand{scout, {9, 4}});
    replay.apply_current_tick(simulation);
    require(simulation.units().front().waypoints.size() == 2);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-waypoints.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(loaded.units().front().waypoints.size() == 2);
    for (int tick = 0; tick < 100; ++tick) {
        loaded.update();
        if (loaded.units().front().position ==
                aoe::TilePosition(9, 4) &&
            loaded.units().front().waypoints.empty() &&
            !loaded.units().front().moving) {
            break;
        }
    }
    require(loaded.units().front().position == aoe::TilePosition(9, 4));
    require(loaded.units().front().waypoints.empty());

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-waypoints.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(20, 10));
    replayed.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1}
    );
    replayed.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 7}
    );
    loaded_replay.apply_current_tick(replayed);
    require(replayed.units().front().waypoints.size() == 2);
    require(replayed.stop_unit(scout));
    require(replayed.units().front().waypoints.empty());

    aoe::Simulation combat(aoe::GameMap(24, 8));
    const aoe::EntityId archer = combat.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {1, 2}
    );
    const aoe::EntityId enemy = combat.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {5, 2}
    );
    combat.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {21, 5}
    );
    require(combat.command_unit(archer, {5, 2}));
    require(combat.queue_waypoint(archer, {11, 2}));
    for (int tick = 0; tick < 300; ++tick) {
        combat.update();
        if (combat.units().front().position ==
                aoe::TilePosition(11, 2) &&
            combat.units().front().waypoints.empty()) {
            break;
        }
    }
    require(
        std::ranges::none_of(
            combat.units(),
            [enemy](const aoe::Unit& unit) { return unit.id == enemy; }
        )
    );
    require(combat.units().front().position == aoe::TilePosition(11, 2));

    aoe::Scenario scenario(20, 8);
    aoe::UnitPlacement placement{
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {1, 1},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        false,
        {{4, 1}, {4, 4}, {9, 4}},
    };
    scenario.units.push_back(placement);
    scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::red,
        {17, 5},
        std::nullopt,
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-waypoints.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(
        from_scenario.units().front().destination ==
        aoe::TilePosition(4, 1)
    );
    require(from_scenario.units().front().waypoints.size() == 2);
}

void unit_stances_control_chasing_and_persist() {
    aoe::Simulation stand_ground(aoe::GameMap(24, 8));
    const aoe::EntityId archer = stand_ground.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId nearby_enemy = stand_ground.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {7, 2}
    );
    stand_ground.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {21, 5}
    );
    require(stand_ground.set_unit_stance(
        archer,
        aoe::UnitStance::stand_ground
    ));
    for (int tick = 0; tick < 8; ++tick) {
        stand_ground.update();
    }
    require(
        stand_ground.units().front().position ==
        aoe::TilePosition(2, 2)
    );
    require(stand_ground.units().front().attack_target_id == 0);
    require(stand_ground.command_unit(nearby_enemy, {6, 2}));
    const int enemy_hit_points = stand_ground.units()[1].hit_points;
    for (int tick = 0; tick < 12; ++tick) {
        stand_ground.update();
    }
    require(
        stand_ground.units().front().position ==
        aoe::TilePosition(2, 2)
    );
    require(
        stand_ground.units()[1].hit_points < enemy_hit_points
    );

    aoe::Simulation passive(aoe::GameMap(16, 8));
    const aoe::EntityId passive_archer = passive.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    passive.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {3, 2}
    );
    passive.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {13, 5}
    );
    require(passive.set_unit_stance(
        passive_archer,
        aoe::UnitStance::passive
    ));
    const int passive_enemy_hp = passive.units()[1].hit_points;
    for (int tick = 0; tick < 10; ++tick) {
        passive.update();
    }
    require(passive.units()[1].hit_points == passive_enemy_hp);
    require(passive.command_unit(passive_archer, {3, 2}));
    for (int tick = 0; tick < 8; ++tick) {
        passive.update();
    }
    require(passive.units()[1].hit_points < passive_enemy_hp);

    aoe::Simulation defensive(aoe::GameMap(24, 8));
    const aoe::EntityId defender = defensive.add_unit(
        aoe::UnitKind::scout_cavalry,
        aoe::Player::blue,
        {2, 2}
    );
    const aoe::EntityId lure = defensive.add_unit(
        aoe::UnitKind::knight,
        aoe::Player::red,
        {4, 2}
    );
    defensive.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {21, 5}
    );
    require(defensive.set_unit_stance(
        defender,
        aoe::UnitStance::defensive
    ));
    require(defensive.command_unit(lure, {17, 2}));
    for (int tick = 0; tick < 100; ++tick) {
        defensive.update();
        if (defensive.units().front().position ==
                aoe::TilePosition(2, 2) &&
            defensive.units().front().attack_target_id == 0 &&
            !defensive.units().front().moving) {
            break;
        }
    }
    require(
        defensive.units().front().position ==
        aoe::TilePosition(2, 2)
    );
    require(defensive.units().front().attack_target_id == 0);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-stance.save";
    aoe::save_game(defensive, save_path);
    const aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(
        loaded.units().front().stance ==
        aoe::UnitStance::defensive
    );
    require(
        loaded.units().front().stance_anchor ==
        aoe::TilePosition(2, 2)
    );

    aoe::Replay replay;
    replay.record(
        0,
        aoe::SetStanceCommand{1, aoe::UnitStance::stand_ground}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-stance.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(10, 6));
    replayed.add_unit(
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2}
    );
    replayed.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {7, 3}
    );
    loaded_replay.apply_current_tick(replayed);
    require(
        replayed.units().front().stance ==
        aoe::UnitStance::stand_ground
    );

    aoe::Scenario scenario(12, 6);
    aoe::UnitPlacement placement{
        aoe::UnitKind::archer,
        aoe::Player::blue,
        {2, 2},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        false,
        {},
        aoe::UnitStance::passive,
    };
    scenario.units.push_back(placement);
    scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::Player::red,
        {9, 3},
        std::nullopt,
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-stance.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario parsed = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    const aoe::Simulation from_scenario = aoe::create_simulation(parsed);
    require(
        from_scenario.units().front().stance ==
        aoe::UnitStance::passive
    );
}

void deleting_entities_refunds_unbuilt_cost_and_ejects_garrison() {
    aoe::Simulation construction(aoe::GameMap(14, 9));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 4}
    );
    construction.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 8}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {500, 500, 500, 500},
        construction.economy(aoe::Player::red),
        0
    );
    require(construction.construct_building_at(
        builder,
        aoe::BuildingKind::barracks,
        {2, 3}
    ));
    for (int tick = 0; tick < 10; ++tick) {
        construction.update();
    }
    const aoe::Building foundation = construction.buildings().front();
    const aoe::BuildingRules& rules =
        aoe::rules_for(aoe::BuildingKind::barracks);
    const int wood_before =
        construction.economy(aoe::Player::blue).wood;
    const int expected_refund =
        rules.wood_cost *
        foundation.construction_ticks_remaining /
        rules.construction_ticks;
    aoe::Simulation replayed = construction;
    aoe::Replay replay;
    replay.record(
        construction.tick_number(),
        aoe::DeleteEntityCommand{foundation.id, true}
    );
    replay.apply_current_tick(construction);
    require(construction.buildings().empty());
    require(
        construction.economy(aoe::Player::blue).wood ==
        wood_before + expected_refund
    );

    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-delete.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(replayed.buildings().empty());
    require(
        replayed.economy(aoe::Player::blue).wood ==
        construction.economy(aoe::Player::blue).wood
    );

    aoe::Simulation shelter(aoe::GameMap(12, 9));
    const aoe::EntityId town_center = shelter.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId refugee = shelter.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 1}
    );
    shelter.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 8}
    );
    require(shelter.command_unit(refugee, {0, 0}));
    shelter.update();
    require(shelter.units().front().garrisoned_in == town_center);
    const int completed_wood =
        shelter.economy(aoe::Player::blue).wood;
    require(shelter.delete_building(town_center));
    require(shelter.buildings().empty());
    require(shelter.units().front().garrisoned_in == 0);
    require(
        shelter.economy(aoe::Player::blue).wood ==
        completed_wood
    );

    aoe::Simulation defeat(aoe::GameMap(8, 6));
    const aoe::EntityId last_blue = defeat.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {1, 1}
    );
    defeat.add_building(
        aoe::BuildingKind::house,
        aoe::Player::red,
        {5, 3}
    );
    require(defeat.delete_unit(last_blue));
    require(defeat.units().empty());
    require(defeat.outcome() == aoe::MatchOutcome::red_victory);
    require(!defeat.delete_unit(last_blue));
}

void sheep_supply_food_without_using_population() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    require(simulation.population(aoe::Player::blue) == 1);
    require(simulation.command_unit(villager, {5, 2}));
    const int starting_food =
        simulation.economy(aoe::Player::blue).food;
    for (int tick = 0; tick < 300; ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).food > starting_food);
    require(
        simulation.economy(aoe::Player::blue).food <
        starting_food + 100
    );
    require(std::ranges::none_of(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::sheep;
        }
    ));
}

void owned_sheep_accepts_player_move_command() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {2, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {0, 7}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );

    require(simulation.select_unit_at({2, 3}, aoe::Player::blue));
    require(simulation.selected_unit() == sheep);
    require(simulation.command_selected({7, 3}));

    const auto commanded = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(commanded != simulation.units().end());
    require(commanded->moving);
    require(commanded->destination == aoe::TilePosition{7, 3});
    require(!commanded->path.empty());

    for (int tick = 0; tick < 30; ++tick) {
        simulation.update();
    }
    const auto arrived = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(arrived != simulation.units().end());
    require(arrived->position == aoe::TilePosition{7, 3});
    require(!arrived->moving);
}

void visible_neutral_sheep_becomes_owned_alive_and_moves() {
    aoe::Simulation simulation(aoe::GameMap(14, 8));
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 3}
    );
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::neutral,
        {5, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 7}
    );

    require(simulation.select_unit_at({5, 3}, aoe::Player::blue));
    require(!aoe::execute(
        simulation,
        aoe::GameCommand{aoe::MoveUnitCommand{sheep, {8, 3}}}
    ));
    simulation.update();

    const auto captured = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(captured != simulation.units().end());
    require(captured->owner == aoe::Player::blue);
    require(captured->hit_points == 7);
    require(aoe::execute(
        simulation,
        aoe::GameCommand{aoe::MoveUnitCommand{sheep, {8, 3}}}
    ));
    for (int tick = 0; tick < 30; ++tick) simulation.update();
    const auto arrived = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(arrived != simulation.units().end());
    require(arrived->position == aoe::TilePosition{8, 3});
    require(!arrived->moving);
}

void sheep_capture_uses_native_radius_priority_and_chaining() {
    const auto owner_of = [](const aoe::Simulation& simulation,
                             aoe::EntityId id) {
        const auto unit = std::ranges::find(
            simulation.units(), id, &aoe::Unit::id
        );
        require(unit != simulation.units().end());
        return unit->owner;
    };

    aoe::Simulation boundary(aoe::GameMap(14, 8));
    boundary.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    const auto inside = boundary.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {4, 4}
    );
    const auto outside = boundary.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {8, 1}
    );
    boundary.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {13, 7}
    );
    boundary.update();
    require(owner_of(boundary, inside) == aoe::Player::blue);
    require(owner_of(boundary, outside).is_neutral());

    aoe::Simulation contested(aoe::GameMap(12, 8));
    contested.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 3}
    );
    const auto nearer = contested.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {5, 3}
    );
    contested.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {6, 3}
    );
    contested.update();
    require(owner_of(contested, nearer) == aoe::Player::red);

    aoe::Simulation tied(aoe::GameMap(10, 6));
    tied.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 3}
    );
    const auto equal = tied.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {5, 3}
    );
    tied.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 3}
    );
    tied.update();
    require(owner_of(tied, equal) == aoe::Player::blue);

    aoe::Simulation defended(aoe::GameMap(12, 8));
    defended.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 3}
    );
    const auto retained = defended.add_unit(
        aoe::UnitKind::sheep, aoe::Player::blue, {5, 3}
    );
    defended.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {5, 2}
    );
    defended.update();
    require(owner_of(defended, retained) == aoe::Player::blue);

    aoe::Simulation stolen(aoe::GameMap(14, 8));
    stolen.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {0, 0}
    );
    const auto undefended = stolen.add_unit(
        aoe::UnitKind::sheep, aoe::Player::blue, {7, 3}
    );
    stolen.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 3}
    );
    stolen.update();
    require(owner_of(stolen, undefended) == aoe::Player::red);

    aoe::Simulation chained(aoe::GameMap(16, 8));
    chained.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 3}
    );
    const auto first = chained.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {4, 3}
    );
    const auto second = chained.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {7, 3}
    );
    const auto third = chained.add_unit(
        aoe::UnitKind::sheep, aoe::Player::neutral, {10, 3}
    );
    chained.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {15, 7}
    );
    chained.update();
    require(owner_of(chained, first) == aoe::Player::blue);
    require(owner_of(chained, second) == aoe::Player::blue);
    require(owner_of(chained, third) == aoe::Player::blue);
}

void sheep_player_movement_groups_are_deterministic_and_persistent() {
    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(16, 10));
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {0, 9}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {15, 9}
        );
        simulation.add_unit(
            aoe::UnitKind::sheep,
            aoe::Player::blue,
            {2, 2}
        );
        simulation.add_unit(
            aoe::UnitKind::sheep,
            aoe::Player::blue,
            {3, 2}
        );
        simulation.add_unit(
            aoe::UnitKind::sheep,
            aoe::Player::red,
            {14, 8}
        );
        return simulation;
    };

    aoe::Simulation group = make_simulation();
    const std::vector<aoe::EntityId> sheep{3, 4};
    require(group.select_units(sheep, aoe::Player::blue));
    require(group.selected_units() == sheep);
    require(group.command_selected({9, 4}));
    const std::vector<aoe::TilePosition> expected =
        group.formation_destinations(sheep, {9, 4});
    for (int tick = 0; tick < 100; ++tick) group.update();
    for (std::size_t index = 0; index < sheep.size(); ++index) {
        const auto unit = std::ranges::find(
            group.units(), sheep[index], &aoe::Unit::id
        );
        require(unit != group.units().end());
        require(unit->position == expected[index]);
        require(!unit->moving);
    }

    aoe::Simulation mixed = make_simulation();
    require(mixed.select_units({1, 3}, aoe::Player::blue));
    require(mixed.command_selected({8, 5}));
    for (int tick = 0; tick < 100; ++tick) mixed.update();
    require(std::ranges::all_of(
        std::array<aoe::EntityId, 2>{1, 3},
        [&mixed](aoe::EntityId id) {
            const auto unit = std::ranges::find(
                mixed.units(), id, &aoe::Unit::id
            );
            return unit != mixed.units().end() && !unit->moving &&
                unit->position.x >= 7 && unit->position.x <= 9 &&
                unit->position.y >= 4 && unit->position.y <= 6;
        }
    ));

    aoe::Simulation ownership = make_simulation();
    require(!ownership.select_units({5}, aoe::Player::blue));
    require(ownership.selected_units().empty());
    require(ownership.select_units({3, 5}, aoe::Player::blue));
    require(ownership.selected_units() == std::vector<aoe::EntityId>{3});

    aoe::Simulation replacement = make_simulation();
    require(replacement.command_unit(3, {10, 2}));
    replacement.update();
    require(replacement.command_unit(3, {5, 6}));
    const auto replaced = std::ranges::find(
        replacement.units(), 3, &aoe::Unit::id
    );
    require(replaced != replacement.units().end());
    require(replaced->destination == aoe::TilePosition{5, 6});
    require(!replacement.command_unit(3, {-1, 6}));
    require(replaced->destination == aoe::TilePosition{5, 6});
    replacement.update();
    require(replaced->moving);

    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-sheep-movement.save";
    aoe::save_game(replacement, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const auto loaded_sheep = std::ranges::find(
        loaded.units(), 3, &aoe::Unit::id
    );
    require(loaded_sheep != loaded.units().end());
    require(loaded_sheep->destination == aoe::TilePosition{5, 6});
    require(loaded_sheep->moving);
    for (int tick = 0; tick < 100; ++tick) loaded.update();
    const auto arrived_after_load = std::ranges::find(
        loaded.units(), 3, &aoe::Unit::id
    );
    require(arrived_after_load != loaded.units().end());
    require(arrived_after_load->position == aoe::TilePosition{5, 6});
    require(!arrived_after_load->moving);

    aoe::Simulation replayed = make_simulation();
    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{3, {7, 7}});
    for (int tick = 0; tick < 100; ++tick) {
        replay.apply_current_tick(replayed);
        replayed.update();
    }
    const auto replayed_sheep = std::ranges::find(
        replayed.units(), 3, &aoe::Unit::id
    );
    require(replayed_sheep != replayed.units().end());
    require(replayed_sheep->position == aoe::TilePosition{7, 7});
    require(!replayed_sheep->moving);
}

void neutral_sheep_select_and_contextual_gather() {
    aoe::Simulation simulation(aoe::GameMap(12, 8));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::neutral,
        {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );

    require(simulation.select_unit_at({5, 2}, aoe::Player::blue));
    require(simulation.selected_unit() == sheep);
    require(simulation.formation_kind(aoe::Player::neutral) ==
        aoe::FormationKind::compact);
    require(simulation.formation_destinations({sheep}, {5, 2}) ==
        std::vector<aoe::TilePosition>{{5, 2}});
    require(simulation.select_unit_at({4, 2}, aoe::Player::blue));
    require(simulation.selected_unit() == villager);
    require(simulation.command_unit(villager, {5, 2}));

    const auto claimed = std::ranges::find(
        simulation.units(), sheep, &aoe::Unit::id
    );
    require(claimed != simulation.units().end());
    require(claimed->owner == aoe::Player::blue);
    require(claimed->hit_points == 0);
    const auto gatherer = std::ranges::find(
        simulation.units(), villager, &aoe::Unit::id
    );
    require(gatherer != simulation.units().end());
    require(gatherer->resource_unit_id == sheep);
}

void sheep_state_round_trips_through_save_and_scenario() {
    aoe::Simulation simulation(aoe::GameMap(10, 6));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 2}
    );
    const aoe::EntityId sheep = simulation.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {5, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {9, 5}
    );
    require(simulation.command_gather_unit(villager, sheep));
    simulation.update();
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-sheep.save";
    aoe::save_game(simulation, save_path);
    const aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const auto loaded_sheep = std::ranges::find_if(
        loaded.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::sheep;
        }
    );
    require(loaded_sheep != loaded.units().end());
    require(loaded_sheep->food_remaining == 99);
    const auto loaded_villager = std::ranges::find_if(
        loaded.units(),
        [villager](const aoe::Unit& unit) {
            return unit.id == villager;
        }
    );
    require(loaded_villager->resource_unit_id == sheep);

    aoe::Scenario scenario(8, 6);
    aoe::UnitPlacement placement{
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {3, 3},
    };
    placement.food_remaining = 37;
    scenario.units.push_back(placement);
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-sheep.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario scenario_loaded =
        aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_loaded.units.size() == 1);
    require(
        scenario_loaded.units.front().kind ==
        aoe::UnitKind::sheep
    );
    require(scenario_loaded.units.front().food_remaining == 37);
    const aoe::Simulation scenario_simulation =
        aoe::create_simulation(scenario_loaded);
    require(scenario_simulation.units().front().food_remaining == 37);
}

void sheep_gather_command_replays_deterministically() {
    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(10, 6));
        simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {0, 0}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {4, 2}
        );
        simulation.add_unit(
            aoe::UnitKind::sheep,
            aoe::Player::blue,
            {5, 2}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {9, 5}
        );
        return simulation;
    };
    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    aoe::Replay replay;
    replay.record(0, aoe::GatherUnitCommand{2, 3});
    const auto path =
        std::filesystem::temp_directory_path() / "aoe-sheep.replay";
    aoe::save_replay(replay, path);
    aoe::Replay loaded = aoe::load_replay(path);
    std::filesystem::remove(path);
    for (int tick = 0; tick < 40; ++tick) {
        replay.apply_current_tick(first);
        loaded.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(
        first.economy(aoe::Player::blue).food ==
        second.economy(aoe::Player::blue).food
    );
    require(first.units().size() == second.units().size());
    for (std::size_t index = 0; index < first.units().size(); ++index) {
        require(
            first.units()[index].food_remaining ==
            second.units()[index].food_remaining
        );
        require(
            first.units()[index].carried_amount ==
            second.units()[index].carried_amount
        );
    }
}

void sheep_integrate_with_pathing_vision_outcomes_and_ai() {
    aoe::GameMap gathering_map(14, 8);
    gathering_map.set_terrain({10, 5}, aoe::Terrain::berry_bush);
    aoe::Simulation gathering(std::move(gathering_map));
    gathering.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId villager = gathering.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {2, 5}
    );
    gathering.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {8, 5}
    );
    gathering.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 7}
    );
    const int starting_food =
        gathering.economy(aoe::Player::blue).food;
    require(gathering.command_unit(villager, {8, 5}));
    for (int tick = 0; tick < 700; ++tick) {
        gathering.update();
    }
    require(gathering.economy(aoe::Player::blue).food > starting_food);
    require(
        gathering.economy(aoe::Player::blue).food <=
        starting_food + 100
    );
    const auto shepherd = std::ranges::find_if(
        gathering.units(), [villager](const aoe::Unit& unit) {
            return unit.id == villager;
        }
    );
    require(shepherd != gathering.units().end());
    require(!shepherd->has_resource_target);
    require(!shepherd->moving);

    aoe::Simulation visibility(aoe::GameMap(12, 8));
    visibility.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::blue,
        {5, 4}
    );
    visibility.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {11, 7}
    );
    require(visibility.is_visible(aoe::Player::blue, {7, 4}));
    require(visibility.is_visible(aoe::Player::blue, {8, 4}));
    require(!visibility.is_visible(aoe::Player::blue, {9, 4}));
    visibility.update();
    require(
        visibility.outcome() == aoe::MatchOutcome::red_victory
    );

    aoe::Simulation automated(aoe::GameMap(16, 10));
    automated.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::red,
        {0, 0}
    );
    automated.add_building(
        aoe::BuildingKind::barracks,
        aoe::Player::red,
        {10, 0}
    );
    automated.add_building(
        aoe::BuildingKind::mill,
        aoe::Player::red,
        {10, 3}
    );
    automated.add_building(
        aoe::BuildingKind::lumber_camp,
        aoe::Player::red,
        {10, 6}
    );
    const aoe::EntityId ai_villager = automated.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {6, 4}
    );
    const aoe::EntityId ai_sheep = automated.add_unit(
        aoe::UnitKind::sheep,
        aoe::Player::red,
        {7, 4}
    );
    automated.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {15, 9}
    );
    aoe::ComputerPlayer computer(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) {
        automated.update();
        computer.update(automated);
    }
    const auto villager_state = std::ranges::find_if(
        automated.units(),
        [ai_villager](const aoe::Unit& unit) {
            return unit.id == ai_villager;
        }
    );
    const auto sheep_state = std::ranges::find_if(
        automated.units(),
        [ai_sheep](const aoe::Unit& unit) {
            return unit.id == ai_sheep;
        }
    );
    require(villager_state != automated.units().end());
    require(sheep_state != automated.units().end());
    require(villager_state->resource_unit_id == ai_sheep);
    require(!sheep_state->moving);
}

void deer_are_passive_finite_huntable_food() {
    aoe::Simulation simulation(aoe::GameMap(14, 8));
    simulation.add_building(
        aoe::BuildingKind::town_center,
        aoe::Player::blue,
        {0, 0}
    );
    const aoe::EntityId hunter = simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::blue,
        {4, 3}
    );
    const aoe::EntityId deer = simulation.add_unit(
        aoe::UnitKind::deer,
        aoe::Player::red,
        {7, 3}
    );
    simulation.add_unit(
        aoe::UnitKind::villager,
        aoe::Player::red,
        {13, 7}
    );
    require(simulation.population(aoe::Player::red) == 1);
    require(simulation.idle_military(aoe::Player::red).empty());
    require(simulation.command_unit(hunter, {7, 3}));
    for (int tick = 0; tick < 30; ++tick) {
        simulation.update();
    }
    const auto carcass = std::ranges::find_if(
        simulation.units(),
        [deer](const aoe::Unit& unit) {
            return unit.id == deer;
        }
    );
    require(carcass != simulation.units().end());
    require(carcass->hit_points <= 0);
    require(carcass->food_remaining > 0);
    require(carcass->food_remaining < 140);
    require(carcass->attack_target_id == 0);
    const int starting_food =
        simulation.economy(aoe::Player::blue).food;
    require(simulation.command_unit(hunter, {7, 3}));
    for (int tick = 0; tick < 500; ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).food > starting_food);
    require(
        simulation.economy(aoe::Player::blue).food <
        starting_food + 140
    );
    require(std::ranges::none_of(
        simulation.units(),
        [deer](const aoe::Unit& unit) {
            return unit.id == deer;
        }
    ));
}

void animal_carcass_decay_is_dat_rated_persistent_and_competitive() {
    aoe::Simulation cadence(aoe::GameMap(10, 7));
    const aoe::EntityId sheep = cadence.add_unit(
        aoe::UnitKind::sheep, aoe::Player::blue, {2, 2}
    );
    const aoe::EntityId boar = cadence.add_unit(
        aoe::UnitKind::boar, aoe::Player::red, {6, 2}
    );
    cadence.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {8, 5}
    );
    cadence.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {0, 5}
    );
    std::vector<aoe::Unit> carcasses = cadence.units();
    carcasses[0].hit_points = 0;
    carcasses[1].hit_points = 0;
    cadence.replace_state(
        std::move(carcasses),
        cadence.buildings(),
        cadence.economy(aoe::Player::blue),
        cadence.economy(aoe::Player::red),
        0
    );

    for (int tick = 0; tick < 12; ++tick) cadence.update();
    require(cadence.units()[0].hit_points == 0);
    require(!cadence.units()[0].moving);
    require(cadence.units()[0].destination == cadence.units()[0].position);
    require(cadence.units()[1].hit_points == 0);
    require(!cadence.units()[1].moving);
    require(cadence.units()[0].food_remaining == 100);
    require(cadence.units()[0].food_decay_remainder == 300);
    require(cadence.units()[1].food_remaining == 340);
    require(cadence.units()[1].food_decay_remainder == 480);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-carcass-decay.save";
    aoe::save_game(cadence, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    cadence.update();
    loaded.update();
    for (aoe::EntityId id : {sheep, boar}) {
        const auto first = std::ranges::find_if(
            cadence.units(), [id](const aoe::Unit& unit) {
                return unit.id == id;
            }
        );
        const auto second = std::ranges::find_if(
            loaded.units(), [id](const aoe::Unit& unit) {
                return unit.id == id;
            }
        );
        require(first != cadence.units().end());
        require(second != loaded.units().end());
        require(first->food_remaining == second->food_remaining);
        require(first->food_decay_remainder ==
                second->food_decay_remainder);
    }
    require(cadence.units()[1].food_remaining == 339);
    require(cadence.units()[1].food_decay_remainder == 20);

    aoe::Simulation competition(aoe::GameMap(12, 8));
    const aoe::EntityId first = competition.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 2}
    );
    const aoe::EntityId second = competition.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {8, 2}
    );
    const aoe::EntityId deer = competition.add_unit(
        aoe::UnitKind::deer, aoe::Player::red, {7, 2}
    );
    competition.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {0, 0}
    );
    competition.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 6}
    );
    std::vector<aoe::Unit> units = competition.units();
    units[2].hit_points = 0;
    units[2].food_remaining = 1;
    competition.replace_state(
        std::move(units),
        competition.buildings(),
        competition.economy(aoe::Player::blue),
        competition.economy(aoe::Player::red),
        0
    );
    require(competition.command_gather_unit(first, deer));
    require(competition.command_gather_unit(second, deer));
    competition.update();
    int gathered = 0;
    for (const aoe::Unit& unit : competition.units()) {
        if (unit.id == first || unit.id == second) {
            gathered += unit.carried_amount;
        }
    }
    require(gathered == 1);
    require(std::ranges::none_of(
        competition.units(),
        [deer](const aoe::Unit& unit) { return unit.id == deer; }
    ));
    for (int tick = 0; tick < 30; ++tick) competition.update();
    for (const aoe::EntityId id : {first, second}) {
        const auto worker = std::ranges::find_if(
            competition.units(), [id](const aoe::Unit& unit) {
                return unit.id == id;
            }
        );
        require(worker != competition.units().end());
        require(!worker->has_resource_target);
        require(!worker->moving);
    }

    aoe::Simulation spoiled(aoe::GameMap(9, 6));
    const aoe::EntityId archer = spoiled.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 2}
    );
    const aoe::EntityId prey = spoiled.add_unit(
        aoe::UnitKind::deer, aoe::Player::red, {4, 2}
    );
    spoiled.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {7, 4}
    );
    require(spoiled.command_unit(archer, {4, 2}));
    for (int tick = 0; tick < 20; ++tick) spoiled.update();
    require(std::ranges::none_of(
        spoiled.units(),
        [prey](const aoe::Unit& unit) { return unit.id == prey; }
    ));
}

void boar_retaliate_and_hunt_state_persists_deterministically() {
    const auto make_dangerous_hunt = [] {
        aoe::Simulation simulation(aoe::GameMap(16, 10));
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {3, 5}
        );
        simulation.add_unit(
            aoe::UnitKind::boar,
            aoe::Player::red,
            {7, 5}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {15, 9}
        );
        return simulation;
    };

    aoe::Simulation first = make_dangerous_hunt();
    aoe::Simulation second = make_dangerous_hunt();
    aoe::Replay replay;
    replay.record(0, aoe::MoveUnitCommand{1, {7, 5}});
    for (int tick = 0; tick < 12; ++tick) {
        replay.apply_current_tick(first);
        if (tick == 0) {
            require(aoe::execute(
                second,
                aoe::GameCommand{
                    aoe::MoveUnitCommand{1, {7, 5}}
                }
            ));
        }
        first.update();
        second.update();
    }
    const auto first_boar = std::ranges::find_if(
        first.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::boar;
        }
    );
    const auto second_boar = std::ranges::find_if(
        second.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::boar;
        }
    );
    require(first_boar != first.units().end());
    require(second_boar != second.units().end());
    require(first_boar->attack_target_id == 1);
    require(first_boar->position == second_boar->position);
    require(first_boar->hit_points == second_boar->hit_points);
    require(first_boar->food_remaining == 340);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-boar-hunt.save";
    aoe::save_game(first, save_path);
    const aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    const auto loaded_boar = std::ranges::find_if(
        loaded.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::boar;
        }
    );
    require(loaded_boar != loaded.units().end());
    require(loaded_boar->attack_target_id == first_boar->attack_target_id);
    require(loaded_boar->food_remaining == 340);

    for (int tick = 0; tick < 100; ++tick) {
        first.update();
    }
    require(std::ranges::none_of(
        first.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::villager &&
                unit.owner == aoe::Player::blue;
        }
    ));

    aoe::Scenario scenario(10, 6);
    aoe::UnitPlacement deer{
        aoe::UnitKind::deer,
        aoe::Player::red,
        {4, 2},
    };
    deer.food_remaining = 75;
    scenario.units.push_back(deer);
    aoe::UnitPlacement boar{
        aoe::UnitKind::boar,
        aoe::Player::red,
        {6, 3},
    };
    boar.food_remaining = 222;
    scenario.units.push_back(boar);
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-hunt.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded_scenario =
        aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded_scenario.units.size() == 2);
    require(loaded_scenario.units[0].kind == aoe::UnitKind::deer);
    require(loaded_scenario.units[0].food_remaining == 75);
    require(loaded_scenario.units[1].kind == aoe::UnitKind::boar);
    require(loaded_scenario.units[1].food_remaining == 222);
}

void playthrough_orders_support_remote_building_live_hunting_and_terminal_settle() {
    aoe::Simulation building = aoe::Simulation::create_demo();
    const aoe::EntityId builder = building.units().front().id;
    require(building.construct_building_at(
        builder, aoe::BuildingKind::house, {8, 7}
    ));
    require(building.buildings().back().kind == aoe::BuildingKind::house);
    require(building.units().front().moving);

    aoe::Simulation hunt(aoe::GameMap(16, 10));
    const aoe::EntityId hunter = hunt.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 4}
    );
    const aoe::EntityId deer = hunt.add_unit(
        aoe::UnitKind::deer, aoe::Player::red, {6, 4}
    );
    hunt.add_unit(aoe::UnitKind::villager, aoe::Player::red, {15, 9});
    require(hunt.command_gather_unit(hunter, deer));
    require(hunt.units()[0].resource_unit_id == deer);
    bool carcass_reached = false;
    for (int tick = 0; tick < 100; ++tick) {
        hunt.update();
        const auto target = std::ranges::find(
            hunt.units(), deer, &aoe::Unit::id
        );
        if (target != hunt.units().end() && target->hit_points == 0) {
            carcass_reached = true;
            break;
        }
    }
    require(carcass_reached);
    require(hunt.units().front().has_resource_target);

    aoe::Simulation terminal(aoe::GameMap(12, 8));
    const aoe::EntityId survivor = terminal.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    const aoe::EntityId defeated = terminal.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 6}
    );
    require(terminal.command_unit(survivor, {8, 5}));
    require(terminal.units().front().moving);
    require(terminal.delete_unit(defeated));
    require(terminal.outcome() == aoe::MatchOutcome::blue_victory);
    require(!terminal.units().front().moving);
    require(terminal.units().front().destination ==
        terminal.units().front().position);
    require(terminal.units().front().attack_target_id == 0);
    require(!terminal.units().front().has_resource_target);
}

void monks_convert_units_with_persisted_replayable_progress() {
    require(aoe::rules_for(aoe::BuildingKind::monastery).wood_cost == 175);
    require(
        aoe::rules_for(aoe::BuildingKind::monastery).minimum_age ==
        aoe::Age::castle
    );
    require(aoe::rules_for(aoe::UnitKind::monk).gold_cost == 100);
    require(
        aoe::rules_for(aoe::UnitKind::monk).trained_at ==
        aoe::BuildingKind::monastery
    );

    aoe::Scenario production_scenario(14, 8);
    production_scenario.blue_economy = {500, 500, 500, 500};
    production_scenario.red_economy = {500, 500, 500, 500};
    production_scenario.blue_age = aoe::Age::castle;
    production_scenario.red_age = aoe::Age::castle;
    production_scenario.units.push_back({
        aoe::UnitKind::villager, aoe::Player::red, {13, 7}
    });
    production_scenario.buildings.push_back({
        aoe::BuildingKind::monastery, aoe::Player::blue, {0, 0}
    });
    production_scenario.buildings.push_back({
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    });
    aoe::Simulation production =
        aoe::create_simulation(production_scenario);
    const aoe::EntityId monastery = production.buildings().front().id;
    require(aoe::can_train(
        aoe::BuildingKind::monastery,
        aoe::UnitKind::monk
    ));
    require(production.population_capacity(aoe::Player::blue) == 5);
    require(production.economy(aoe::Player::blue).gold == 500);
    require(production.age(aoe::Player::blue) == aoe::Age::castle);
    require(production.queue_unit_at(monastery, aoe::UnitKind::monk));
    for (int tick = 0; tick < 12; ++tick) {
        production.update();
    }
    require(std::ranges::any_of(
        production.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::monk &&
                unit.owner == aoe::Player::blue;
        }
    ));

    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(16, 10));
        simulation.add_unit(
            aoe::UnitKind::monk,
            aoe::Player::blue,
            {4, 4}
        );
        simulation.add_unit(
            aoe::UnitKind::knight,
            aoe::Player::red,
            {10, 4}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {15, 9}
        );
        simulation.add_unit(
            aoe::UnitKind::deer,
            aoe::Player::red,
            {6, 6}
        );
        return simulation;
    };
    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    require(!first.command_convert(1, 1));
    require(!first.command_convert(1, 4));

    aoe::Replay replay;
    replay.record(0, aoe::ConvertUnitCommand{1, 2});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-monk.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    for (int tick = 0; tick < 9; ++tick) {
        replay.apply_current_tick(first);
        loaded_replay.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units()[1].owner == aoe::Player::red);
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-monk.save";
    aoe::save_game(first, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.units()[0].conversion_target_id == 2);
    require(restored.units()[0].conversion_progress == 9);
    first.update();
    second.update();
    restored.update();
    require(first.units()[1].owner == aoe::Player::blue);
    require(second.units()[1].owner == aoe::Player::blue);
    require(restored.units()[1].owner == aoe::Player::blue);
    require(first.units()[0].conversion_cooldown == 20);
    require(!first.command_convert(1, 3));
}

void monks_heal_and_bank_neutral_relics_deterministically() {
    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(16, 10));
        simulation.add_building(
            aoe::BuildingKind::monastery,
            aoe::Player::blue,
            {1, 1}
        );
        simulation.add_unit(
            aoe::UnitKind::monk,
            aoe::Player::blue,
            {4, 3}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {4, 4}
        );
        simulation.add_unit(
            aoe::UnitKind::relic,
            aoe::Player::neutral,
            {5, 3}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {15, 9}
        );
        std::vector<aoe::Unit> units = simulation.units();
        units[1].hit_points = 20;
        simulation.replace_state(
            std::move(units),
            simulation.buildings(),
            {0, 0, 100, 0},
            {0, 0, 0, 0},
            0
        );
        return simulation;
    };

    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    require(aoe::is_relic(aoe::UnitKind::relic));
    require(!aoe::is_organic(aoe::UnitKind::relic));
    require(first.population(aoe::Player::blue) == 2);
    require(!first.select_unit_at({5, 3}, aoe::Player::blue));
    require(!first.command_unit(2, {5, 3}));
    require(first.command_heal(2, 3));
    for (int tick = 0; tick < 5; ++tick) {
        first.update();
    }
    require(first.units()[1].hit_points == 25);
    require(first.units()[0].healing_target_id == 0);

    aoe::Replay replay;
    replay.record(5, aoe::CollectRelicCommand{2, 4});
    replay.record(5, aoe::DepositRelicCommand{2, 1});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-relic.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);

    require(second.command_heal(2, 3));
    for (int tick = 0; tick < 5; ++tick) {
        second.update();
    }
    replay.apply_current_tick(first);
    loaded_replay.apply_current_tick(second);
    require(first.units().size() == 3);
    require(second.units().size() == 3);
    require(first.buildings()[0].relic_count == 1);
    require(second.buildings()[0].relic_count == 1);
    require(!first.units()[0].carrying_relic);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-relic.save";
    aoe::save_game(first, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.buildings()[0].relic_count == 1);
    require(restored.units()[0].healing_target_id == 0);
    for (int tick = 0; tick < 5; ++tick) {
        first.update();
        second.update();
        restored.update();
    }
    require(first.economy(aoe::Player::blue).gold == 101);
    require(second.economy(aoe::Player::blue).gold == 101);
    require(restored.economy(aoe::Player::blue).gold == 101);

    aoe::Scenario scenario(8, 6);
    scenario.units.push_back({
        aoe::UnitKind::relic, aoe::Player::neutral, {4, 3}
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-relic.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded_scenario =
        aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded_scenario.units[0].kind == aoe::UnitKind::relic);
    require(loaded_scenario.units[0].owner == aoe::Player::neutral);
}

void markets_exchange_resources_at_shared_dynamic_prices() {
    require(aoe::rules_for(aoe::BuildingKind::market).wood_cost == 175);
    require(
        aoe::rules_for(aoe::BuildingKind::market).minimum_age ==
        aoe::Age::feudal
    );

    aoe::Simulation construction(aoe::GameMap(16, 10));
    const aoe::EntityId builder = construction.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {4, 6}
    );
    construction.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 7}
    );
    construction.replace_state(
        construction.units(),
        construction.buildings(),
        {500, 500, 500, 500},
        {500, 500, 500, 500},
        0
    );
    require(!construction.construct_building_at(
        builder, aoe::BuildingKind::market, {5, 5}
    ));
    construction.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(construction.construct_building_at(
        builder, aoe::BuildingKind::market, {5, 5}
    ));
    require(!construction.buy_resource(
        aoe::Player::blue, aoe::MarketResource::food
    ));

    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(16, 10));
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::blue, {0, 0}
        );
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::red, {12, 6}
        );
        simulation.replace_state(
            simulation.units(),
            simulation.buildings(),
            {200, 200, 500, 50},
            {100, 200, 500, 50},
            0
        );
        return simulation;
    };
    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    require(first.market_buy_price(aoe::MarketResource::food) == 130);
    require(first.market_sell_price(aoe::MarketResource::food) == 70);

    aoe::Replay replay;
    replay.record(0, aoe::SellResourceCommand{
        aoe::Player::blue, aoe::MarketResource::food
    });
    replay.record(0, aoe::BuyResourceCommand{
        aoe::Player::red, aoe::MarketResource::food
    });
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-market.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    replay.apply_current_tick(first);
    loaded_replay.apply_current_tick(second);

    require(first.economy(aoe::Player::blue).food == 100);
    require(first.economy(aoe::Player::blue).gold == 570);
    require(first.economy(aoe::Player::red).food == 300);
    require(first.economy(aoe::Player::red).gold == 374);
    require(first.market_base_price(aoe::MarketResource::food) == 100);
    require(first.economy(aoe::Player::blue).gold ==
            second.economy(aoe::Player::blue).gold);

    require(!first.sell_resource(
        aoe::Player::blue, aoe::MarketResource::stone
    ));
    require(!first.buy_resource(
        aoe::Player::neutral, aoe::MarketResource::wood
    ));
    const auto invalid_market_resource =
        static_cast<aoe::MarketResource>(99);
    const int stone_before =
        first.economy(aoe::Player::blue).stone;
    require(!first.buy_resource(
        aoe::Player::blue, invalid_market_resource
    ));
    require(!first.sell_resource(
        aoe::Player::blue, invalid_market_resource
    ));
    require(
        first.economy(aoe::Player::blue).stone == stone_before
    );
    bool invalid_price_rejected = false;
    try {
        (void)first.market_base_price(invalid_market_resource);
    } catch (const std::invalid_argument&) {
        invalid_price_rejected = true;
    }
    require(invalid_price_rejected);

    require(first.sell_resource(
        aoe::Player::blue, aoe::MarketResource::wood
    ));
    require(first.market_base_price(aoe::MarketResource::wood) == 97);
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-market.save";
    aoe::save_game(first, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.market_base_price(aoe::MarketResource::wood) == 97);
    require(restored.buildings()[0].kind == aoe::BuildingKind::market);
    restored.replace_market_prices(100, 20, 100);
    require(restored.sell_resource(
        aoe::Player::blue, aoe::MarketResource::wood
    ));
    require(restored.market_base_price(aoe::MarketResource::wood) == 20);

    aoe::Scenario scenario(8, 6);
    scenario.buildings.push_back({
        aoe::BuildingKind::market, aoe::Player::blue, {1, 1}
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-market.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.buildings[0].kind == aoe::BuildingKind::market);
}

void diplomacy_and_allied_trade_are_deterministic() {
    require(
        aoe::rules_for(aoe::UnitKind::trade_cart).trained_at ==
        aoe::BuildingKind::market
    );
    require(aoe::rules_for(aoe::UnitKind::trade_cart).wood_cost == 100);
    require(aoe::rules_for(aoe::UnitKind::trade_cart).gold_cost == 50);
    const auto make_simulation = [] {
        aoe::Simulation simulation(aoe::GameMap(18, 10));
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::blue, {0, 4}
        );
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::red, {14, 4}
        );
        simulation.add_unit(
            aoe::UnitKind::trade_cart, aoe::Player::blue, {4, 5}
        );
        simulation.replace_state(
            simulation.units(), simulation.buildings(),
            {500, 500, 100, 500}, {500, 500, 100, 500}, 0
        );
        return simulation;
    };
    aoe::Simulation enemy = make_simulation();
    require(enemy.diplomacy(aoe::Player::blue, aoe::Player::red) ==
            aoe::Diplomacy::enemy);
    require(!enemy.command_trade_route(3, 2));

    aoe::Simulation first = make_simulation();
    aoe::Simulation second = make_simulation();
    aoe::Replay replay;
    replay.record(0, aoe::SetDiplomacyCommand{
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    });
    replay.record(0, aoe::TradeRouteCommand{3, 2});
    const auto path =
        std::filesystem::temp_directory_path() / "aoe-trade.replay";
    aoe::save_replay(replay, path);
    aoe::Replay loaded_replay = aoe::load_replay(path);
    std::filesystem::remove(path);
    replay.apply_current_tick(first);
    loaded_replay.apply_current_tick(second);
    require(first.is_ally(aoe::Player::blue, aoe::Player::red));
    require(!first.command_convert(3, 3));
    for (int tick = 0; tick < 120; ++tick) {
        first.update();
        second.update();
    }
    require(first.economy(aoe::Player::blue).gold > 100);
    require(first.economy(aoe::Player::blue).gold ==
            second.economy(aoe::Player::blue).gold);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-trade.save";
    aoe::save_game(first, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.is_ally(aoe::Player::blue, aoe::Player::red));
    require(restored.units()[0].trade_target_market_id == 2);
}

void civilization_bonuses_are_scoped_and_persisted() {
    aoe::Simulation simulation(aoe::GameMap(20, 12));
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::franks
    ));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::castle);
    const auto blue_archer = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 2}
    );
    const auto red_archer = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {17, 9}
    );
    const auto red_knight = simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {16, 9}
    );
    require(simulation.effective_attack_range(
        simulation.units()[blue_archer - 1]
    ) == aoe::rules_for(aoe::UnitKind::archer).attack_range + 1);
    require(simulation.effective_attack_range(
        simulation.units()[red_archer - 1]
    ) == aoe::rules_for(aoe::UnitKind::archer).attack_range);
    require(simulation.maximum_hit_points(
        simulation.units()[red_knight - 1]
    ) == 120);
    require(simulation.maximum_hit_points(
        simulation.units()[blue_archer - 1]
    ) == aoe::rules_for(aoe::UnitKind::archer).hit_points);

    aoe::Replay replay;
    replay.record(0, aoe::SetCivilizationCommand{
        aoe::Player::blue, aoe::Civilization::teutons
    });
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-civ.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(loaded_replay.commands().size() == 1);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-civ.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.civilization(aoe::Player::blue) ==
            aoe::Civilization::britons);
    require(restored.civilization(aoe::Player::red) ==
            aoe::Civilization::franks);
    require(restored.maximum_hit_points(restored.units()[2]) == 120);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::teutons;
    scenario.red_civilization = aoe::Civilization::britons;
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-civ.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.blue_civilization == aoe::Civilization::teutons);
    require(loaded.red_civilization == aoe::Civilization::britons);
}

void additional_civilization_bonuses_use_existing_systems() {
    aoe::Simulation simulation(aoe::GameMap(20, 12));
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::vikings
    ));
    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::byzantines
    ));
    simulation.replace_ages(aoe::Age::castle, aoe::Age::imperial);
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {2, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 8}
    );
    require(simulation.maximum_hit_points(simulation.units()[0]) ==
            aoe::rules_for(aoe::UnitKind::militia).hit_points * 120 / 100);
    require(simulation.maximum_hit_points(simulation.buildings()[0]) ==
            aoe::rules_for(aoe::BuildingKind::house).hit_points * 140 / 100);

    aoe::Simulation celts(aoe::GameMap(12, 8));
    require(celts.set_civilization(
        aoe::Player::blue, aoe::Civilization::celts
    ));
    celts.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {2, 2}
    );
    require(celts.effective_attack_interval(celts.units()[0]) ==
            std::max(
                1,
                aoe::rules_for(aoe::UnitKind::mangonel)
                    .attack_interval_ticks * 3 / 4
            ));

    aoe::Simulation goths(aoe::GameMap(14, 8));
    goths.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {0, 0}
    );
    goths.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    );
    goths.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    goths.replace_state(
        goths.units(), goths.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(goths.set_civilization(
        aoe::Player::blue, aoe::Civilization::goths
    ));
    goths.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(goths.population_capacity(aoe::Player::blue) == 15);
    const int food_before = goths.economy(aoe::Player::blue).food;
    require(goths.queue_unit_at(1, aoe::UnitKind::militia));
    require(food_before - goths.economy(aoe::Player::blue).food ==
            aoe::rules_for(aoe::UnitKind::militia).food_cost * 65 / 100);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-extra-civs.save";
    aoe::save_game(simulation, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    require(restored.civilization(aoe::Player::blue) ==
            aoe::Civilization::vikings);
    require(restored.civilization(aoe::Player::red) ==
            aoe::Civilization::byzantines);
}

void asian_and_saracen_bonuses_are_exact_and_isolated() {
    aoe::Simulation simulation(aoe::GameMap(18, 10));
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::japanese
    ));
    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::persians
    ));
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::feudal);
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {15, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {12, 3}
    );
    require(simulation.effective_attack_interval(simulation.units()[0]) ==
            std::max(
                1,
                aoe::rules_for(aoe::UnitKind::militia)
                    .attack_interval_ticks * 3 / 4
            ));
    require(simulation.effective_attack_interval(simulation.units()[1]) ==
            aoe::rules_for(aoe::UnitKind::militia).attack_interval_ticks);
    require(simulation.maximum_hit_points(simulation.buildings()[0]) ==
            aoe::rules_for(aoe::BuildingKind::town_center).hit_points * 2);

    aoe::Simulation chinese(aoe::GameMap(14, 8));
    chinese.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    chinese.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    chinese.replace_state(
        chinese.units(), chinese.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(chinese.set_civilization(
        aoe::Player::blue, aoe::Civilization::chinese
    ));
    chinese.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const auto& technology = aoe::rules_for(aoe::Technology::fletching);
    const int food_before = chinese.economy(aoe::Player::blue).food;
    const int gold_before = chinese.economy(aoe::Player::blue).gold;
    require(chinese.research_technology_at(
        1, aoe::Technology::fletching
    ));
    require(food_before - chinese.economy(aoe::Player::blue).food ==
            technology.food_cost * 85 / 100);
    require(gold_before - chinese.economy(aoe::Player::blue).gold ==
            technology.gold_cost * 85 / 100);

    aoe::Simulation saracens(aoe::GameMap(14, 8));
    saracens.add_building(
        aoe::BuildingKind::market, aoe::Player::blue, {0, 0}
    );
    saracens.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    saracens.replace_state(
        saracens.units(), saracens.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(saracens.set_civilization(
        aoe::Player::blue, aoe::Civilization::saracens
    ));
    require(saracens.market_buy_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 105);
    require(saracens.market_sell_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 95);
    require(saracens.market_buy_price(
        aoe::Player::red, aoe::MarketResource::food
    ) == 130);
    require(saracens.buy_resource(
        aoe::Player::blue, aoe::MarketResource::food
    ));
    require(saracens.economy(aoe::Player::blue).gold == 395);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-asian-civs.save";
    aoe::save_game(simulation, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    require(restored.civilization(aoe::Player::blue) ==
            aoe::Civilization::japanese);
    require(restored.civilization(aoe::Player::red) ==
            aoe::Civilization::persians);
}

void final_civilization_bonuses_use_supported_systems() {
    aoe::Simulation simulation(aoe::GameMap(18, 10));
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::turks
    ));
    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::mongols
    ));
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {15, 8}
    );
    require(simulation.pierce_armor(simulation.units()[0]) ==
            aoe::rules_for(aoe::UnitKind::scout_cavalry).pierce_armor + 1);
    require(simulation.maximum_hit_points(simulation.units()[1]) ==
            aoe::rules_for(aoe::UnitKind::scout_cavalry).hit_points *
                130 / 100);

    aoe::Simulation huns(aoe::GameMap(14, 8));
    huns.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {0, 0}
    );
    huns.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    huns.replace_state(
        huns.units(), huns.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(huns.set_civilization(
        aoe::Player::blue, aoe::Civilization::huns
    ));
    huns.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(huns.population_capacity(aoe::Player::blue) == 200);
    require(huns.queue_unit_at(1, aoe::UnitKind::scout_cavalry));
    require(huns.buildings()[0].production_queue[0].ticks_remaining ==
            aoe::rules_for(aoe::UnitKind::scout_cavalry).training_ticks *
                5 / 6);

    aoe::Simulation spanish(aoe::GameMap(14, 8));
    spanish.add_building(
        aoe::BuildingKind::blacksmith, aoe::Player::blue, {0, 0}
    );
    spanish.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    spanish.replace_state(
        spanish.units(), spanish.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(spanish.set_civilization(
        aoe::Player::blue, aoe::Civilization::spanish
    ));
    spanish.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    const int gold_before = spanish.economy(aoe::Player::blue).gold;
    require(spanish.research_technology_at(
        1, aoe::Technology::fletching
    ));
    require(spanish.economy(aoe::Player::blue).gold == gold_before);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-final-civs.save";
    aoe::save_game(simulation, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    require(restored.civilization(aoe::Player::blue) ==
            aoe::Civilization::turks);
    require(restored.civilization(aoe::Player::red) ==
            aoe::Civilization::mongols);
}

void conquerors_civilizations_use_supported_exact_bonuses() {
    aoe::Simulation koreans(aoe::GameMap(16, 10));
    require(koreans.set_civilization(
        aoe::Player::blue, aoe::Civilization::koreans
    ));
    koreans.replace_ages(aoe::Age::castle, aoe::Age::dark);
    koreans.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    koreans.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {5, 2}
    );
    koreans.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 6}
    );
    require(koreans.effective_unit_vision_range(koreans.units()[0]) ==
            aoe::rules_for(aoe::UnitKind::villager).vision_range + 3);
    require(koreans.has_technology(
        aoe::Player::blue, aoe::Technology::guard_tower
    ));
    require(koreans.buildings()[0].kind == aoe::BuildingKind::guard_tower);

    aoe::Simulation aztecs(aoe::GameMap(14, 8));
    aztecs.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {0, 0}
    );
    aztecs.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    );
    aztecs.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    aztecs.replace_state(
        aztecs.units(), aztecs.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(aztecs.set_civilization(
        aoe::Player::blue, aoe::Civilization::aztecs
    ));
    require(aztecs.queue_unit_at(1, aoe::UnitKind::militia));
    require(aztecs.buildings()[0].production_queue[0].ticks_remaining ==
            aoe::rules_for(aoe::UnitKind::militia).training_ticks *
                100 / 111);

    aoe::Simulation mayans(aoe::GameMap(14, 8));
    mayans.add_building(
        aoe::BuildingKind::archery_range, aoe::Player::blue, {0, 0}
    );
    mayans.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    );
    mayans.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    mayans.replace_state(
        mayans.units(), mayans.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(mayans.set_civilization(
        aoe::Player::blue, aoe::Civilization::mayans
    ));
    mayans.replace_ages(aoe::Age::castle, aoe::Age::dark);
    const int wood_before = mayans.economy(aoe::Player::blue).wood;
    require(mayans.queue_unit_at(1, aoe::UnitKind::archer));
    require(wood_before - mayans.economy(aoe::Player::blue).wood ==
            aoe::rules_for(aoe::UnitKind::archer).wood_cost * 80 / 100);

    const auto path =
        std::filesystem::temp_directory_path() / "aoe-conquerors.save";
    aoe::save_game(koreans, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    require(restored.civilization(aoe::Player::blue) ==
            aoe::Civilization::koreans);
}

void represented_team_los_bonuses_follow_alliance_without_stacking() {
    aoe::GameMap map(20, 10);
    map.set_terrain({3, 3}, aoe::Terrain::water);
    aoe::Simulation simulation(std::move(map));
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::byzantines
    ));
    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::franks
    ));
    simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {2, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {15, 6}
    );
    const int knight_los =
        aoe::rules_for(aoe::UnitKind::knight).vision_range;
    require(
        simulation.effective_unit_vision_range(simulation.units().front()) ==
        knight_los
    );
    require(simulation.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    require(
        simulation.effective_unit_vision_range(simulation.units().front()) ==
        knight_los + 2
    );
    require(simulation.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::enemy
    ));
    require(
        simulation.effective_unit_vision_range(simulation.units().front()) ==
        knight_los
    );

    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::japanese
    ));
    require(simulation.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    simulation.add_unit(
        aoe::UnitKind::galley, aoe::Player::blue, {3, 3}
    );
    require(
        simulation.effective_unit_vision_range(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::galley).vision_range * 3 / 2
    );

    require(simulation.set_civilization(
        aoe::Player::red, aoe::Civilization::mongols
    ));
    simulation.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {4, 4}
    );
    require(
        simulation.effective_unit_vision_range(simulation.units().back()) ==
        aoe::rules_for(aoe::UnitKind::scout_cavalry).vision_range + 2
    );

    const int allied_training_ticks =
        (aoe::rules_for(aoe::UnitKind::archer).training_ticks * 100 + 119) /
        120;
    const auto trained_archers = [allied_training_ticks](
        aoe::Diplomacy relation
    ) {
        aoe::Simulation production(aoe::GameMap(16, 8));
        production.set_civilization(
            aoe::Player::blue, aoe::Civilization::byzantines
        );
        production.set_civilization(
            aoe::Player::red, aoe::Civilization::britons
        );
        production.set_diplomacy(
            aoe::Player::blue, aoe::Player::red, relation
        );
        const auto range = production.add_building(
            aoe::BuildingKind::archery_range, aoe::Player::blue, {1, 1}
        );
        production.add_building(
            aoe::BuildingKind::house, aoe::Player::blue, {12, 4}
        );
        production.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {14, 4}
        );
        production.replace_ages(aoe::Age::feudal, aoe::Age::dark);
        production.replace_state(
            production.units(), production.buildings(),
            {500, 500, 500, 500}, {500, 500, 500, 500}, 0
        );
        require(production.queue_unit_at(range, aoe::UnitKind::archer));
        for (int tick = 1; tick < allied_training_ticks; ++tick) {
            production.update();
        }
        require(production.units().empty());
        production.update();
        return production.units().size();
    };
    require(trained_archers(aoe::Diplomacy::ally) == 1);
    require(trained_archers(aoe::Diplomacy::enemy) == 0);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-team-los-bonuses.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(
        loaded.effective_unit_vision_range(loaded.units().back()) ==
        aoe::rules_for(aoe::UnitKind::scout_cavalry).vision_range + 2
    );
}

void newly_represented_team_bonuses_follow_reciprocal_alliance() {
    aoe::Simulation farms(aoe::GameMap(12, 8));
    require(farms.set_civilization(
        aoe::Player::red, aoe::Civilization::chinese
    ));
    require(farms.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    farms.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {2, 2}
    );
    require(farms.buildings().back().resource_amount == 220);

    aoe::Simulation construction(aoe::GameMap(16, 8));
    require(construction.set_civilization(
        aoe::Player::red, aoe::Civilization::mayans
    ));
    require(construction.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    const auto builder = construction.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    construction.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 4}
    );
    construction.replace_state(
        construction.units(), construction.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    const int wood_before =
        construction.economy(aoe::Player::blue).wood;
    require(construction.construct_building_at(
        builder, aoe::BuildingKind::palisade_wall, {3, 2}
    ));
    require(
        wood_before - construction.economy(aoe::Player::blue).wood ==
        aoe::rules_for(aoe::BuildingKind::palisade_wall).wood_cost / 2
    );

    aoe::Simulation healing(aoe::GameMap(12, 8));
    require(healing.set_civilization(
        aoe::Player::red, aoe::Civilization::byzantines
    ));
    require(healing.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    const auto monk = healing.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 2}
    );
    const auto patient = healing.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {3, 2}
    );
    healing.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    auto damaged_units = healing.units();
    damaged_units[1].hit_points -= 10;
    healing.replace_state(
        std::move(damaged_units), healing.buildings(),
        healing.economy(aoe::Player::blue),
        healing.economy(aoe::Player::red), 0
    );
    const int hit_points = healing.units()[1].hit_points;
    require(healing.command_heal(monk, patient));
    healing.update();
    healing.update();
    require(healing.units()[1].hit_points == hit_points + 3);

    aoe::Simulation relics(aoe::GameMap(12, 8));
    require(relics.set_civilization(
        aoe::Player::red, aoe::Civilization::aztecs
    ));
    require(relics.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    relics.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {2, 2}
    );
    relics.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 4}
    );
    auto relic_buildings = relics.buildings();
    relic_buildings[0].relic_count = 1;
    relics.replace_state(
        relics.units(), std::move(relic_buildings),
        {}, {}, 0
    );
    for (int tick = 0; tick < 10; ++tick) relics.update();
    require(relics.economy(aoe::Player::blue).gold == 1);
    require(
        relics.aztec_relic_gold_remainder(aoe::Player::blue) == 33
    );
    const auto relic_path = std::filesystem::temp_directory_path() /
        "aoe-aztec-team-relic-remainder-v108.save";
    aoe::save_game(relics, relic_path);
    aoe::Simulation loaded_relics = aoe::load_game(relic_path);
    std::filesystem::remove(relic_path);
    require(
        loaded_relics.aztec_relic_gold_remainder(
            aoe::Player::blue
        ) == 33
    );
    for (int tick = 0; tick < 30; ++tick) loaded_relics.update();
    require(loaded_relics.economy(aoe::Player::blue).gold == 5);
    require(
        loaded_relics.aztec_relic_gold_remainder(
            aoe::Player::blue
        ) == 32
    );
}

void dock_shoreline_and_ship_spawn_geometry_match_original() {
    aoe::GameMap map(12, 9);
    prepare_dock_foundation(map, {2, 2});
    map.set_terrain({3, 1}, aoe::Terrain::ice2);
    for (int y = 5; y < map.height(); ++y) {
        for (int x = 2; x < map.width(); ++x) {
            map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    require(map.supports_dock_foundation({2, 2}));
    require(!map.supports_dock_foundation({6, 2}));

    aoe::Simulation simulation(std::move(map));
    const auto dock = simulation.add_building(
        aoe::BuildingKind::dock, aoe::Player::blue, {2, 2}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {7, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 0}
    );
    require(simulation.set_rally_point(dock, {10, 6}));
    require(simulation.queue_unit_at(
        dock, aoe::UnitKind::fishing_ship
    ));
    for (int tick = 0; tick < 12; ++tick) simulation.update();
    require(simulation.units().size() == 1);
    const aoe::Unit& ship = simulation.units().front();
    require(ship.kind == aoe::UnitKind::fishing_ship);
    require(ship.position == aoe::TilePosition{3, 5});
    require(simulation.map().sailable(ship.position));
    require(ship.destination == aoe::TilePosition{10, 6});
    require(ship.moving);

    aoe::GameMap grass_anchor(8, 8);
    grass_anchor.set_terrain({5, 3}, aoe::Terrain::water);
    aoe::Simulation rejected(std::move(grass_anchor));
    const auto builder = rejected.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    require(!rejected.construct_building_at(
        builder, aoe::BuildingKind::dock, {2, 2}
    ));
}

void docks_and_fishing_ships_form_a_water_only_food_economy() {
    require(aoe::rules_for(aoe::BuildingKind::dock).wood_cost == 150);
    require(aoe::rules_for(aoe::BuildingKind::dock).minimum_age ==
            aoe::Age::dark);
    require(aoe::rules_for(aoe::UnitKind::fishing_ship).wood_cost == 75);
    require(aoe::rules_for(aoe::UnitKind::fishing_ship).trained_at ==
            aoe::BuildingKind::dock);

    aoe::GameMap map(14, 8);
    for (int y = 4; y < 8; ++y) {
        for (int x = 0; x < 14; ++x) {
            map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    prepare_dock_foundation(map, {1, 1});
    map.set_terrain({8, 4}, aoe::Terrain::fish);
    aoe::Simulation simulation(std::move(map));
    const auto dock = simulation.add_building(
        aoe::BuildingKind::dock, aoe::Player::blue, {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {5, 0}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 0}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {500, 100, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(simulation.queue_unit_at(
        dock, aoe::UnitKind::fishing_ship
    ));
    for (int tick = 0; tick < 12; ++tick) {
        simulation.update();
    }
    require(simulation.units().size() == 1);
    const auto ship = simulation.units()[0].id;
    require(simulation.units()[0].kind == aoe::UnitKind::fishing_ship);
    require(simulation.command_unit(ship, {6, 4}));
    for (int tick = 0; tick < 4; ++tick) {
        simulation.update();
    }
    require(simulation.units()[0].position.y == 4);
    require(!simulation.command_unit(ship, {6, 3}));
    require(simulation.command_unit(ship, {8, 4}));
    for (int tick = 0;
         tick < 200 &&
         simulation.economy(aoe::Player::blue).food == 100;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).food >= 115);
    require(simulation.map().resource_amount_at({8, 4}) <= 185);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-fishing.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.buildings()[0].kind == aoe::BuildingKind::dock);
    require(restored.units()[0].kind == aoe::UnitKind::fishing_ship);
    require(restored.map().terrain_at({8, 4}) == aoe::Terrain::fish);

    aoe::Scenario scenario(10, 6);
    prepare_dock_foundation(scenario.map, {1, 1});
    scenario.map.set_terrain({4, 4}, aoe::Terrain::water);
    scenario.map.set_terrain({5, 4}, aoe::Terrain::fish);
    scenario.buildings.push_back({
        aoe::BuildingKind::dock, aoe::Player::blue, {1, 1}
    });
    scenario.units.push_back({
        aoe::UnitKind::fishing_ship, aoe::Player::blue, {4, 4}
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-fishing.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.buildings[0].kind == aoe::BuildingKind::dock);
    require(loaded.units[0].kind == aoe::UnitKind::fishing_ship);
    require(loaded.map.terrain_at({5, 4}) == aoe::Terrain::fish);
}

void galley_line_combat_and_transport_passengers_persist() {
    require(aoe::rules_for(aoe::UnitKind::galley).hit_points == 120);
    require(aoe::rules_for(aoe::UnitKind::galley).attack == 6);
    require(aoe::rules_for(aoe::UnitKind::galley).attack_range == 5);
    require(aoe::rules_for(aoe::UnitKind::transport_ship).wood_cost == 125);

    aoe::GameMap map(14, 8);
    for (int y = 1; y < 8; ++y) {
        for (int x = 0; x < 14; ++x) {
            map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    aoe::Simulation combat(std::move(map));
    const auto blue = combat.add_unit(
        aoe::UnitKind::galley, aoe::Player::blue, {2, 2}
    );
    combat.add_unit(
        aoe::UnitKind::galley, aoe::Player::red, {7, 2}
    );
    require(combat.command_unit(blue, {7, 2}));
    for (int tick = 0; tick < 5; ++tick) combat.update();
    require(combat.units()[1].hit_points < 120);
    require(!combat.projectiles().empty() ||
            !combat.impact_effects().empty());

    aoe::GameMap transport_map(10, 6);
    for (int y = 1; y < 6; ++y)
        for (int x = 0; x < 10; ++x)
            transport_map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation passengers(std::move(transport_map));
    const auto villager = passengers.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 0}
    );
    const auto ship = passengers.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::blue, {3, 1}
    );
    passengers.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 0}
    );
    require(passengers.command_embark(villager, ship));
    require(passengers.units()[0].garrisoned_in == ship);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-transport.save";
    aoe::save_game(passengers, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.units()[0].garrisoned_in == ship);
    require(restored.command_disembark(ship, {3, 0}));
    require(restored.units()[0].garrisoned_in == 0);
    require(restored.units()[0].position == aoe::TilePosition{3, 0});

    aoe::Replay replay;
    replay.record(0, aoe::EmbarkCommand{villager, ship});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-transport.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(loaded_replay.commands().size() == 1);
}

void transport_disembark_spreads_all_passengers_on_connected_land() {
    aoe::GameMap map(10, 6);
    for (int y = 1; y < 6; ++y)
        for (int x = 0; x < 10; ++x)
            map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation simulation(std::move(map));
    const auto first = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 0}
    );
    const auto ship = simulation.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::blue, {3, 1}
    );
    require(simulation.command_embark(first, ship));
    const auto second = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {3, 0}
    );
    require(simulation.command_embark(second, ship));
    const auto third = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {3, 0}
    );
    require(simulation.command_embark(third, ship));
    require(simulation.command_disembark(ship, {3, 0}));

    const auto unit_by_id = [&simulation](aoe::EntityId id) {
        return &*std::ranges::find(
            simulation.units(), id, &aoe::Unit::id
        );
    };
    const aoe::Unit* first_unit = unit_by_id(first);
    const aoe::Unit* second_unit = unit_by_id(second);
    const aoe::Unit* third_unit = unit_by_id(third);
    require(first_unit->garrisoned_in == 0);
    require(second_unit->garrisoned_in == 0);
    require(third_unit->garrisoned_in == 0);
    require(first_unit->position != second_unit->position);
    require(first_unit->position != third_unit->position);
    require(second_unit->position != third_unit->position);
    require(first_unit->position.y == 0);
    require(second_unit->position.y == 0);
    require(third_unit->position.y == 0);
}

void fire_and_demolition_ship_lines_are_dat_backed_and_persist() {
    const auto& fire = aoe::rules_for(aoe::UnitKind::fire_ship);
    const auto& fast = aoe::rules_for(aoe::UnitKind::fast_fire_ship);
    const auto& demolition =
        aoe::rules_for(aoe::UnitKind::demolition_ship);
    const auto& heavy =
        aoe::rules_for(aoe::UnitKind::heavy_demolition_ship);
    require(fire.hit_points == 100 && fire.attack == 2);
    require(fire.wood_cost == 75 && fire.gold_cost == 45);
    require(fire.vision_range == 5 && fire.pierce_armor == 6);
    require(fast.hit_points == 120 && fast.attack == 3);
    require(fast.vision_range == 6 && fast.pierce_armor == 8);
    require(demolition.hit_points == 50 && demolition.attack == 110);
    require(demolition.wood_cost == 70 && demolition.gold_cost == 50);
    require(demolition.vision_range == 6);
    require(heavy.hit_points == 60 && heavy.attack == 140);
    require(aoe::rules_for(
        aoe::Technology::fast_fire_ship
    ).wood_cost == 280);
    require(aoe::rules_for(
        aoe::Technology::heavy_demolition_ship
    ).wood_cost == 200);

    aoe::GameMap map(10, 7);
    for (int y = 1; y < 7; ++y)
        for (int x = 0; x < 10; ++x)
            map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation simulation(std::move(map));
    const auto attacker = simulation.add_unit(
        aoe::UnitKind::demolition_ship, aoe::Player::blue, {2, 2}
    );
    const auto friendly = simulation.add_unit(
        aoe::UnitKind::galley, aoe::Player::blue, {2, 3}
    );
    const auto enemy = simulation.add_unit(
        aoe::UnitKind::galley, aoe::Player::red, {3, 2}
    );
    require(simulation.command_unit(attacker, {3, 2}));
    simulation.update();
    const auto unit_with_id = [&simulation](aoe::EntityId id)
        -> const aoe::Unit* {
        for (const aoe::Unit& unit : simulation.units())
            if (unit.id == id) return &unit;
        return nullptr;
    };
    require(unit_with_id(attacker) == nullptr);
    require(unit_with_id(friendly)->hit_points < 120);
    require(unit_with_id(enemy)->hit_points < 120);

    aoe::GameMap production_map(10, 7);
    prepare_dock_foundation(production_map, {0, 1});
    aoe::Simulation production(std::move(production_map));
    production.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fast_fire_ship,
         aoe::Technology::heavy_demolition_ship}
    );
    const auto dock = production.add_building(
        aoe::BuildingKind::dock, aoe::Player::blue, {0, 1}
    );
    production.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    );
    production.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(production.buildings()[0].completed());
    require(production.age(aoe::Player::blue) == aoe::Age::imperial);
    require(production.population_capacity(aoe::Player::blue) > 0);
    require(production.has_technology(
        aoe::Player::blue, aoe::Technology::fast_fire_ship
    ));
    require(aoe::can_train(
        aoe::BuildingKind::dock, aoe::UnitKind::fast_fire_ship
    ));
    require(production.outcome() == aoe::MatchOutcome::ongoing);
    require(production.economy(aoe::Player::blue).wood >= 75);
    require(production.economy(aoe::Player::blue).gold >= 45);
    require(production.queue_unit_at(dock, aoe::UnitKind::fire_ship));
    require(production.buildings()[0].production_queue.back().kind ==
            aoe::UnitKind::fast_fire_ship);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-fire-demo.save";
    aoe::save_game(production, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::fast_fire_ship
    ));
    require(restored.buildings()[0].production_queue[0].kind ==
            aoe::UnitKind::fast_fire_ship);

    aoe::Scenario scenario(8, 6);
    scenario.map.set_terrain({2, 2}, aoe::Terrain::water);
    scenario.map.set_terrain({4, 2}, aoe::Terrain::water);
    scenario.units.push_back({
        aoe::UnitKind::fast_fire_ship, aoe::Player::blue, {2, 2}
    });
    scenario.units.push_back({
        aoe::UnitKind::heavy_demolition_ship, aoe::Player::red, {4, 2}
    });
    scenario.blue_technologies.push_back(
        aoe::Technology::fast_fire_ship
    );
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-fire-test.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.units[0].kind == aoe::UnitKind::fast_fire_ship);
    require(loaded.units[1].kind ==
            aoe::UnitKind::heavy_demolition_ship);
    require(loaded.blue_technologies[0] ==
            aoe::Technology::fast_fire_ship);

    const auto make_replay_simulation = [] {
        aoe::GameMap replay_map(8, 6);
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 8; ++x)
                replay_map.set_terrain({x, y}, aoe::Terrain::water);
        aoe::Simulation result(std::move(replay_map));
        result.add_unit(
            aoe::UnitKind::heavy_demolition_ship,
            aoe::Player::blue, {1, 2}
        );
        result.add_unit(
            aoe::UnitKind::galleon, aoe::Player::red, {4, 2}
        );
        return result;
    };
    aoe::Replay recorded;
    recorded.record(0, aoe::MoveUnitCommand{1, {4, 2}});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-fire-demo.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation first = make_replay_simulation();
    aoe::Simulation second = make_replay_simulation();
    for (int tick = 0; tick < 5; ++tick) {
        recorded.apply_current_tick(first);
        replayed.apply_current_tick(second);
        first.update();
        second.update();
    }
    require(first.units().size() == second.units().size());
    require(first.units()[0].kind == second.units()[0].kind);
    require(first.units()[0].hit_points == second.units()[0].hit_points);
}

void cannon_galleons_and_dock_technologies_follow_live_dat() {
    const auto& cannon = aoe::rules_for(aoe::UnitKind::cannon_galleon);
    const auto& elite =
        aoe::rules_for(aoe::UnitKind::elite_cannon_galleon);
    require(cannon.hit_points == 120 && cannon.attack == 35);
    require(cannon.attack_range == 13 &&
            cannon.minimum_attack_range == 3);
    require(cannon.attack_interval_ticks == 10 &&
            cannon.accuracy_percent == 50);
    require(cannon.bonus_vs_buildings == 200 &&
            cannon.bonus_vs_ships == 40);
    require(cannon.wood_cost == 200 && cannon.gold_cost == 150);
    require(elite.hit_points == 150 && elite.attack == 45);
    require(elite.attack_range == 15 &&
            elite.minimum_attack_range == 3);
    require(elite.bonus_vs_buildings == 275 &&
            elite.bonus_vs_ships == 40);
    require(aoe::rules_for(
        aoe::Technology::cannon_galleon
    ).wood_cost == 500);
    require(aoe::rules_for(
        aoe::Technology::cannon_galleon
    ).food_cost == 400);
    require(aoe::rules_for(
        aoe::Technology::elite_cannon_galleon
    ).wood_cost == 525);
    require(aoe::rules_for(aoe::Technology::careening).food_cost == 250);
    require(aoe::rules_for(aoe::Technology::dry_dock).gold_cost == 400);
    require(aoe::rules_for(aoe::Technology::shipwright).food_cost == 1000);

    aoe::GameMap water(20, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 20; ++x)
            water.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation combat(std::move(water));
    const auto gun = combat.add_unit(
        aoe::UnitKind::cannon_galleon, aoe::Player::blue, {2, 3}
    );
    combat.add_unit(
        aoe::UnitKind::galleon, aoe::Player::red, {12, 3}
    );
    require(combat.command_unit(gun, {12, 3}));
    combat.update();
    require(combat.projectiles().size() == 1);
    require(combat.projectiles()[0].damage == 75);
    require(combat.projectiles()[0].target == 2);

    aoe::Replay recorded;
    recorded.record(0, aoe::MoveUnitCommand{gun, {12, 3}});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-cannon.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::GameMap replay_water(20, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 20; ++x)
            replay_water.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation replayed(std::move(replay_water));
    replayed.add_unit(
        aoe::UnitKind::cannon_galleon, aoe::Player::blue, {2, 3}
    );
    replayed.add_unit(
        aoe::UnitKind::galleon, aoe::Player::red, {12, 3}
    );
    loaded_replay.apply_current_tick(replayed);
    replayed.update();
    require(replayed.projectiles()[0].damage ==
            combat.projectiles()[0].damage);
    require(replayed.projectiles()[0].target ==
            combat.projectiles()[0].target);

    aoe::GameMap dock_technology_map(12, 8);
    prepare_dock_foundation(dock_technology_map, {0, 1});
    aoe::Simulation dock_techs(std::move(dock_technology_map));
    const auto dock = dock_techs.add_building(
        aoe::BuildingKind::dock, aoe::Player::blue, {0, 1}
    );
    dock_techs.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
    );
    dock_techs.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {8, 0}
    );
    dock_techs.replace_state(
        dock_techs.units(), dock_techs.buildings(),
        {1000, 2000, 2000, 1000}, {500, 500, 500, 500}, 0
    );
    dock_techs.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    dock_techs.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::cannon_galleon,
         aoe::Technology::elite_cannon_galleon,
         aoe::Technology::careening,
         aoe::Technology::dry_dock,
         aoe::Technology::shipwright}
    );
    require(dock_techs.queue_unit_at(
        dock, aoe::UnitKind::cannon_galleon
    ));
    const auto& order =
        dock_techs.buildings()[0].production_queue.front();
    require(order.kind == aoe::UnitKind::elite_cannon_galleon);
    require(order.paid_wood == 160 && order.paid_gold == 120);
    require(order.ticks_remaining == 9);

    aoe::GameMap transport_map(8, 6);
    for (int y = 3; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            transport_map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation transport(std::move(transport_map));
    transport.replace_technologies(
        aoe::Player::blue, {aoe::Technology::careening}
    );
    const auto ship = transport.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::blue, {3, 3}
    );
    for (int index = 0; index < 10; ++index) {
        const auto passenger = transport.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {3, 2}
        );
        require(transport.command_embark(passenger, ship));
    }
    const auto extra = transport.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 2}
    );
    require(!transport.command_embark(extra, ship));

    const auto make_speed_test = [](bool dry_dock) {
        aoe::GameMap map(140, 3);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 140; ++x)
                map.set_terrain({x, y}, aoe::Terrain::water);
        aoe::Simulation result(std::move(map));
        const auto id = result.add_unit(
            aoe::UnitKind::fishing_ship, aoe::Player::blue, {1, 1}
        );
        result.add_unit(
            aoe::UnitKind::fishing_ship, aoe::Player::red, {139, 2}
        );
        if (dry_dock) {
            result.replace_technologies(
                aoe::Player::blue,
                {aoe::Technology::careening, aoe::Technology::dry_dock}
            );
        }
        require(result.command_unit(id, {130, 1}));
        return result;
    };
    aoe::Simulation normal_speed = make_speed_test(false);
    aoe::Simulation dry_dock_speed = make_speed_test(true);
    for (int tick = 0; tick < 100; ++tick) {
        normal_speed.update();
        dry_dock_speed.update();
    }
    require(dry_dock_speed.units()[0].position.x ==
            normal_speed.units()[0].position.x + 15);
    require(dry_dock_speed.units()[0].movement_speed_remainder == 0);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-cannon.save";
    aoe::save_game(dock_techs, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::shipwright
    ));
    require(restored.buildings()[0].production_queue[0].kind ==
            aoe::UnitKind::elite_cannon_galleon);

    aoe::Scenario scenario(8, 6);
    scenario.map.set_terrain({3, 3}, aoe::Terrain::water);
    scenario.units.push_back({
        aoe::UnitKind::elite_cannon_galleon,
        aoe::Player::blue, {3, 3}
    });
    scenario.blue_technologies = {
        aoe::Technology::cannon_galleon,
        aoe::Technology::careening,
        aoe::Technology::dry_dock,
        aoe::Technology::shipwright,
    };
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-cannon.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.units[0].kind ==
            aoe::UnitKind::elite_cannon_galleon);
    require(loaded.blue_technologies.back() ==
            aoe::Technology::shipwright);
}

void civilization_unique_ship_lines_are_locked_and_deterministic() {
    const auto& longboat = aoe::rules_for(aoe::UnitKind::longboat);
    const auto& elite_longboat =
        aoe::rules_for(aoe::UnitKind::elite_longboat);
    require(longboat.hit_points == 130 && longboat.attack == 7);
    require(longboat.attack_range == 6 && longboat.projectile_count == 5);
    require(longboat.projectile_spread == 2);
    require(elite_longboat.hit_points == 160 &&
            elite_longboat.attack == 8);
    require(elite_longboat.attack_range == 7 &&
            elite_longboat.projectile_spread == 1);
    const auto& turtle = aoe::rules_for(aoe::UnitKind::turtle_ship);
    const auto& elite_turtle =
        aoe::rules_for(aoe::UnitKind::elite_turtle_ship);
    require(turtle.hit_points == 200 && turtle.attack == 50);
    require(turtle.attack_range == 6 &&
            turtle.minimum_attack_range == 0);
    require(turtle.melee_armor == 6 && turtle.pierce_armor == 5);
    require(elite_turtle.hit_points == 300 &&
            elite_turtle.melee_armor == 8 &&
            elite_turtle.pierce_armor == 6);
    require(aoe::rules_for(
        aoe::Technology::elite_longboat
    ).food_cost == 750);
    require(aoe::rules_for(
        aoe::Technology::elite_turtle_ship
    ).gold_cost == 800);

    const auto make_dock = [](aoe::Civilization civilization) {
        aoe::GameMap map(14, 8);
        for (int y = 4; y < 8; ++y)
            for (int x = 0; x < 14; ++x)
                map.set_terrain({x, y}, aoe::Terrain::water);
        prepare_dock_foundation(map, {0, 1});
        aoe::Simulation result(std::move(map));
        result.add_building(
            aoe::BuildingKind::dock, aoe::Player::blue, {0, 1}
        );
        result.add_building(
            aoe::BuildingKind::house, aoe::Player::blue, {4, 0}
        );
        result.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {10, 0}
        );
        result.replace_state(
            result.units(), result.buildings(),
            {1000, 2000, 2000, 1000}, {500, 500, 500, 500}, 0
        );
        result.replace_ages(aoe::Age::imperial, aoe::Age::dark);
        require(result.set_civilization(
            aoe::Player::blue, civilization
        ));
        return result;
    };
    aoe::Simulation generic = make_dock(aoe::Civilization::generic);
    require(!generic.queue_unit_at(1, aoe::UnitKind::longboat));
    require(!generic.queue_unit_at(1, aoe::UnitKind::turtle_ship));
    require(!generic.research_technology_at(
        1, aoe::Technology::elite_longboat
    ));
    require(!generic.research_technology_at(
        1, aoe::Technology::elite_turtle_ship
    ));

    aoe::Simulation vikings = make_dock(aoe::Civilization::vikings);
    const auto old_longboat = vikings.add_unit(
        aoe::UnitKind::longboat, aoe::Player::blue, {6, 4}
    );
    require(vikings.queue_unit_at(1, aoe::UnitKind::longboat));
    require(vikings.cancel_production_at(1));
    require(vikings.research_technology_at(
        1, aoe::Technology::elite_longboat
    ));
    for (int tick = 0; tick < 20; ++tick) vikings.update();
    require(vikings.has_technology(
        aoe::Player::blue, aoe::Technology::elite_longboat
    ));
    const auto upgraded = std::ranges::find_if(
        vikings.units(),
        [old_longboat](const aoe::Unit& unit) {
            return unit.id == old_longboat;
        }
    );
    require(upgraded != vikings.units().end());
    require(upgraded->kind == aoe::UnitKind::elite_longboat);
    require(vikings.queue_unit_at(1, aoe::UnitKind::longboat));
    require(vikings.buildings()[0].production_queue.back().kind ==
            aoe::UnitKind::elite_longboat);

    aoe::Simulation koreans = make_dock(aoe::Civilization::koreans);
    const auto old_turtle = koreans.add_unit(
        aoe::UnitKind::turtle_ship, aoe::Player::blue, {6, 4}
    );
    require(koreans.queue_unit_at(1, aoe::UnitKind::turtle_ship));
    require(koreans.cancel_production_at(1));
    require(koreans.research_technology_at(
        1, aoe::Technology::elite_turtle_ship
    ));
    for (int tick = 0; tick < 22; ++tick) koreans.update();
    require(koreans.has_technology(
        aoe::Player::blue, aoe::Technology::elite_turtle_ship
    ));
    require(std::ranges::find_if(
        koreans.units(),
        [old_turtle](const aoe::Unit& unit) {
            return unit.id == old_turtle &&
                unit.kind == aoe::UnitKind::elite_turtle_ship;
        }
    ) != koreans.units().end());

    aoe::GameMap combat_map(16, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 16; ++x)
            combat_map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation combat(std::move(combat_map));
    const auto attacker = combat.add_unit(
        aoe::UnitKind::longboat, aoe::Player::blue, {2, 3}
    );
    combat.add_unit(
        aoe::UnitKind::turtle_ship, aoe::Player::red, {7, 3}
    );
    require(combat.command_unit(attacker, {7, 3}));
    combat.update();
    require(combat.projectiles().size() == 6);
    require(combat.projectiles()[0].target == 2);
    require(combat.projectiles()[0].damage == 7);
    for (std::size_t index = 1; index < 5; ++index) {
        require(combat.projectiles()[index].target == 0);
        require(combat.projectiles()[index].damage == 0);
    }
    require(combat.projectiles()[5].damage == 50);

    aoe::Replay recorded;
    recorded.record(0, aoe::MoveUnitCommand{attacker, {7, 3}});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-unique-ships.replay";
    aoe::save_replay(recorded, replay_path);
    aoe::Replay replayed_commands = aoe::load_replay(replay_path);
    require(replayed_commands.commands().size() == 1);
    std::filesystem::remove(replay_path);
    aoe::GameMap replay_map(16, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 16; ++x)
            replay_map.set_terrain({x, y}, aoe::Terrain::water);
    aoe::Simulation replayed(std::move(replay_map));
    replayed.add_unit(
        aoe::UnitKind::longboat, aoe::Player::blue, {2, 3}
    );
    replayed.add_unit(
        aoe::UnitKind::turtle_ship, aoe::Player::red, {7, 3}
    );
    replayed_commands.apply_current_tick(replayed);
    replayed.update();
    require(replayed.projectiles().size() == combat.projectiles().size());
    for (std::size_t index = 0;
         index < combat.projectiles().size(); ++index) {
        require(replayed.projectiles()[index].target ==
                combat.projectiles()[index].target);
        require(replayed.projectiles()[index].damage ==
                combat.projectiles()[index].damage);
        require(replayed.projectiles()[index].visual_lane ==
                combat.projectiles()[index].visual_lane);
    }

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-unique-ships.save";
    aoe::save_game(koreans, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::elite_turtle_ship
    ));

    aoe::Scenario scenario(10, 6);
    scenario.map.set_terrain({2, 3}, aoe::Terrain::water);
    scenario.map.set_terrain({7, 3}, aoe::Terrain::water);
    scenario.blue_civilization = aoe::Civilization::vikings;
    scenario.red_civilization = aoe::Civilization::koreans;
    scenario.units = {
        {aoe::UnitKind::elite_longboat, aoe::Player::blue, {2, 3}},
        {aoe::UnitKind::elite_turtle_ship, aoe::Player::red, {7, 3}},
    };
    scenario.blue_technologies = {aoe::Technology::elite_longboat};
    scenario.red_technologies = {aoe::Technology::elite_turtle_ship};
    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-unique-ships.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.units[0].kind == aoe::UnitKind::elite_longboat);
    require(loaded.units[1].kind == aoe::UnitKind::elite_turtle_ship);
}

void castle_unique_lines_are_civilization_locked_and_upgrade() {
    require(aoe::rules_for(aoe::UnitKind::longbowman).hit_points == 35);
    require(aoe::rules_for(aoe::UnitKind::elite_longbowman).attack_range == 6);
    require(aoe::rules_for(
        aoe::UnitKind::throwing_axeman
    ).bonus_vs_buildings == 1);
    require(aoe::rules_for(
        aoe::UnitKind::elite_huskarl
    ).bonus_vs_archers == 10);
    require(aoe::rules_for(
        aoe::UnitKind::teutonic_knight
    ).melee_armor == 5);
    require(aoe::rules_for(
        aoe::UnitKind::elite_teutonic_knight
    ).melee_armor == 10);
    require(aoe::rules_for(
        aoe::Technology::elite_longbowman
    ).food_cost == 850);
    require(aoe::rules_for(
        aoe::Technology::elite_throwing_axeman
    ).gold_cost == 850);
    require(aoe::rules_for(
        aoe::Technology::elite_huskarl
    ).food_cost == 1200);
    require(aoe::rules_for(
        aoe::Technology::elite_teutonic_knight
    ).gold_cost == 600);
    require(aoe::rules_for(
        aoe::UnitKind::samurai
    ).bonus_vs_unique_units == 10);
    require(aoe::rules_for(
        aoe::UnitKind::elite_chu_ko_nu
    ).projectile_count == 6);
    require(aoe::rules_for(
        aoe::UnitKind::cataphract
    ).bonus_vs_infantry == 9);
    require(aoe::rules_for(
        aoe::UnitKind::cataphract
    ).splash_radius == 0);
    require(aoe::rules_for(
        aoe::UnitKind::war_elephant
    ).splash_radius == 0);
    require(aoe::rules_for(
        aoe::UnitKind::elite_war_elephant
    ).splash_radius == 1);
    require(aoe::is_infantry(aoe::UnitKind::samurai));
    require(aoe::is_archer(aoe::UnitKind::chu_ko_nu));
    require(aoe::is_cavalry(aoe::UnitKind::cataphract));
    require(aoe::is_cavalry(aoe::UnitKind::war_elephant));
    require(aoe::rules_for(aoe::UnitKind::mameluke).damage_class ==
            aoe::DamageClass::melee);
    require(aoe::rules_for(aoe::UnitKind::mameluke).bonus_vs_cavalry == 9);
    require(aoe::rules_for(aoe::UnitKind::janissary).accuracy_percent == 50);
    require(aoe::rules_for(aoe::UnitKind::elite_berserk).hit_points == 60);
    require(aoe::rules_for(aoe::UnitKind::elite_mangudai).bonus_vs_siege == 5);
    require(aoe::rules_for(
        aoe::UnitKind::jaguar_warrior
    ).bonus_vs_infantry == 10);
    require(aoe::rules_for(
        aoe::UnitKind::elite_plumed_archer
    ).accuracy_percent == 90);
    require(aoe::rules_for(
        aoe::UnitKind::conquistador
    ).accuracy_percent == 65);
    require(aoe::rules_for(aoe::UnitKind::elite_tarkan).bonus_vs_buildings ==
            10);
    require(aoe::rules_for(aoe::UnitKind::woad_raider).hit_points == 65);
    require(aoe::rules_for(aoe::UnitKind::woad_raider).attack == 8);
    require(aoe::rules_for(
        aoe::UnitKind::woad_raider
    ).bonus_vs_eagle_warriors == 2);
    require(aoe::rules_for(
        aoe::UnitKind::elite_woad_raider
    ).hit_points == 80);
    require(aoe::rules_for(
        aoe::Technology::elite_woad_raider
    ).food_cost == 1000);
    require(aoe::rules_for(
        aoe::Technology::elite_woad_raider
    ).gold_cost == 800);

    const auto make_castle = [](aoe::Civilization civilization) {
        aoe::Simulation result(aoe::GameMap(22, 10));
        result.add_building(
            aoe::BuildingKind::castle, aoe::Player::blue, {0, 0}
        );
        result.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {15, 0}
        );
        result.replace_state(
            result.units(), result.buildings(),
            {2000, 3000, 3000, 2000}, {500, 500, 500, 500}, 0
        );
        result.replace_ages(aoe::Age::imperial, aoe::Age::dark);
        require(result.set_civilization(
            aoe::Player::blue, civilization
        ));
        return result;
    };

    aoe::Simulation generic = make_castle(aoe::Civilization::generic);
    require(!generic.queue_unit_at(1, aoe::UnitKind::longbowman));
    require(!generic.queue_unit_at(1, aoe::UnitKind::throwing_axeman));
    require(!generic.queue_unit_at(1, aoe::UnitKind::huskarl));
    require(!generic.queue_unit_at(1, aoe::UnitKind::teutonic_knight));
    require(!generic.queue_unit_at(1, aoe::UnitKind::samurai));
    require(!generic.queue_unit_at(1, aoe::UnitKind::chu_ko_nu));
    require(!generic.queue_unit_at(1, aoe::UnitKind::cataphract));
    require(!generic.queue_unit_at(1, aoe::UnitKind::war_elephant));
    require(!generic.queue_unit_at(1, aoe::UnitKind::mameluke));
    require(!generic.queue_unit_at(1, aoe::UnitKind::janissary));
    require(!generic.queue_unit_at(1, aoe::UnitKind::berserk));
    require(!generic.queue_unit_at(1, aoe::UnitKind::mangudai));
    require(!generic.queue_unit_at(1, aoe::UnitKind::jaguar_warrior));
    require(!generic.queue_unit_at(1, aoe::UnitKind::plumed_archer));
    require(!generic.queue_unit_at(1, aoe::UnitKind::conquistador));
    require(!generic.queue_unit_at(1, aoe::UnitKind::tarkan));
    require(!generic.queue_unit_at(1, aoe::UnitKind::woad_raider));

    struct Line {
        aoe::Civilization civilization;
        aoe::UnitKind base;
        aoe::UnitKind elite;
        aoe::Technology technology;
        int ticks;
    };
    const std::array lines{
        Line{aoe::Civilization::britons, aoe::UnitKind::longbowman,
             aoe::UnitKind::elite_longbowman,
             aoe::Technology::elite_longbowman, 20},
        Line{aoe::Civilization::franks, aoe::UnitKind::throwing_axeman,
             aoe::UnitKind::elite_throwing_axeman,
             aoe::Technology::elite_throwing_axeman, 15},
        Line{aoe::Civilization::goths, aoe::UnitKind::huskarl,
             aoe::UnitKind::elite_huskarl,
             aoe::Technology::elite_huskarl, 13},
        Line{aoe::Civilization::teutons,
             aoe::UnitKind::teutonic_knight,
             aoe::UnitKind::elite_teutonic_knight,
             aoe::Technology::elite_teutonic_knight, 17},
        Line{aoe::Civilization::japanese, aoe::UnitKind::samurai,
             aoe::UnitKind::elite_samurai,
             aoe::Technology::elite_samurai, 20},
        Line{aoe::Civilization::chinese, aoe::UnitKind::chu_ko_nu,
             aoe::UnitKind::elite_chu_ko_nu,
             aoe::Technology::elite_chu_ko_nu, 17},
        Line{aoe::Civilization::byzantines, aoe::UnitKind::cataphract,
             aoe::UnitKind::elite_cataphract,
             aoe::Technology::elite_cataphract, 17},
        Line{aoe::Civilization::persians, aoe::UnitKind::war_elephant,
             aoe::UnitKind::elite_war_elephant,
             aoe::Technology::elite_war_elephant, 25},
        Line{aoe::Civilization::saracens, aoe::UnitKind::mameluke,
             aoe::UnitKind::elite_mameluke,
             aoe::Technology::elite_mameluke, 17},
        Line{aoe::Civilization::turks, aoe::UnitKind::janissary,
             aoe::UnitKind::elite_janissary,
             aoe::Technology::elite_janissary, 18},
        Line{aoe::Civilization::vikings, aoe::UnitKind::berserk,
             aoe::UnitKind::elite_berserk,
             aoe::Technology::elite_berserk, 15},
        Line{aoe::Civilization::mongols, aoe::UnitKind::mangudai,
             aoe::UnitKind::elite_mangudai,
             aoe::Technology::elite_mangudai, 17},
        Line{aoe::Civilization::aztecs, aoe::UnitKind::jaguar_warrior,
             aoe::UnitKind::elite_jaguar_warrior,
             aoe::Technology::elite_jaguar_warrior, 15},
        Line{aoe::Civilization::mayans, aoe::UnitKind::plumed_archer,
             aoe::UnitKind::elite_plumed_archer,
             aoe::Technology::elite_plumed_archer, 15},
        Line{aoe::Civilization::spanish, aoe::UnitKind::conquistador,
             aoe::UnitKind::elite_conquistador,
             aoe::Technology::elite_conquistador, 20},
        Line{aoe::Civilization::huns, aoe::UnitKind::tarkan,
             aoe::UnitKind::elite_tarkan,
             aoe::Technology::elite_tarkan, 15},
        Line{aoe::Civilization::celts, aoe::UnitKind::woad_raider,
             aoe::UnitKind::elite_woad_raider,
             aoe::Technology::elite_woad_raider, 45},
    };
    aoe::Simulation persisted = make_castle(aoe::Civilization::persians);
    for (const Line& line : lines) {
        aoe::Simulation simulation = make_castle(line.civilization);
        const auto existing = simulation.add_unit(
            line.base, aoe::Player::blue, {7, 5}
        );
        require(simulation.queue_unit_at(1, line.base));
        require(simulation.cancel_production_at(1));
        require(simulation.research_technology_at(1, line.technology));
        for (int tick = 0; tick < line.ticks; ++tick) {
            simulation.update();
        }
        const auto upgraded = std::ranges::find_if(
            simulation.units(),
            [existing](const aoe::Unit& unit) {
                return unit.id == existing;
            }
        );
        require(upgraded != simulation.units().end());
        require(upgraded->kind == line.elite);
        require(simulation.queue_unit_at(1, line.base));
        require(simulation.buildings()[0].production_queue.back().kind ==
                line.elite);
        if (line.civilization == aoe::Civilization::celts) {
            persisted = simulation;
        }
    }

    aoe::Simulation regeneration = make_castle(
        aoe::Civilization::vikings
    );
    regeneration.add_unit(
        aoe::UnitKind::berserk, aoe::Player::blue, {7, 5}
    );
    auto damaged_units = regeneration.units();
    damaged_units[0].hit_points = 30;
    regeneration.replace_state(
        damaged_units, regeneration.buildings(),
        regeneration.economy(aoe::Player::blue),
        regeneration.economy(aoe::Player::red), 0
    );
    for (int tick = 0; tick < 6; ++tick) regeneration.update();
    require(regeneration.units()[0].hit_points == 32);
    regeneration.replace_technologies(
        aoe::Player::blue, {aoe::Technology::berserkergang}
    );
    for (int tick = 0; tick < 6; ++tick) regeneration.update();
    require(regeneration.units()[0].hit_points == 36);

    aoe::Simulation blacksmith = make_castle(aoe::Civilization::britons);
    blacksmith.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::fletching, aoe::Technology::bodkin_arrow,
         aoe::Technology::bracer}
    );
    blacksmith.add_unit(
        aoe::UnitKind::longbowman, aoe::Player::blue, {7, 5}
    );
    require(blacksmith.units().back().attack == 9);
    aoe::Simulation infantry = make_castle(aoe::Civilization::franks);
    infantry.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::forging, aoe::Technology::iron_casting,
         aoe::Technology::blast_furnace}
    );
    infantry.add_unit(
        aoe::UnitKind::throwing_axeman, aoe::Player::blue, {7, 5}
    );
    require(infantry.units().back().attack == 11);

    aoe::Simulation counter(aoe::GameMap(12, 7));
    const auto huskarl = counter.add_unit(
        aoe::UnitKind::huskarl, aoe::Player::blue, {2, 3}
    );
    counter.add_unit(
        aoe::UnitKind::archer, aoe::Player::red, {3, 3}
    );
    require(counter.command_unit(huskarl, {3, 3}));
    counter.update();
    require(counter.units()[1].hit_points == 14);

    aoe::Simulation unique_counter(aoe::GameMap(12, 7));
    const auto samurai = unique_counter.add_unit(
        aoe::UnitKind::samurai, aoe::Player::blue, {2, 3}
    );
    unique_counter.add_unit(
        aoe::UnitKind::huskarl, aoe::Player::red, {3, 3}
    );
    require(unique_counter.command_unit(samurai, {3, 3}));
    unique_counter.update();
    require(unique_counter.units()[1].hit_points == 42);

    aoe::Simulation infantry_counter(aoe::GameMap(12, 7));
    const auto cataphract = infantry_counter.add_unit(
        aoe::UnitKind::cataphract, aoe::Player::blue, {2, 3}
    );
    infantry_counter.add_unit(
        aoe::UnitKind::samurai, aoe::Player::red, {3, 3}
    );
    require(infantry_counter.command_unit(cataphract, {3, 3}));
    infantry_counter.update();
    require(infantry_counter.units()[1].hit_points == 43);

    aoe::Simulation jaguar_counter(aoe::GameMap(12, 7));
    const auto jaguar = jaguar_counter.add_unit(
        aoe::UnitKind::jaguar_warrior, aoe::Player::blue, {2, 3}
    );
    jaguar_counter.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {3, 3}
    );
    require(jaguar_counter.command_unit(jaguar, {3, 3}));
    jaguar_counter.update();
    require(jaguar_counter.units()[1].hit_points == 20);

    aoe::Simulation volley(aoe::GameMap(12, 7));
    const auto chu = volley.add_unit(
        aoe::UnitKind::chu_ko_nu, aoe::Player::blue, {2, 3}
    );
    volley.add_unit(
        aoe::UnitKind::teutonic_knight, aoe::Player::red, {6, 3}
    );
    require(volley.command_unit(chu, {6, 3}));
    for (int tick = 0; tick < 3; ++tick) volley.update();
    require(volley.projectiles().size() == 4);
    require(volley.projectiles()[0].damage == 8);
    for (std::size_t index = 1; index < 4; ++index) {
        require(volley.projectiles()[index].damage == 0);
    }

    aoe::Simulation trample(aoe::GameMap(12, 7));
    const auto elephant = trample.add_unit(
        aoe::UnitKind::elite_war_elephant,
        aoe::Player::blue, {2, 3}
    );
    trample.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {3, 3}
    );
    trample.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {4, 3}
    );
    require(trample.command_unit(elephant, {3, 3}));
    trample.update();
    require(trample.units()[1].hit_points < 40);
    require(trample.units()[2].hit_points < 40);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-castle-unique.save";
    aoe::save_game(persisted, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::elite_woad_raider
    ));
    require(restored.units()[0].kind ==
            aoe::UnitKind::elite_woad_raider);

    aoe::Replay replay;
    replay.record(
        0, aoe::QueueUnitCommand{1, aoe::UnitKind::woad_raider}
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-castle-unique.replay";
    aoe::save_replay(replay, replay_path);
    aoe::Replay replayed = aoe::load_replay(replay_path);
    require(replayed.commands().size() == 1);
    std::filesystem::remove(replay_path);
    aoe::Simulation replay_target =
        make_castle(aoe::Civilization::celts);
    replayed.apply_current_tick(replay_target);
    require(replay_target.buildings()[0].production_queue[0].kind ==
            aoe::UnitKind::woad_raider);

    aoe::Scenario scenario(12, 7);
    scenario.blue_civilization = aoe::Civilization::generic;
    scenario.red_civilization = aoe::Civilization::generic;
    scenario.units = {
        {aoe::UnitKind::elite_jaguar_warrior,
         aoe::Player::blue, {2, 3}},
        {aoe::UnitKind::elite_conquistador,
         aoe::Player::red, {8, 3}},
        {aoe::UnitKind::elite_woad_raider,
         aoe::Player::blue, {4, 3}},
    };
    scenario.blue_technologies = {
        aoe::Technology::elite_jaguar_warrior
    };
    scenario.red_technologies = {
        aoe::Technology::elite_conquistador
    };
    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-castle-unique.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.units[0].kind ==
            aoe::UnitKind::elite_jaguar_warrior);
    require(loaded.units[1].kind ==
            aoe::UnitKind::elite_conquistador);
    require(loaded.units[2].kind ==
            aoe::UnitKind::elite_woad_raider);
}

void civilization_availability_matches_live_dat_matrix() {
    using Words = std::array<std::uint64_t, 2>;
    const std::array civilizations{
        aoe::Civilization::britons, aoe::Civilization::franks,
        aoe::Civilization::teutons, aoe::Civilization::goths,
        aoe::Civilization::celts, aoe::Civilization::vikings,
        aoe::Civilization::byzantines, aoe::Civilization::japanese,
        aoe::Civilization::chinese, aoe::Civilization::persians,
        aoe::Civilization::saracens, aoe::Civilization::turks,
        aoe::Civilization::mongols, aoe::Civilization::spanish,
        aoe::Civilization::huns, aoe::Civilization::koreans,
        aoe::Civilization::aztecs, aoe::Civilization::mayans,
    };
    const std::array<Words, 18> units{{
        {0x00000c0ffffebfffULL, 0x94f5000}, {0x0000301ffff6ffffULL, 0xf4f7000},
        {0x0003001ffff67fffULL, 0xf47f000}, {0x0000c01ffff7bfffULL, 0xf4f7000},
        {0x0000001dfff7ffffULL, 0x9cff000}, {0xc00000fcfffebfffULL, 0x8c77000},
        {0x00c0003fffffffffULL, 0xfff5000}, {0x000c0037fffebfffULL, 0xb4f7000},
        {0x0030001dfffebfffULL, 0x9ff7000}, {0x0300003ffff1ffffULL, 0xfff7000},
        {0x0c00003dffff9fffULL, 0xeffd000}, {0x3000003dffe7b7ffULL, 0xeff3000},
        {0x0000003fffffbfffULL, 0x8fff003}, {0x0000003ffff7fbffULL, 0xfcf50c0},
        {0x0000001dfff3ffffULL, 0x9cf1300}, {0x00000313ffffbfffULL, 0xf4fd000},
        {0x00000003bffe1ff5ULL, 0x8c3dc0c}, {0x0000000ffffa1ff5ULL, 0x9c37c30},
    }};
    const std::array<std::uint64_t, 18> buildings{{
        0x7fffff, 0x7fffff, 0xffffff, 0x737fff, 0x7fffff, 0x7fffff,
        0xffffff, 0x7fffff, 0xffffff, 0x7fffff, 0x7fffff, 0xffffff,
        0x7fffff, 0xffffff, 0x7fffff, 0x7fffff, 0x7ffeff, 0x7ffeff,
    }};
    const std::array<Words, 18> technologies{{
        {0x000c39feb7ffffffULL, 0x1cd5000080000},
        {0x00301bf6f3ff5fffULL, 0x1fd5800100000},
        {0x03000bf66fff7fffULL, 0x3fd3800400000},
        {0x00c02bf7bf6fc7ffULL, 0x1bd5800200000},
        {0x00003b77f37f7fffULL, 0x1cf7800000000}, {0x0000df7ea77fdfffULL, 0x1cb18000004c0},
        {0xc0003ffff7fdffffULL, 0x3bfd002000000},
        {0x0c003efeb77fffffULL, 0x1dd5800800000},
        {0x30003b7ebfffffffULL, 0x38fd801000000},
        {0x00001ff1ffff57ffULL, 0x1bfd804000003},
        {0x00001f7f9fffffffULL, 0x1fbf00800000c},
        {0x00003f67bfffffbfULL, 0x3bbc810000030},
        {0x00002fffbb7fdfffULL, 0x1cbf820000300},
        {0x00003ff7ffffffdfULL, 0x3bf5040018000},
        {0x00001b73fbefc7ffULL, 0x18b4080060000},
        {0x00033affb77dcfffULL, 0x1fd7100000000},
        {0x000038be031fdfffULL, 0x1cb3400001800},
        {0x000039fa071fffffULL, 0x18f1e00006000},
    }};
    const auto bit = [](const Words& words, std::size_t index) {
        return (words[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
    };
    for (std::size_t civ = 0; civ < civilizations.size(); ++civ) {
        for (std::size_t unit = 0; unit < 92; ++unit) {
            require(aoe::civilization_has_unit(
                civilizations[civ], static_cast<aoe::UnitKind>(unit)
            ) == bit(units[civ], unit));
        }
        for (std::size_t building = 0; building < 24; ++building) {
            require(aoe::civilization_has_building(
                civilizations[civ],
                static_cast<aoe::BuildingKind>(building)
            ) == ((buildings[civ] & (std::uint64_t{1} << building)) != 0));
        }
        for (std::size_t technology = 0; technology < 114; ++technology) {
            require(aoe::civilization_has_technology(
                civilizations[civ],
                static_cast<aoe::Technology>(technology)
            ) == bit(technologies[civ], technology));
        }
    }
    require(aoe::civilization_has_unit(
        aoe::Civilization::generic, aoe::UnitKind::paladin
    ));
    require(aoe::civilization_has_building(
        aoe::Civilization::generic, aoe::BuildingKind::dock
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::generic, aoe::Technology::bracer
    ));

    aoe::Simulation commands(aoe::GameMap(20, 10));
    const auto villager = commands.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    commands.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 2}
    );
    commands.replace_state(
        commands.units(), commands.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    require(commands.set_civilization(
        aoe::Player::blue, aoe::Civilization::aztecs
    ));
    require(!commands.construct_building_at(
        villager, aoe::BuildingKind::stable, {3, 2}
    ));

    aoe::Simulation britons(aoe::GameMap(20, 10));
    const auto stable = britons.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {1, 1}
    );
    britons.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 2}
    );
    britons.replace_state(
        britons.units(), britons.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    britons.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(britons.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    require(!britons.queue_unit_at(stable, aoe::UnitKind::paladin));
    require(!britons.research_technology_at(
        stable, aoe::Technology::paladin
    ));

    aoe::Replay unavailable_replay;
    unavailable_replay.record(
        0, aoe::QueueUnitCommand{stable, aoe::UnitKind::paladin}
    );
    bool replay_rejected{};
    try {
        unavailable_replay.apply_current_tick(britons);
    } catch (const std::runtime_error&) {
        replay_rejected = true;
    }
    require(replay_rejected);
    require(britons.buildings()[0].production_queue.empty());

    britons.add_unit(
        aoe::UnitKind::paladin, aoe::Player::blue, {8, 5}
    );
    const auto legacy_save =
        std::filesystem::temp_directory_path() / "aoe-illegal-legacy.save";
    aoe::save_game(britons, legacy_save);
    const aoe::Simulation legacy_loaded = aoe::load_game(legacy_save);
    std::filesystem::remove(legacy_save);
    require(std::ranges::any_of(
        legacy_loaded.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::paladin;
        }
    ));

    aoe::Scenario invalid_new(10, 8);
    invalid_new.blue_civilization = aoe::Civilization::aztecs;
    invalid_new.buildings.push_back({
        aoe::BuildingKind::stable, aoe::Player::blue, {1, 1}
    });
    bool rejected{};
    try {
        (void)aoe::create_simulation(invalid_new);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected);

    const auto legacy_scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-illegal-legacy.scenario";
    {
        std::ofstream legacy(legacy_scenario_path);
        legacy << "AOE-ARCHAEOLOGY-SCENARIO 40\n"
               << "map 10 8\n"
               << "civilization blue aztecs\n"
               << "building stable blue 1 1\n";
    }
    const aoe::Scenario legacy_scenario =
        aoe::load_scenario(legacy_scenario_path);
    std::filesystem::remove(legacy_scenario_path);
    const aoe::Simulation legacy_scenario_simulation =
        aoe::create_simulation(legacy_scenario);
    require(legacy_scenario_simulation.buildings()[0].kind ==
            aoe::BuildingKind::stable);
}

void first_unique_technologies_follow_live_dat_effects() {
    require(aoe::rules_for(aoe::Technology::yeomen).wood_cost == 750);
    require(aoe::rules_for(aoe::Technology::yeomen).gold_cost == 450);
    require(aoe::rules_for(aoe::Technology::bearded_axe).food_cost == 400);
    require(aoe::rules_for(aoe::Technology::anarchy).research_ticks == 13);
    require(aoe::rules_for(aoe::Technology::crenellations).stone_cost == 400);
    require(aoe::civilization_has_technology(
        aoe::Civilization::britons, aoe::Technology::yeomen
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::franks, aoe::Technology::yeomen
    ));

    aoe::Simulation anarchy(aoe::GameMap(18, 9));
    const auto barracks = anarchy.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {1, 1}
    );
    anarchy.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {5, 1}
    );
    anarchy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 1}
    );
    anarchy.replace_state(
        anarchy.units(), anarchy.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    anarchy.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(anarchy.set_civilization(
        aoe::Player::blue, aoe::Civilization::goths
    ));
    require(!anarchy.queue_unit_at(barracks, aoe::UnitKind::huskarl));
    anarchy.replace_technologies(
        aoe::Player::blue, {aoe::Technology::anarchy}
    );
    require(anarchy.queue_unit_at(barracks, aoe::UnitKind::huskarl));
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-unique-tech.save";
    aoe::save_game(anarchy, save_path);
    const aoe::Simulation restored_save = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored_save.has_technology(
        aoe::Player::blue, aoe::Technology::anarchy
    ));

    aoe::Simulation yeomen(aoe::GameMap(20, 9));
    const auto archer = yeomen.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 4}
    );
    yeomen.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {9, 4}
    );
    yeomen.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    yeomen.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 1}
    );
    yeomen.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(yeomen.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    yeomen.replace_technologies(
        aoe::Player::blue, {aoe::Technology::yeomen}
    );
    const auto longbow = yeomen.add_unit(
        aoe::UnitKind::longbowman, aoe::Player::blue, {2, 7}
    );
    const auto longbow_unit = std::ranges::find_if(
        yeomen.units(),
        [longbow](const aoe::Unit& unit) {
            return unit.id == longbow;
        }
    );
    require(longbow_unit != yeomen.units().end());
    require(yeomen.effective_attack_range(*longbow_unit) == 6);
    aoe::Scenario yeomen_scenario(12, 8);
    yeomen_scenario.blue_civilization = aoe::Civilization::britons;
    yeomen_scenario.blue_age = aoe::Age::imperial;
    yeomen_scenario.blue_technologies = {aoe::Technology::yeomen};
    yeomen_scenario.units = {{
        aoe::UnitKind::longbowman, aoe::Player::blue, {3, 3}
    }};
    const aoe::Simulation yeomen_scenario_simulation =
        aoe::create_simulation(yeomen_scenario);
    require(yeomen_scenario_simulation.has_technology(
        aoe::Player::blue, aoe::Technology::yeomen
    ));
    require(yeomen_scenario_simulation.effective_attack_range(
        yeomen_scenario_simulation.units()[0]
    ) == 6);
    require(yeomen.command_unit(archer, {9, 4}));
    for (int tick = 0; tick < 3; ++tick) yeomen.update();
    require(!yeomen.projectiles().empty());

    aoe::Simulation axes(aoe::GameMap(16, 8));
    const auto axeman = axes.add_unit(
        aoe::UnitKind::throwing_axeman, aoe::Player::blue, {2, 4}
    );
    axes.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {6, 4}
    );
    axes.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    axes.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 1}
    );
    axes.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(axes.set_civilization(
        aoe::Player::blue, aoe::Civilization::franks
    ));
    axes.replace_technologies(
        aoe::Player::blue, {aoe::Technology::bearded_axe}
    );
    require(axes.command_unit(axeman, {6, 4}));
    for (int tick = 0; tick < 8; ++tick) axes.update();
    require(!axes.projectiles().empty());

    aoe::Simulation crenellations(aoe::GameMap(22, 9));
    crenellations.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {1, 1}
    );
    crenellations.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {15, 2}
    );
    crenellations.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {19, 1}
    );
    crenellations.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(crenellations.set_civilization(
        aoe::Player::blue, aoe::Civilization::teutons
    ));
    crenellations.replace_technologies(
        aoe::Player::blue, {aoe::Technology::crenellations}
    );
    crenellations.update();
    require(!crenellations.projectiles().empty());

    aoe::Simulation tower(aoe::GameMap(14, 8));
    tower.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {1, 1}
    );
    tower.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {5, 1}
    );
    tower.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {11, 1}
    );
    tower.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(tower.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    tower.replace_technologies(
        aoe::Player::blue, {aoe::Technology::yeomen}
    );
    tower.update();
    require(!tower.projectiles().empty());
    require(tower.projectiles()[0].damage ==
            aoe::rules_for(aoe::BuildingKind::watch_tower).attack + 2);

    require(aoe::rules_for(aoe::Technology::kataparuto).wood_cost == 750);
    require(aoe::rules_for(aoe::Technology::rocketry).gold_cost == 750);
    require(aoe::rules_for(aoe::Technology::logistica).research_ticks == 17);
    require(aoe::rules_for(aoe::Technology::mahouts).food_cost == 300);
    require(aoe::civilization_has_technology(
        aoe::Civilization::japanese, aoe::Technology::kataparuto
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::chinese, aoe::Technology::rocketry
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::byzantines, aoe::Technology::logistica
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::persians, aoe::Technology::mahouts
    ));

    aoe::Simulation rockets(aoe::GameMap(12, 8));
    rockets.add_unit(
        aoe::UnitKind::chu_ko_nu, aoe::Player::blue, {2, 3}
    );
    rockets.replace_technologies(
        aoe::Player::blue, {aoe::Technology::rocketry}
    );
    require(rockets.units()[0].attack ==
            aoe::rules_for(aoe::UnitKind::chu_ko_nu).attack + 2);
    rockets.add_unit(
        aoe::UnitKind::elite_chu_ko_nu, aoe::Player::blue, {3, 3}
    );
    require(rockets.units()[1].attack ==
            aoe::rules_for(aoe::UnitKind::elite_chu_ko_nu).attack + 2);
    const auto rocketry_save =
        std::filesystem::temp_directory_path() / "aoe-rocketry.save";
    aoe::save_game(rockets, rocketry_save);
    const aoe::Simulation restored_rocketry =
        aoe::load_game(rocketry_save);
    std::filesystem::remove(rocketry_save);
    require(restored_rocketry.has_technology(
        aoe::Player::blue, aoe::Technology::rocketry
    ));

    aoe::Simulation logistica(aoe::GameMap(12, 8));
    const auto cataphract = logistica.add_unit(
        aoe::UnitKind::cataphract, aoe::Player::blue, {2, 3}
    );
    logistica.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {3, 3}
    );
    logistica.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {4, 3}
    );
    logistica.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 6}
    );
    logistica.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 1}
    );
    logistica.replace_technologies(
        aoe::Player::blue, {aoe::Technology::logistica}
    );
    require(logistica.command_unit(cataphract, {3, 3}));
    logistica.update();
    require(logistica.units()[1].hit_points == 16);
    require(logistica.units()[2].hit_points == 16);
    require(!logistica.impact_effects().empty());
    require(logistica.impact_effects()[0].splash);

    aoe::Simulation elephant_speed(aoe::GameMap(30, 8));
    const auto elephant = elephant_speed.add_unit(
        aoe::UnitKind::war_elephant, aoe::Player::blue, {2, 3}
    );
    elephant_speed.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 6}
    );
    elephant_speed.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {27, 1}
    );
    elephant_speed.replace_technologies(
        aoe::Player::blue, {aoe::Technology::mahouts}
    );
    require(elephant_speed.unique_unit_movement_numerator(
        elephant_speed.units()[0]
    ) == 78);
    require(elephant_speed.command_unit(elephant, {20, 3}));
    for (int tick = 0; tick < 10; ++tick) elephant_speed.update();
    require(elephant_speed.units()[0].position.x >= 9);

    require(aoe::rules_for(aoe::Technology::zealotry).food_cost == 750);
    require(aoe::rules_for(aoe::Technology::artillery).stone_cost == 450);
    require(aoe::rules_for(aoe::Technology::drill).research_ticks == 20);
    require(aoe::rules_for(
        aoe::Technology::berserkergang
    ).research_ticks == 13);

    aoe::Simulation zealotry(aoe::GameMap(16, 8));
    const auto zealotry_castle = zealotry.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {1, 1}
    );
    zealotry.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 1}
    );
    zealotry.add_unit(
        aoe::UnitKind::mameluke, aoe::Player::blue, {7, 4}
    );
    zealotry.replace_state(
        zealotry.units(), zealotry.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    zealotry.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(zealotry.set_civilization(
        aoe::Player::blue, aoe::Civilization::saracens
    ));
    require(zealotry.research_technology_at(
        zealotry_castle, aoe::Technology::zealotry
    ));
    for (int tick = 0; tick < 17; ++tick) zealotry.update();
    require(zealotry.units()[0].hit_points == 95);
    require(zealotry.maximum_hit_points(zealotry.units()[0]) == 95);
    const auto zealotry_save =
        std::filesystem::temp_directory_path() / "aoe-zealotry.save";
    aoe::save_game(zealotry, zealotry_save);
    const aoe::Simulation restored_zealotry =
        aoe::load_game(zealotry_save);
    std::filesystem::remove(zealotry_save);
    require(restored_zealotry.has_technology(
        aoe::Player::blue, aoe::Technology::zealotry
    ));
    require(restored_zealotry.maximum_hit_points(
        restored_zealotry.units()[0]
    ) == 95);

    aoe::GameMap artillery_map(16, 8);
    artillery_map.set_terrain({2, 3}, aoe::Terrain::water);
    artillery_map.set_terrain({3, 3}, aoe::Terrain::water);
    aoe::Simulation artillery(std::move(artillery_map));
    artillery.add_unit(
        aoe::UnitKind::cannon_galleon, aoe::Player::blue, {2, 3}
    );
    artillery.add_unit(
        aoe::UnitKind::elite_cannon_galleon,
        aoe::Player::blue, {3, 3}
    );
    artillery.replace_technologies(
        aoe::Player::blue, {aoe::Technology::artillery}
    );
    require(artillery.effective_attack_range(artillery.units()[0]) == 15);
    require(artillery.effective_attack_range(artillery.units()[1]) == 17);
    require(artillery.effective_unit_vision_range(
        artillery.units()[0]
    ) == aoe::rules_for(aoe::UnitKind::cannon_galleon).vision_range + 2);

    aoe::Simulation drill(aoe::GameMap(24, 8));
    drill.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {2, 3}
    );
    drill.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {2, 5}
    );
    require(drill.effective_siege_movement_numerator(
        drill.units()[0]
    ) == 50);
    require(drill.effective_siege_movement_numerator(
        drill.units()[1]
    ) == 60);
    drill.replace_technologies(
        aoe::Player::blue, {aoe::Technology::drill}
    );
    require(drill.effective_siege_movement_numerator(
        drill.units()[0]
    ) == 75);
    require(drill.effective_siege_movement_numerator(
        drill.units()[1]
    ) == 90);

    aoe::Simulation berserk_status(aoe::GameMap(8, 8));
    berserk_status.add_unit(
        aoe::UnitKind::berserk, aoe::Player::blue, {2, 2}
    );
    require(berserk_status.berserk_regeneration_per_three_ticks(
        berserk_status.units()[0]
    ) == 1);
    berserk_status.replace_technologies(
        aoe::Player::blue, {aoe::Technology::berserkergang}
    );
    require(berserk_status.berserk_regeneration_per_three_ticks(
        berserk_status.units()[0]
    ) == 2);

    aoe::Scenario berserk_combat(18, 12);
    berserk_combat.blue_economy = {5000, 5000, 5000, 5000};
    berserk_combat.red_economy = {0, 0, 0, 0};
    berserk_combat.blue_age = aoe::Age::imperial;
    berserk_combat.red_age = aoe::Age::imperial;
    berserk_combat.blue_civilization = aoe::Civilization::vikings;
    berserk_combat.red_civilization = aoe::Civilization::franks;
    berserk_combat.blue_technologies = {
        aoe::Technology::berserkergang
    };
    berserk_combat.buildings = {
        {aoe::BuildingKind::castle, aoe::Player::blue, {3, 3}},
        {aoe::BuildingKind::house, aoe::Player::red, {15, 9}},
    };
    berserk_combat.units = {
        {aoe::UnitKind::berserk, aoe::Player::blue, {9, 4}},
        {aoe::UnitKind::militia, aoe::Player::red, {10, 4}},
    };
    aoe::Simulation healing_after_combat =
        aoe::create_simulation(berserk_combat);
    for (int tick = 0; tick < 14 &&
         std::ranges::any_of(
             healing_after_combat.units(),
             [](const aoe::Unit& unit) {
                 return unit.kind == aoe::UnitKind::militia;
             }
         ); ++tick) {
        healing_after_combat.update();
    }
    require(std::ranges::none_of(
        healing_after_combat.units(),
        [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::militia;
        }
    ));
    const int post_combat_hit_points =
        healing_after_combat.units()[0].hit_points;
    for (int tick = 0; tick < 6; ++tick) {
        healing_after_combat.update();
    }
    require(healing_after_combat.outcome() == aoe::MatchOutcome::ongoing);
    require(healing_after_combat.units()[0].hit_points >
            post_combat_hit_points);

    require(aoe::rules_for(aoe::Technology::supremacy).food_cost == 400);
    require(aoe::rules_for(aoe::Technology::supremacy).gold_cost == 250);
    require(aoe::rules_for(aoe::Technology::atheism).research_ticks == 20);
    require(aoe::rules_for(aoe::Technology::shinkichon).wood_cost == 800);
    require(aoe::rules_for(aoe::Technology::el_dorado).research_ticks == 17);

    aoe::Simulation supremacy(aoe::GameMap(16, 8));
    const auto supremacy_castle = supremacy.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {1, 1}
    );
    supremacy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 1}
    );
    supremacy.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 4}
    );
    supremacy.replace_state(
        supremacy.units(), supremacy.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    supremacy.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(supremacy.set_civilization(
        aoe::Player::blue, aoe::Civilization::spanish
    ));
    const int villager_base_attack = supremacy.units()[0].attack;
    require(supremacy.research_technology_at(
        supremacy_castle, aoe::Technology::supremacy
    ));
    for (int tick = 0; tick < 20; ++tick) supremacy.update();
    require(supremacy.maximum_hit_points(supremacy.units()[0]) == 65);
    require(supremacy.units()[0].hit_points == 65);
    require(supremacy.units()[0].attack == villager_base_attack + 6);
    require(supremacy.melee_armor(supremacy.units()[0]) == 2);
    require(supremacy.pierce_armor(supremacy.units()[0]) == 2);

    aoe::Simulation shinkichon(aoe::GameMap(16, 8));
    shinkichon.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {2, 3}
    );
    const int mangonel_range =
        shinkichon.effective_attack_range(shinkichon.units()[0]);
    const int mangonel_vision =
        shinkichon.effective_unit_vision_range(shinkichon.units()[0]);
    shinkichon.replace_technologies(
        aoe::Player::blue, {aoe::Technology::shinkichon}
    );
    require(shinkichon.effective_attack_range(
        shinkichon.units()[0]
    ) == mangonel_range + 1);
    require(shinkichon.effective_unit_vision_range(
        shinkichon.units()[0]
    ) == mangonel_vision + 1);
    require(shinkichon.units()[0].attack ==
            aoe::rules_for(aoe::UnitKind::mangonel).attack);

    aoe::Simulation future_effects(aoe::GameMap(10, 8));
    future_effects.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::atheism,
            aoe::Technology::el_dorado,
        }
    );
    const auto future_save =
        std::filesystem::temp_directory_path() / "aoe-future-tech.save";
    aoe::save_game(future_effects, future_save);
    const aoe::Simulation restored_future =
        aoe::load_game(future_save);
    std::filesystem::remove(future_save);
    require(restored_future.has_technology(
        aoe::Player::blue, aoe::Technology::atheism
    ));
    require(restored_future.has_technology(
        aoe::Player::blue, aoe::Technology::el_dorado
    ));

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::koreans;
    scenario.red_civilization = aoe::Civilization::mayans;
    scenario.blue_technologies = {aoe::Technology::shinkichon};
    scenario.red_technologies = {aoe::Technology::el_dorado};
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-unique-tech.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const aoe::Scenario restored_scenario =
        aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(restored_scenario.blue_technologies[0] ==
            aoe::Technology::shinkichon);
    require(restored_scenario.red_technologies[0] ==
            aoe::Technology::el_dorado);

    aoe::Replay replay;
    replay.record(
        0, aoe::ResearchTechnologyCommand{
            1, aoe::Technology::supremacy
        }
    );
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-unique-tech.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay restored_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(restored_replay.commands().size() == 1);
    require(std::get<aoe::ResearchTechnologyCommand>(
        restored_replay.commands()[0].command
    ).technology == aoe::Technology::supremacy);
}

void conquerors_siege_eagles_and_trebuchets_are_vertical() {
    require(aoe::rules_for(aoe::UnitKind::eagle_warrior).hit_points == 50);
    require(aoe::rules_for(aoe::UnitKind::heavy_scorpion).attack == 16);
    require(aoe::rules_for(aoe::UnitKind::onager).splash_radius == 1);
    require(aoe::rules_for(aoe::UnitKind::siege_onager).attack == 75);
    require(aoe::rules_for(aoe::UnitKind::trebuchet).attack_range == 16);
    require(aoe::rules_for(aoe::Technology::siege_onager).food_cost == 1450);

    aoe::Simulation effects(aoe::GameMap(24, 10));
    effects.add_unit(
        aoe::UnitKind::elite_eagle_warrior,
        aoe::Player::blue, {2, 2}
    );
    effects.add_unit(
        aoe::UnitKind::heavy_scorpion,
        aoe::Player::blue, {2, 4}
    );
    effects.add_unit(
        aoe::UnitKind::siege_onager,
        aoe::Player::blue, {2, 6}
    );
    effects.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::el_dorado,
            aoe::Technology::rocketry,
            aoe::Technology::shinkichon,
            aoe::Technology::drill,
        }
    );
    require(effects.maximum_hit_points(effects.units()[0]) == 100);
    effects.add_unit(
        aoe::UnitKind::scorpion, aoe::Player::blue, {3, 4}
    );
    require(effects.units()[1].attack == 20);
    effects.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::el_dorado,
            aoe::Technology::rocketry,
            aoe::Technology::shinkichon,
            aoe::Technology::drill,
        }
    );
    require(effects.effective_attack_range(effects.units()[2]) == 9);
    require(effects.effective_siege_movement_numerator(
        effects.units()[1]
    ) == 97);

    aoe::Simulation transform(aoe::GameMap(24, 10));
    const auto trebuchet = transform.add_unit(
        aoe::UnitKind::packed_trebuchet,
        aoe::Player::blue, {3, 4}
    );
    transform.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {18, 3}
    );
    require(transform.command_pack_trebuchet(trebuchet, false));
    require(transform.units()[0].kind == aoe::UnitKind::packed_trebuchet);
    require(transform.units()[0].trebuchet_transform_ticks_remaining == 2);
    require(!transform.command_unit(trebuchet, {5, 4}));
    transform.update();
    const auto transform_save =
        std::filesystem::temp_directory_path() / "aoe-treb-transform.save";
    aoe::save_game(transform, transform_save);
    aoe::Simulation restored = aoe::load_game(transform_save);
    std::filesystem::remove(transform_save);
    require(restored.units()[0].trebuchet_transform_ticks_remaining == 1);
    restored.update();
    require(restored.units()[0].kind == aoe::UnitKind::trebuchet);
    require(!restored.command_unit(trebuchet, {5, 4}));

    require(restored.command_unit(trebuchet, {18, 3}));
    for (int tick = 0; tick < 5; ++tick) restored.update();
    require(!restored.projectiles().empty());
    require(restored.projectiles()[0].source_kind ==
            aoe::UnitKind::trebuchet);
    while (!restored.projectiles().empty()) restored.update();
    require(!restored.impact_effects().empty());
    require(restored.impact_effects().back().source_kind ==
            aoe::UnitKind::trebuchet);

    aoe::Replay replay;
    replay.record(0, aoe::PackTrebuchetCommand{trebuchet, true});
    const auto replay_path =
        std::filesystem::temp_directory_path() / "aoe-treb-pack.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay replay_copy = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(std::get<aoe::PackTrebuchetCommand>(
        replay_copy.commands()[0].command
    ).pack);
}

void cavalry_archer_line_is_dat_backed_and_persistent() {
    const auto& base = aoe::rules_for(aoe::UnitKind::cavalry_archer);
    const auto& elite =
        aoe::rules_for(aoe::UnitKind::heavy_cavalry_archer);
    require(base.hit_points == 50 && base.attack == 6);
    require(base.attack_range == 4 && base.accuracy_percent == 50);
    require(base.wood_cost == 40 && base.gold_cost == 70);
    require(base.training_ticks == 14 && base.vision_range == 5);
    require(elite.hit_points == 60 && elite.attack == 7);
    require(elite.melee_armor == 1 && elite.training_ticks == 11);
    const auto& upgrade =
        aoe::rules_for(aoe::Technology::heavy_cavalry_archer);
    require(upgrade.food_cost == 900 && upgrade.gold_cost == 500);
    require(upgrade.research_ticks == 17);
    require(!aoe::civilization_has_unit(
        aoe::Civilization::aztecs, aoe::UnitKind::cavalry_archer
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::teutons,
        aoe::Technology::heavy_cavalry_archer
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::huns,
        aoe::Technology::heavy_cavalry_archer
    ));

    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const auto range = simulation.add_building(
        aoe::BuildingKind::archery_range, aoe::Player::blue, {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    simulation.add_unit(
        aoe::UnitKind::cavalry_archer, aoe::Player::blue, {6, 4}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    require(simulation.research_technology_at(
        range, aoe::Technology::heavy_cavalry_archer
    ));
    for (int tick = 0; tick < 17; ++tick) simulation.update();
    require(simulation.units()[0].kind ==
            aoe::UnitKind::heavy_cavalry_archer);
    require(simulation.units()[0].hit_points == 60);
    require(simulation.queue_unit_at(
        range, aoe::UnitKind::cavalry_archer
    ));
    require(simulation.buildings()[0].production_queue.back().kind ==
            aoe::UnitKind::heavy_cavalry_archer);
    require(simulation.unique_unit_movement_numerator(
        simulation.units()[0]
    ) == 140);

    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::heavy_cavalry_archer,
            aoe::Technology::bloodlines,
            aoe::Technology::husbandry,
            aoe::Technology::fletching,
            aoe::Technology::padded_archer_armor,
        }
    );
    simulation.add_unit(
        aoe::UnitKind::heavy_cavalry_archer,
        aoe::Player::blue, {7, 4}
    );
    require(simulation.maximum_hit_points(simulation.units().back()) == 80);
    require(simulation.unique_unit_movement_numerator(
        simulation.units().back()
    ) == 154);
    require(simulation.units().back().attack == 8);
    require(simulation.melee_armor(simulation.units().back()) == 2);
    require(simulation.pierce_armor(simulation.units().back()) == 1);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-cavalry-archer.save";
    aoe::save_game(simulation, save_path);
    const aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::heavy_cavalry_archer
    ));
    require(restored.units().back().kind ==
            aoe::UnitKind::heavy_cavalry_archer);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::huns;
    scenario.blue_technologies = {
        aoe::Technology::heavy_cavalry_archer
    };
    scenario.units = {{
        aoe::UnitKind::heavy_cavalry_archer,
        aoe::Player::blue, {2, 2}
    }};
    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-cavalry-archer.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto restored_scenario = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(restored_scenario.units[0].kind ==
            aoe::UnitKind::heavy_cavalry_archer);
}

void camel_line_is_exact_counter_and_persistent() {
    const auto& camel = aoe::rules_for(aoe::UnitKind::camel_rider);
    const auto& heavy = aoe::rules_for(aoe::UnitKind::heavy_camel);
    require(camel.hit_points == 100 && camel.attack == 5);
    require(camel.bonus_vs_cavalry == 10);
    require(camel.food_cost == 55 && camel.gold_cost == 60);
    require(camel.training_ticks == 9 && camel.vision_range == 4);
    require(heavy.hit_points == 120 && heavy.attack == 7);
    require(heavy.bonus_vs_cavalry == 18 && heavy.vision_range == 5);
    const auto& upgrade = aoe::rules_for(aoe::Technology::heavy_camel);
    require(upgrade.food_cost == 325 && upgrade.gold_cost == 360);
    require(upgrade.research_ticks == 42);
    require(aoe::civilization_has_unit(
        aoe::Civilization::saracens, aoe::UnitKind::camel_rider
    ));
    require(!aoe::civilization_has_unit(
        aoe::Civilization::franks, aoe::UnitKind::camel_rider
    ));

    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const auto stable = simulation.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {1, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::camel_rider, aoe::Player::blue, {6, 4}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::saracens
    ));
    require(simulation.research_technology_at(
        stable, aoe::Technology::heavy_camel
    ));
    for (int tick = 0; tick < 42; ++tick) simulation.update();
    require(simulation.units()[0].kind == aoe::UnitKind::heavy_camel);
    require(simulation.units()[0].hit_points == 120);
    require(simulation.queue_unit_at(stable, aoe::UnitKind::camel_rider));
    require(simulation.buildings()[0].production_queue.back().kind ==
            aoe::UnitKind::heavy_camel);

    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::heavy_camel,
            aoe::Technology::bloodlines,
            aoe::Technology::husbandry,
            aoe::Technology::forging,
            aoe::Technology::scale_barding_armor,
            aoe::Technology::zealotry,
        }
    );
    simulation.add_unit(
        aoe::UnitKind::heavy_camel, aoe::Player::blue, {7, 4}
    );
    require(simulation.maximum_hit_points(simulation.units().back()) == 170);
    require(simulation.has_technology(
        aoe::Player::blue,
        aoe::Technology::husbandry
    ));
    require(simulation.units().back().attack == 8);
    require(simulation.melee_armor(simulation.units().back()) == 1);
    require(simulation.pierce_armor(simulation.units().back()) == 1);

    aoe::Simulation counter(aoe::GameMap(12, 8));
    const auto attacker = counter.add_unit(
        aoe::UnitKind::heavy_camel, aoe::Player::blue, {3, 3}
    );
    counter.add_unit(
        aoe::UnitKind::knight, aoe::Player::red, {4, 3}
    );
    counter.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 6}
    );
    counter.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 1}
    );
    require(counter.command_unit(attacker, {4, 3}));
    counter.update();
    require(counter.units()[1].hit_points ==
            aoe::rules_for(aoe::UnitKind::knight).hit_points -
                (heavy.attack - aoe::rules_for(aoe::UnitKind::knight).melee_armor +
                 heavy.bonus_vs_cavalry));

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-heavy-camel.save";
    aoe::save_game(simulation, save_path);
    const aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::heavy_camel
    ));
    require(restored.units().back().kind == aoe::UnitKind::heavy_camel);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::saracens;
    scenario.blue_technologies = {aoe::Technology::heavy_camel};
    scenario.units = {{
        aoe::UnitKind::heavy_camel, aoe::Player::blue, {2, 2}
    }};
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-heavy-camel.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[0].kind == aoe::UnitKind::heavy_camel);
}

void ram_upgrades_are_exact_splashing_and_persistent() {
    const auto& capped = aoe::rules_for(aoe::UnitKind::capped_ram);
    const auto& siege = aoe::rules_for(aoe::UnitKind::siege_ram);
    require(capped.hit_points == 200 && capped.attack == 3);
    require(capped.bonus_vs_buildings == 150);
    require(capped.pierce_armor == 190 && capped.splash_radius == 1);
    require(capped.wood_cost == 160 && capped.gold_cost == 75);
    require(capped.training_ticks == 18 && capped.vision_range == 3);
    require(siege.hit_points == 270 && siege.attack == 4);
    require(siege.bonus_vs_buildings == 200);
    require(siege.pierce_armor == 195 && siege.splash_radius == 2);
    require(aoe::rules_for(aoe::Technology::capped_ram).food_cost == 300);
    require(aoe::rules_for(aoe::Technology::capped_ram).research_ticks == 17);
    require(aoe::rules_for(aoe::Technology::siege_ram).food_cost == 1000);
    require(aoe::rules_for(aoe::Technology::siege_ram).research_ticks == 25);
    require(aoe::civilization_has_technology(
        aoe::Civilization::chinese, aoe::Technology::siege_ram
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::franks, aoe::Technology::siege_ram
    ));

    aoe::Simulation simulation(aoe::GameMap(24, 12));
    const auto research_workshop = simulation.add_building(
        aoe::BuildingKind::siege_workshop, aoe::Player::blue, {1, 1}
    );
    const auto production_workshop = simulation.add_building(
        aoe::BuildingKind::siege_workshop, aoe::Player::blue, {5, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {16, 8}
    );
    const auto existing = simulation.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {4, 7}
    );
    auto damaged_units = simulation.units();
    damaged_units.back().hit_points = 100;
    simulation.replace_state(
        std::move(damaged_units), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::chinese
    ));
    require(!simulation.research_technology_at(
        research_workshop, aoe::Technology::siege_ram
    ));
    require(simulation.research_technology_at(
        research_workshop, aoe::Technology::capped_ram
    ));
    require(simulation.queue_unit_at(
        production_workshop, aoe::UnitKind::battering_ram
    ));
    for (int tick = 0; tick < 17; ++tick) simulation.update();
    require(simulation.units()[0].id == existing);
    require(simulation.units()[0].kind == aoe::UnitKind::capped_ram);
    require(simulation.units()[0].hit_points == 125);
    require(simulation.buildings()[1].production_queue[0].kind ==
            aoe::UnitKind::capped_ram);

    require(simulation.research_technology_at(
        research_workshop, aoe::Technology::siege_ram
    ));
    for (int tick = 0; tick < 25; ++tick) simulation.update();
    require(simulation.units()[0].kind == aoe::UnitKind::siege_ram);
    require(simulation.units()[0].hit_points == 195);
    require(std::ranges::all_of(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.owner != aoe::Player::blue ||
                   unit.kind == aoe::UnitKind::siege_ram;
        }
    ));
    require(simulation.queue_unit_at(
        production_workshop, aoe::UnitKind::battering_ram
    ));
    require(simulation.buildings()[1].production_queue.back().kind ==
            aoe::UnitKind::siege_ram);
    require(simulation.effective_siege_movement_numerator(
        simulation.units()[0]
    ) == 60);
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::capped_ram,
            aoe::Technology::siege_ram,
            aoe::Technology::drill,
        }
    );
    require(simulation.effective_siege_movement_numerator(
        simulation.units()[0]
    ) == 90);

    aoe::Simulation splash(aoe::GameMap(14, 8));
    const auto ram = splash.add_unit(
        aoe::UnitKind::siege_ram, aoe::Player::blue, {2, 3}
    );
    splash.add_building(
        aoe::BuildingKind::palisade_wall, aoe::Player::red, {3, 3}
    );
    splash.add_building(
        aoe::BuildingKind::palisade_wall, aoe::Player::red, {4, 3}
    );
    splash.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 6}
    );
    const int adjacent_before = splash.buildings()[1].hit_points;
    require(splash.command_unit(ram, {3, 3}));
    splash.update();
    require(splash.buildings()[0].hit_points <
            aoe::rules_for(aoe::BuildingKind::palisade_wall).hit_points);
    require(splash.buildings()[1].hit_points < adjacent_before);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-siege-ram.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::siege_ram
    ));
    require(restored.units()[0].id == existing);
    require(restored.units()[0].kind == aoe::UnitKind::siege_ram);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::chinese;
    scenario.blue_technologies = {
        aoe::Technology::capped_ram, aoe::Technology::siege_ram
    };
    scenario.units = {{
        aoe::UnitKind::siege_ram, aoe::Player::blue, {2, 2}
    }};
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-siege-ram.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[0].kind == aoe::UnitKind::siege_ram);
}

void halberdier_line_is_exact_and_persistent() {
    const auto& halberdier = aoe::rules_for(aoe::UnitKind::halberdier);
    require(halberdier.hit_points == 60 && halberdier.attack == 6);
    require(halberdier.attack_interval_ticks == 6);
    require(halberdier.bonus_vs_cavalry == 32);
    require(halberdier.bonus_vs_war_elephants == 28);
    require(halberdier.bonus_vs_camels == 16);
    require(halberdier.food_cost == 35 && halberdier.wood_cost == 25);
    require(halberdier.training_ticks == 12 && halberdier.vision_range == 4);
    const auto& upgrade = aoe::rules_for(aoe::Technology::halberdier);
    require(upgrade.food_cost == 300 && upgrade.gold_cost == 600);
    require(upgrade.research_ticks == 17);
    require(aoe::civilization_has_unit(
        aoe::Civilization::britons, aoe::UnitKind::halberdier
    ));
    require(!aoe::civilization_has_unit(
        aoe::Civilization::saracens, aoe::UnitKind::halberdier
    ));

    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const auto barracks = simulation.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {1, 1}
    );
    const auto production_barracks = simulation.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {5, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::pikeman, aoe::Player::blue, {6, 4}
    );
    auto damaged = simulation.units();
    damaged[0].hit_points = 30;
    auto initial_buildings = simulation.buildings();
    initial_buildings[1].production_queue.push_back({
        aoe::UnitKind::pikeman, 30, 25, 35, 0
    });
    simulation.replace_state(
        std::move(damaged), std::move(initial_buildings),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::britons
    ));
    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::pikeman}
    );
    require(simulation.research_technology_at(
        barracks, aoe::Technology::halberdier
    ));
    for (int tick = 0; tick < 17; ++tick) simulation.update();
    require(simulation.units()[0].kind == aoe::UnitKind::halberdier);
    require(simulation.units()[0].hit_points == 35);
    require(simulation.buildings()[1].production_queue[0].kind ==
            aoe::UnitKind::halberdier);
    require(simulation.queue_unit_at(
        production_barracks, aoe::UnitKind::spearman
    ));
    require(simulation.buildings()[1].production_queue.back().kind ==
            aoe::UnitKind::halberdier);

    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::pikeman,
            aoe::Technology::halberdier,
            aoe::Technology::forging,
            aoe::Technology::scale_mail_armor,
        }
    );
    simulation.add_unit(
        aoe::UnitKind::halberdier, aoe::Player::blue, {7, 4}
    );
    require(simulation.units().back().attack == 7);
    require(simulation.melee_armor(simulation.units().back()) == 1);
    require(simulation.pierce_armor(simulation.units().back()) == 1);

    aoe::Simulation japanese(aoe::GameMap(8, 8));
    japanese.add_unit(
        aoe::UnitKind::halberdier, aoe::Player::blue, {2, 2}
    );
    require(japanese.set_civilization(
        aoe::Player::blue, aoe::Civilization::japanese
    ));
    japanese.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(japanese.effective_attack_interval(
        japanese.units()[0]
    ) == 4);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-halberdier.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::halberdier
    ));
    require(restored.units()[0].kind == aoe::UnitKind::halberdier);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::britons;
    scenario.blue_technologies = {
        aoe::Technology::pikeman, aoe::Technology::halberdier
    };
    scenario.units = {{
        aoe::UnitKind::halberdier, aoe::Player::blue, {2, 2}
    }};
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-halberdier.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[0].kind == aoe::UnitKind::halberdier);
}

void chemistry_unlocks_exact_land_gunpowder() {
    const auto& chemistry = aoe::rules_for(aoe::Technology::chemistry);
    require(chemistry.food_cost == 300 && chemistry.gold_cost == 200);
    require(chemistry.research_ticks == 34);
    const auto& hand = aoe::rules_for(aoe::UnitKind::hand_cannoneer);
    require(hand.hit_points == 35 && hand.attack == 17);
    require(hand.attack_range == 7 && hand.attack_interval_ticks == 7);
    require(hand.accuracy_percent == 65 && hand.attack_frame_delay == 5);
    require(hand.movement_speed_percent == 96);
    require(hand.food_cost == 45 && hand.gold_cost == 50);
    require(hand.training_ticks == 17 && hand.bonus_vs_infantry == 10);
    const auto& cannon = aoe::rules_for(aoe::UnitKind::bombard_cannon);
    require(cannon.hit_points == 80 && cannon.attack == 40);
    require(cannon.attack_range == 12 && cannon.minimum_attack_range == 5);
    require(cannon.attack_interval_ticks == 13);
    require(cannon.accuracy_percent == 92 && cannon.attack_frame_delay == 7);
    require(cannon.movement_speed_percent == 70);
    require(cannon.splash_radius_half_tiles == 1);
    require(cannon.wood_cost == 225 && cannon.gold_cost == 225);
    require(cannon.training_ticks == 28);
    require(aoe::civilization_has_unit(
        aoe::Civilization::franks, aoe::UnitKind::hand_cannoneer
    ));
    require(aoe::civilization_has_unit(
        aoe::Civilization::franks, aoe::UnitKind::bombard_cannon
    ));
    require(aoe::civilization_has_unit(
        aoe::Civilization::japanese, aoe::UnitKind::hand_cannoneer
    ));
    require(!aoe::civilization_has_unit(
        aoe::Civilization::japanese, aoe::UnitKind::bombard_cannon
    ));

    aoe::Simulation simulation(aoe::GameMap(24, 12));
    const auto university = simulation.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {1, 1}
    );
    const auto range = simulation.add_building(
        aoe::BuildingKind::archery_range, aoe::Player::blue, {5, 1}
    );
    const auto workshop = simulation.add_building(
        aoe::BuildingKind::siege_workshop, aoe::Player::blue, {9, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 8}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 8}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::franks
    ));
    require(!simulation.queue_unit_at(range, aoe::UnitKind::hand_cannoneer));
    require(!simulation.queue_unit_at(
        workshop, aoe::UnitKind::bombard_cannon
    ));
    require(!simulation.research_technology_at(
        range, aoe::Technology::hand_cannoneer_gate
    ));
    require(simulation.research_technology_at(
        university, aoe::Technology::chemistry
    ));
    for (int tick = 0; tick < 34; ++tick) simulation.update();
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::chemistry
    ));
    require(simulation.queue_unit_at(range, aoe::UnitKind::hand_cannoneer));
    require(simulation.queue_unit_at(
        workshop, aoe::UnitKind::bombard_cannon
    ));

    aoe::Simulation blast(aoe::GameMap(16, 9));
    const auto bombard = blast.add_unit(
        aoe::UnitKind::bombard_cannon, aoe::Player::blue, {2, 3}
    );
    blast.add_building(
        aoe::BuildingKind::palisade_wall, aoe::Player::red, {8, 3}
    );
    require(blast.command_unit(bombard, {8, 3}));
    const auto same_tile = blast.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {6, 2}
    );
    const auto adjacent = blast.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {8, 4}
    );
    auto overlapping = blast.units();
    overlapping[1].position = {8, 3};
    overlapping[1].previous_position = {8, 3};
    blast.replace_state(
        std::move(overlapping), blast.buildings(),
        blast.economy(aoe::Player::blue),
        blast.economy(aoe::Player::red),
        blast.tick_number()
    );
    const int adjacent_hp = blast.units()[2].hit_points;
    for (int tick = 0; tick < 3; ++tick) blast.update();
    require(!blast.projectiles().empty());
    require(blast.projectiles()[0].splash_radius_half_tiles == 1);
    for (int tick = 1; tick < 6; ++tick) blast.update();
    require(std::ranges::none_of(
        blast.units(), [same_tile](const aoe::Unit& unit) {
            return unit.id == same_tile;
        }
    ));
    const auto adjacent_after = std::ranges::find_if(
        blast.units(), [adjacent](const aoe::Unit& unit) {
            return unit.id == adjacent;
        }
    );
    require(adjacent_after != blast.units().end());
    require(adjacent_after->hit_points == adjacent_hp);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-gunpowder.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::chemistry
    ));
    require(restored.buildings()[1].production_queue[0].kind ==
            aoe::UnitKind::hand_cannoneer);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::franks;
    scenario.blue_technologies = {aoe::Technology::chemistry};
    scenario.units = {
        {aoe::UnitKind::hand_cannoneer, aoe::Player::blue, {2, 2}},
        {aoe::UnitKind::bombard_cannon, aoe::Player::blue, {3, 2}},
    };
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-gunpowder.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[1].kind == aoe::UnitKind::bombard_cannon);
}

void broad_siege_and_production_technologies_are_bounded() {
    const auto& engineers = aoe::rules_for(aoe::Technology::siege_engineers);
    require(engineers.food_cost == 500 && engineers.wood_cost == 600);
    require(engineers.research_ticks == 15);
    const auto& conscription = aoe::rules_for(aoe::Technology::conscription);
    require(conscription.food_cost == 150 && conscription.gold_cost == 150);
    require(conscription.research_ticks == 20);
    require(aoe::civilization_has_technology(
        aoe::Civilization::britons, aoe::Technology::siege_engineers
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::chinese, aoe::Technology::siege_engineers
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::chinese, aoe::Technology::conscription
    ));

    aoe::Simulation siege(aoe::GameMap(20, 10));
    siege.add_unit(
        aoe::UnitKind::capped_ram, aoe::Player::blue, {2, 2}
    );
    const auto mangonel = siege.add_unit(
        aoe::UnitKind::mangonel, aoe::Player::blue, {2, 4}
    );
    const auto bombard = siege.add_unit(
        aoe::UnitKind::bombard_cannon, aoe::Player::blue, {2, 6}
    );
    siege.add_building(
        aoe::BuildingKind::palisade_wall, aoe::Player::red, {8, 6}
    );
    siege.replace_technologies(
        aoe::Player::blue, {aoe::Technology::siege_engineers}
    );
    require(siege.effective_attack_range(siege.units()[0]) == 1);
    require(siege.effective_unit_vision_range(siege.units()[0]) == 4);
    require(siege.effective_attack_range(siege.units()[1]) ==
            aoe::rules_for(aoe::UnitKind::mangonel).attack_range + 1);
    require(siege.effective_unit_vision_range(siege.units()[1]) ==
            aoe::rules_for(aoe::UnitKind::mangonel).vision_range + 1);
    require(siege.command_unit(bombard, {8, 6}));
    for (int tick = 0; tick < 3; ++tick) siege.update();
    require(!siege.projectiles().empty());
    const auto bombard_projectile = std::ranges::find_if(
        siege.projectiles(), [](const aoe::Projectile& projectile) {
            return projectile.source_kind == aoe::UnitKind::bombard_cannon;
        }
    );
    require(bombard_projectile != siege.projectiles().end());
    require(bombard_projectile->damage ==
            aoe::rules_for(aoe::UnitKind::bombard_cannon).attack +
                aoe::rules_for(aoe::UnitKind::bombard_cannon)
                    .bonus_vs_buildings * 120 / 100);
    (void)mangonel;

    aoe::Simulation production(aoe::GameMap(28, 14));
    const auto barracks = production.add_building(
        aoe::BuildingKind::barracks, aoe::Player::blue, {1, 1}
    );
    const auto range = production.add_building(
        aoe::BuildingKind::archery_range, aoe::Player::blue, {5, 1}
    );
    const auto stable = production.add_building(
        aoe::BuildingKind::stable, aoe::Player::blue, {9, 1}
    );
    const auto castle = production.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {13, 1}
    );
    const auto workshop = production.add_building(
        aoe::BuildingKind::siege_workshop, aoe::Player::blue, {19, 1}
    );
    production.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 10}
    );
    production.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {5, 10}
    );
    production.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {24, 10}
    );
    production.replace_state(
        production.units(), production.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    production.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(production.queue_unit_at(barracks, aoe::UnitKind::militia));
    require(production.queue_unit_at(range, aoe::UnitKind::archer));
    require(production.queue_unit_at(stable, aoe::UnitKind::knight));
    require(production.queue_unit_at(castle, aoe::UnitKind::trebuchet));
    require(production.queue_unit_at(workshop, aoe::UnitKind::mangonel));
    production.replace_technologies(
        aoe::Player::blue, {aoe::Technology::conscription}
    );
    const auto future_range = production.add_building(
        aoe::BuildingKind::archery_range, aoe::Player::blue, {9, 9}
    );
    require(production.queue_unit_at(
        future_range, aoe::UnitKind::archer
    ));
    const int future_before =
        production.buildings().back().production_queue[0].ticks_remaining;
    const std::array<int, 5> before{
        production.buildings()[0].production_queue[0].ticks_remaining,
        production.buildings()[1].production_queue[0].ticks_remaining,
        production.buildings()[2].production_queue[0].ticks_remaining,
        production.buildings()[3].production_queue[0].ticks_remaining,
        production.buildings()[4].production_queue[0].ticks_remaining,
    };
    for (int tick = 0; tick < 4; ++tick) production.update();
    for (int index = 0; index < 4; ++index) {
        require(production.buildings()[index]
                    .production_queue[0].ticks_remaining ==
                before[index] - 5);
    }
    require(production.buildings()[4]
                .production_queue[0].ticks_remaining == before[4] - 4);
    require(production.buildings().back()
                .production_queue[0].ticks_remaining == future_before - 5);

    aoe::Simulation timer_boundary(aoe::GameMap(14, 8));
    const auto timer_university = timer_boundary.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {1, 1}
    );
    timer_boundary.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {10, 5}
    );
    timer_boundary.replace_state(
        timer_boundary.units(), timer_boundary.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    timer_boundary.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    timer_boundary.replace_technologies(
        aoe::Player::blue, {aoe::Technology::conscription}
    );
    require(timer_boundary.research_technology_at(
        timer_university, aoe::Technology::chemistry
    ));
    for (int tick = 0; tick < 4; ++tick) timer_boundary.update();
    require(timer_boundary.buildings()[0]
                .technology_research_ticks_remaining == 30);

    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-conscription.save";
    aoe::save_game(production, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::conscription
    ));
    require(restored.buildings()[0].production_queue[0].work_remainder ==
            production.buildings()[0].production_queue[0].work_remainder);
}

void petards_unlock_and_explode_deterministically() {
    const auto& rules = aoe::rules_for(aoe::UnitKind::petard);
    require(rules.hit_points == 50 && rules.attack == 25);
    require(rules.attack_interval_ticks == 10);
    require(rules.movement_speed_percent == 80);
    require(rules.food_cost == 80 && rules.gold_cost == 20);
    require(rules.training_ticks == 13 && rules.vision_range == 4);
    require(rules.pierce_armor == 2);
    require(rules.bonus_vs_buildings == 500);
    require(rules.bonus_vs_siege == 60);
    require(rules.bonus_vs_walls == 900);
    require(rules.splash_radius_half_tiles == 1);
    require(aoe::civilization_has_unit(
        aoe::Civilization::mayans, aoe::UnitKind::petard
    ));

    aoe::Simulation unlock(aoe::GameMap(16, 9));
    const auto castle = unlock.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {1, 1}
    );
    unlock.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    unlock.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 6}
    );
    unlock.replace_state(
        unlock.units(), unlock.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    unlock.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(!unlock.has_technology(
        aoe::Player::blue, aoe::Technology::petard_gate
    ));
    require(!unlock.queue_unit_at(castle, aoe::UnitKind::petard));
    unlock.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(unlock.has_technology(
        aoe::Player::blue, aoe::Technology::petard_gate
    ));
    require(!unlock.research_technology_at(
        castle, aoe::Technology::petard_gate
    ));
    require(unlock.queue_unit_at(castle, aoe::UnitKind::petard));

    aoe::Simulation blast(aoe::GameMap(14, 8));
    const auto petard = blast.add_unit(
        aoe::UnitKind::petard, aoe::Player::blue, {4, 3}
    );
    const auto wall = blast.add_building(
        aoe::BuildingKind::palisade_wall, aoe::Player::red, {5, 3}
    );
    const auto same_tile = blast.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {4, 1}
    );
    const auto adjacent = blast.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {5, 4}
    );
    require(blast.command_unit(petard, {5, 3}));
    auto overlap = blast.units();
    overlap[1].position = {5, 3};
    overlap[1].previous_position = {5, 3};
    blast.replace_state(
        std::move(overlap), blast.buildings(),
        blast.economy(aoe::Player::blue),
        blast.economy(aoe::Player::red),
        blast.tick_number()
    );
    blast.update();
    require(std::ranges::none_of(
        blast.units(), [petard](const aoe::Unit& unit) {
            return unit.id == petard;
        }
    ));
    const auto same_after = std::ranges::find_if(
        blast.units(), [same_tile](const aoe::Unit& unit) {
            return unit.id == same_tile;
        }
    );
    const auto adjacent_after = std::ranges::find_if(
        blast.units(), [adjacent](const aoe::Unit& unit) {
            return unit.id == adjacent;
        }
    );
    require(same_after != blast.units().end());
    require(same_after->hit_points == 15);
    require(adjacent_after != blast.units().end());
    require(adjacent_after->hit_points == 40);
    require(std::ranges::none_of(
        blast.buildings(), [wall](const aoe::Building& building) {
            return building.id == wall;
        }
    ));
    require(!blast.impact_effects().empty());
    require(blast.impact_effects()[0].source_kind == aoe::UnitKind::petard);
    require(!blast.death_effects().empty());

    const auto wall_damage = [](bool engineers) {
        aoe::Simulation simulation(aoe::GameMap(12, 7));
        const auto attacker = simulation.add_unit(
            aoe::UnitKind::petard, aoe::Player::blue, {3, 3}
        );
        simulation.add_building(
            aoe::BuildingKind::stone_wall, aoe::Player::red, {4, 3}
        );
        if (engineers) {
            simulation.replace_technologies(
                aoe::Player::blue, {aoe::Technology::siege_engineers}
            );
        }
        const int before = simulation.buildings()[0].hit_points;
        require(simulation.command_unit(attacker, {4, 3}));
        simulation.update();
        return before - simulation.buildings()[0].hit_points;
    };
    require(wall_damage(true) == wall_damage(false) + 200);

    aoe::Simulation persisted(aoe::GameMap(18, 8));
    const auto moving_petard = persisted.add_unit(
        aoe::UnitKind::petard, aoe::Player::blue, {2, 3}
    );
    persisted.add_building(
        aoe::BuildingKind::stone_wall, aoe::Player::red, {12, 3}
    );
    require(persisted.command_unit(moving_petard, {12, 3}));
    const auto save_path =
        std::filesystem::temp_directory_path() / "aoe-petard.save";
    aoe::save_game(persisted, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.units()[0].kind == aoe::UnitKind::petard);
    require(restored.units()[0].attack_target_is_building);

    aoe::Scenario scenario(10, 8);
    scenario.blue_civilization = aoe::Civilization::mayans;
    scenario.blue_age = aoe::Age::castle;
    scenario.units = {{
        aoe::UnitKind::petard, aoe::Player::blue, {2, 2}
    }};
    const auto scenario_path =
        std::filesystem::temp_directory_path() / "aoe-petard.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.units[0].kind == aoe::UnitKind::petard);
}

void bombard_towers_are_exact_unlocked_defenses() {
    const auto& building_rules =
        aoe::rules_for(aoe::BuildingKind::bombard_tower);
    require(building_rules.hit_points == 2220);
    require(building_rules.melee_armor == 3);
    require(building_rules.pierce_armor == 9);
    require(building_rules.gold_cost == 100);
    require(building_rules.stone_cost == 125);
    require(building_rules.construction_ticks == 16);
    require(building_rules.vision_range == 10);
    require(building_rules.footprint_width == 1);
    require(building_rules.footprint_height == 1);
    require(building_rules.attack == 120);
    require(building_rules.bonus_vs_camels == 40);
    require(building_rules.attack_range == 8);
    require(building_rules.minimum_attack_range == 1);
    require(building_rules.attack_interval_ticks == 12);
    require(building_rules.accuracy_percent == 92);
    require(building_rules.projectile_speed_tenths == 30);
    const auto& technology_rules =
        aoe::rules_for(aoe::Technology::bombard_tower);
    require(technology_rules.researched_at ==
            aoe::BuildingKind::university);
    require(technology_rules.minimum_age == aoe::Age::imperial);
    require(technology_rules.food_cost == 800);
    require(technology_rules.wood_cost == 400);
    require(technology_rules.stone_cost == 0);
    require(technology_rules.research_ticks == 20);

    aoe::Simulation unlock(aoe::GameMap(24, 12));
    const auto university = unlock.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {5, 5}
    );
    const auto villager = unlock.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    unlock.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 8}
    );
    unlock.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 8}
    );
    unlock.replace_state(
        unlock.units(), unlock.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    require(unlock.set_civilization(
        aoe::Player::blue, aoe::Civilization::byzantines
    ));
    unlock.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(!unlock.research_technology_at(
        university, aoe::Technology::bombard_tower
    ));
    unlock.replace_technologies(
        aoe::Player::blue, {aoe::Technology::chemistry}
    );
    require(unlock.research_technology_at(
        university, aoe::Technology::bombard_tower
    ));
    for (int tick = 0; tick < 20; ++tick) unlock.update();
    require(unlock.has_technology(
        aoe::Player::blue, aoe::Technology::bombard_tower
    ));
    const auto before = unlock.economy(aoe::Player::blue);
    require(unlock.construct_building_at(
        villager, aoe::BuildingKind::bombard_tower, {3, 2}
    ));
    require(unlock.economy(aoe::Player::blue).gold ==
            before.gold - 100);
    require(unlock.economy(aoe::Player::blue).stone ==
            before.stone - 125);

    aoe::Simulation turks(aoe::GameMap(12, 8));
    const auto turk_university = turks.add_building(
        aoe::BuildingKind::university, aoe::Player::blue, {1, 1}
    );
    turks.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 5}
    );
    turks.replace_state(
        turks.units(), turks.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    require(turks.set_civilization(
        aoe::Player::blue, aoe::Civilization::turks
    ));
    turks.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(turks.research_technology_at(
        turk_university, aoe::Technology::bombard_tower
    ));

    aoe::Simulation defense(aoe::GameMap(18, 10));
    defense.add_building(
        aoe::BuildingKind::bombard_tower, aoe::Player::blue, {2, 2}
    );
    defense.add_unit(
        aoe::UnitKind::camel_rider, aoe::Player::red, {6, 2}
    );
    defense.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    defense.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {15, 7}
    );
    defense.replace_technologies(
        aoe::Player::blue, {aoe::Technology::chemistry}
    );
    require(defense.effective_building_attack(
        defense.buildings().front()
    ) == 120);
    defense.update();
    require(defense.projectiles().size() == 1);
    const auto& projectile = defense.projectiles().front();
    require(projectile.damage == 160);
    require(projectile.source_is_building);
    require(projectile.source_building_kind ==
            aoe::BuildingKind::bombard_tower);
    require(projectile.projectile_speed_tenths == 30);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-bombard-tower.save";
    aoe::save_game(defense, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.buildings().front().kind ==
            aoe::BuildingKind::bombard_tower);
    require(restored.projectiles().front().source_is_building);
    require(restored.projectiles().front().source_building_kind ==
            aoe::BuildingKind::bombard_tower);
    require(restored.projectiles().front().projectile_speed_tenths == 30);
}

void missionary_dat_metadata_and_bounded_religious_effects() {
    const auto& missionary = aoe::rules_for(aoe::UnitKind::missionary);
    require(missionary.hit_points == 30);
    require(missionary.gold_cost == 100);
    require(missionary.training_ticks == 17);
    require(missionary.attack_range == 7);
    require(missionary.vision_range == 9);
    require(missionary.movement_speed_percent == 110);
    require(aoe::civilization_has_unit(
        aoe::Civilization::spanish, aoe::UnitKind::missionary
    ));
    require(!aoe::civilization_has_unit(
        aoe::Civilization::aztecs, aoe::UnitKind::missionary
    ));
    const std::array exact{
        std::tuple{aoe::Technology::sanctity, aoe::Age::castle, 0, 120, 20},
        std::tuple{aoe::Technology::fervor, aoe::Age::castle, 0, 140, 17},
        std::tuple{aoe::Technology::redemption, aoe::Age::castle, 0, 475, 17},
        std::tuple{aoe::Technology::atonement, aoe::Age::castle, 0, 325, 14},
        std::tuple{aoe::Technology::illumination, aoe::Age::imperial, 0, 120, 22},
        std::tuple{aoe::Technology::block_printing, aoe::Age::imperial, 0, 200, 19},
        std::tuple{aoe::Technology::faith, aoe::Age::imperial, 750, 1000, 20},
        std::tuple{aoe::Technology::theocracy, aoe::Age::imperial, 0, 200, 25},
        std::tuple{aoe::Technology::heresy, aoe::Age::castle, 0, 1000, 20},
    };
    for (const auto& [technology, age, food, gold, ticks] : exact) {
        const auto& rules = aoe::rules_for(technology);
        require(rules.researched_at == aoe::BuildingKind::monastery);
        require(rules.minimum_age == age);
        require(rules.food_cost == food);
        require(rules.gold_cost == gold);
        require(rules.research_ticks == ticks);
    }

    aoe::Simulation simulation(aoe::GameMap(20, 10));
    const auto monastery = simulation.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {1, 1}
    );
    const auto missionary_id = simulation.add_unit(
        aoe::UnitKind::missionary, aoe::Player::blue, {5, 3}
    );
    const auto enemy_monk = simulation.add_unit(
        aoe::UnitKind::monk, aoe::Player::red, {9, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 7}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 7}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    require(simulation.set_civilization(
        aoe::Player::blue, aoe::Civilization::spanish
    ));
    simulation.replace_ages(aoe::Age::imperial, aoe::Age::imperial);
    require(simulation.queue_unit_at(
        monastery, aoe::UnitKind::missionary
    ));
    require(!simulation.command_collect_relic(missionary_id, enemy_monk));
    require(!simulation.command_convert(missionary_id, enemy_monk));
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::sanctity,
            aoe::Technology::fervor,
            aoe::Technology::atonement,
            aoe::Technology::block_printing,
            aoe::Technology::illumination,
        }
    );
    require(simulation.maximum_hit_points(simulation.units()[0]) == 45);
    require(simulation.effective_unit_vision_range(
        simulation.units()[0]
    ) == 12);
    require(simulation.command_convert(missionary_id, enemy_monk));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-missionary.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.units()[0].kind == aoe::UnitKind::missionary);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::block_printing
    ));
}

void economy_technology_metadata_and_bounded_rates() {
    const std::array exact{
        std::tuple{aoe::Technology::heavy_plow, aoe::BuildingKind::mill,
                   aoe::Age::castle, 125, 125, 14},
        std::tuple{aoe::Technology::crop_rotation, aoe::BuildingKind::mill,
                   aoe::Age::imperial, 250, 250, 24},
        std::tuple{aoe::Technology::bow_saw,
                   aoe::BuildingKind::lumber_camp,
                   aoe::Age::castle, 100, 150, 17},
        std::tuple{aoe::Technology::two_man_saw,
                   aoe::BuildingKind::lumber_camp,
                   aoe::Age::imperial, 200, 300, 34},
        std::tuple{aoe::Technology::gold_mining,
                   aoe::BuildingKind::mining_camp,
                   aoe::Age::feudal, 75, 100, 10},
        std::tuple{aoe::Technology::gold_shaft_mining,
                   aoe::BuildingKind::mining_camp,
                   aoe::Age::castle, 150, 200, 25},
        std::tuple{aoe::Technology::stone_mining,
                   aoe::BuildingKind::mining_camp,
                   aoe::Age::feudal, 75, 100, 10},
        std::tuple{aoe::Technology::stone_shaft_mining,
                   aoe::BuildingKind::mining_camp,
                   aoe::Age::castle, 150, 200, 25},
        std::tuple{aoe::Technology::hand_cart,
                   aoe::BuildingKind::town_center,
                   aoe::Age::castle, 200, 300, 19},
    };
    for (const auto& [technology, location, age, wood, food, ticks] : exact) {
        const auto& rules = aoe::rules_for(technology);
        require(rules.researched_at == location);
        require(rules.minimum_age == age);
        require(rules.wood_cost == wood);
        require(rules.food_cost == food);
        require(rules.research_ticks == ticks);
    }
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    require(simulation.farm_capacity(aoe::Player::blue) == 175);
    simulation.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::horse_collar,
            aoe::Technology::heavy_plow,
            aoe::Technology::crop_rotation,
            aoe::Technology::double_bit_axe,
            aoe::Technology::bow_saw,
            aoe::Technology::two_man_saw,
            aoe::Technology::gold_mining,
            aoe::Technology::gold_shaft_mining,
            aoe::Technology::stone_mining,
            aoe::Technology::stone_shaft_mining,
            aoe::Technology::wheelbarrow,
            aoe::Technology::hand_cart,
        }
    );
    require(simulation.farm_capacity(aoe::Player::blue) == 550);
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    require(simulation.unique_unit_movement_numerator(
        simulation.units().back()
    ) == 121);
    aoe::Unit carry = simulation.units().back();
    require(simulation.effective_carry_capacity(carry) == 18);
    aoe::Simulation wheel(aoe::GameMap(4, 4));
    wheel.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    wheel.replace_technologies(
        aoe::Player::blue, {aoe::Technology::wheelbarrow}
    );
    require(wheel.effective_carry_capacity(wheel.units().front()) == 12);
    const auto movement_sample = [](bool hand_cart) {
        aoe::Simulation sample(aoe::GameMap(140, 4));
        sample.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
        );
        sample.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {139, 3}
        );
        if (hand_cart) {
            sample.replace_technologies(
                aoe::Player::blue,
                {
                    aoe::Technology::wheelbarrow,
                    aoe::Technology::hand_cart,
                }
            );
        }
        require(sample.command_unit(1, {130, 1}));
        for (int tick = 0; tick < 100; ++tick) sample.update();
        return sample.units().front().position.x;
    };
    require(movement_sample(false) == 51);
    require(movement_sample(true) == 61);
    const auto farm = simulation.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {3, 3}
    );
    carry.carried_resource = aoe::ResourceKind::food;
    carry.has_resource_target = true;
    carry.resource_building_id = farm;
    carry.returning_resource = false;
    require(simulation.effective_carry_capacity(carry) == 19);
    carry.resource_building_id = 0;
    require(simulation.effective_carry_capacity(carry) == 18);
    carry.resource_building_id = farm;
    carry.returning_resource = true;
    require(simulation.effective_carry_capacity(carry) == 18);

    aoe::GameMap rate_map(8, 8);
    rate_map.set_terrain({3, 2}, aoe::Terrain::forest);
    aoe::Simulation rate(std::move(rate_map));
    const auto gatherer = rate.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    rate.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 6}
    );
    rate.replace_technologies(
        aoe::Player::blue,
        {
            aoe::Technology::double_bit_axe,
            aoe::Technology::bow_saw,
            aoe::Technology::two_man_saw,
        }
    );
    require(rate.command_unit(gatherer, {3, 2}));
    for (int tick = 0;
         tick < 10 && rate.units().front().carried_amount == 0;
         ++tick) {
        rate.update();
    }
    require(rate.units().front().carried_amount == 1);
    require(rate.units().front().gather_work_remainder == 5840);
    const auto verify_mining_remainder = [](
        aoe::Terrain terrain,
        aoe::Technology first,
        aoe::Technology second
    ) {
        aoe::GameMap map(8, 8);
        map.set_terrain({3, 2}, terrain);
        aoe::Simulation mining(std::move(map));
        const auto worker = mining.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
        );
        mining.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {6, 6}
        );
        mining.replace_technologies(
            aoe::Player::blue, {first, second}
        );
        require(mining.command_unit(worker, {3, 2}));
        for (int tick = 0;
             tick < 10 && mining.units().front().carried_amount == 0;
             ++tick) {
            mining.update();
        }
        require(mining.units().front().carried_amount == 1);
        require(mining.units().front().gather_work_remainder == 3225);
    };
    verify_mining_remainder(
        aoe::Terrain::gold_mine,
        aoe::Technology::gold_mining,
        aoe::Technology::gold_shaft_mining
    );
    verify_mining_remainder(
        aoe::Terrain::stone_mine,
        aoe::Technology::stone_mining,
        aoe::Technology::stone_shaft_mining
    );
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-economy-technologies.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::two_man_saw
    ));
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::hand_cart
    ));
}

void naval_trade_and_fish_traps_are_bounded_and_persistent() {
    const auto& cog = aoe::rules_for(aoe::UnitKind::trade_cog);
    require(cog.hit_points == 80);
    require(cog.wood_cost == 100 && cog.gold_cost == 50);
    require(cog.training_ticks == 12);
    require(cog.vision_range == 6);
    require(cog.pierce_armor == 6);
    require(cog.movement_speed_percent == 132);
    const auto& trap = aoe::rules_for(aoe::BuildingKind::fish_trap);
    require(trap.hit_points == 50);
    require(trap.wood_cost == 100);
    require(trap.construction_ticks == 18);
    require(trap.minimum_age == aoe::Age::feudal);
    require(aoe::rules_for(aoe::Technology::coinage).food_cost == 150);
    require(aoe::rules_for(aoe::Technology::banking).gold_cost == 100);
    require(aoe::rules_for(aoe::Technology::cartography).research_ticks == 20);
    require(aoe::rules_for(aoe::Technology::caravan).research_ticks == 14);
    require(aoe::rules_for(aoe::Technology::guilds).food_cost == 300);

    aoe::GameMap map(18, 8);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    prepare_dock_foundation(map, {0, 1});
    prepare_dock_foundation(map, {14, 1});
    aoe::Simulation simulation(std::move(map));
    simulation.add_building(
        aoe::BuildingKind::dock, aoe::Player::blue, {0, 1}
    );
    const auto red_dock = simulation.add_building(
        aoe::BuildingKind::dock, aoe::Player::red, {14, 1}
    );
    const auto fisher = simulation.add_unit(
        aoe::UnitKind::fishing_ship, aoe::Player::blue, {4, 5}
    );
    const auto trade_cog = simulation.add_unit(
        aoe::UnitKind::trade_cog, aoe::Player::blue, {3, 3}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {5000, 5000, 5000, 5000}, {500, 500, 500, 500}, 0
    );
    simulation.replace_ages(aoe::Age::feudal, aoe::Age::feudal);
    require(simulation.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    require(simulation.construct_building_at(
        fisher, aoe::BuildingKind::fish_trap, {5, 5}
    ));
    for (int tick = 0; tick < 18; ++tick) simulation.update();
    const auto built_trap = std::ranges::find_if(
        simulation.buildings(), [](const aoe::Building& building) {
            return building.kind == aoe::BuildingKind::fish_trap;
        }
    );
    require(built_trap != simulation.buildings().end());
    require(built_trap->resource_amount == 700);
    require(simulation.command_unit(fisher, built_trap->position));
    for (int tick = 0; tick < 10; ++tick) simulation.update();
    const auto trap_after = std::ranges::find_if(
        simulation.buildings(), [](const aoe::Building& building) {
            return building.kind == aoe::BuildingKind::fish_trap;
        }
    );
    require(trap_after->resource_amount < 700);

    const int gold_before = simulation.economy(aoe::Player::blue).gold;
    require(simulation.command_trade_route(trade_cog, red_dock));
    for (int tick = 0;
         tick < 300 &&
         simulation.economy(aoe::Player::blue).gold == gold_before;
         ++tick) {
        simulation.update();
    }
    require(simulation.economy(aoe::Player::blue).gold > gold_before);

    simulation.replace_technologies(
        aoe::Player::blue, {aoe::Technology::guilds}
    );
    require(simulation.market_buy_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 115);
    require(simulation.market_sell_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 85);
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-naval-trade.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(std::ranges::any_of(
        restored.units(), [](const aoe::Unit& unit) {
            return unit.kind == aoe::UnitKind::trade_cog;
        }
    ));
    require(std::ranges::any_of(
        restored.buildings(), [](const aoe::Building& building) {
            return building.kind == aoe::BuildingKind::fish_trap;
        }
    ));
}

void caravan_cartography_and_tribute_have_bounded_contracts() {
    require(aoe::percentage_fee_floor(99, 30) == 29);
    require(aoe::percentage_fee_floor(99, 20) == 19);
    require(aoe::percentage_fee_floor(1, 30) == 0);
    require(aoe::market_price_after_fee(99, 15, true) == 113);
    require(aoe::market_price_after_fee(99, 15, false) == 84);
    require(
        aoe::percentage_fee_floor(
            std::numeric_limits<int>::max(),
            std::numeric_limits<int>::max()
        ) == std::numeric_limits<int>::max()
    );

    aoe::Simulation vision(aoe::GameMap(40, 6));
    vision.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    vision.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {30, 1}
    );
    require(vision.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    require(!vision.is_visible(aoe::Player::blue, {30, 1}));
    vision.replace_technologies(
        aoe::Player::blue, {aoe::Technology::cartography}
    );
    require(vision.is_visible(aoe::Player::blue, {30, 1}));

    aoe::GameMap movement_map(8, 8);
    movement_map.set_terrain({2, 2}, aoe::Terrain::water);
    aoe::Simulation movement(std::move(movement_map));
    movement.add_unit(
        aoe::UnitKind::trade_cart, aoe::Player::blue, {1, 1}
    );
    movement.add_unit(
        aoe::UnitKind::trade_cog, aoe::Player::blue, {2, 2}
    );
    movement.replace_technologies(
        aoe::Player::blue, {aoe::Technology::caravan}
    );
    require(movement.unique_unit_movement_numerator(
        movement.units()[0]
    ) == 150);
    require(movement.effective_ship_movement_numerator(
        movement.units()[1]
    ) == 198);

    const auto make_tribute = [](std::vector<aoe::Technology> technologies) {
        aoe::Simulation simulation(aoe::GameMap(12, 6));
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::blue, {1, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {9, 1}
        );
        simulation.replace_state(
            simulation.units(), simulation.buildings(),
            {500, 0, 0, 0}, {0, 0, 0, 0}, 0
        );
        require(simulation.set_diplomacy(
            aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
        ));
        simulation.replace_technologies(
            aoe::Player::blue, std::move(technologies)
        );
        return simulation;
    };
    auto base = make_tribute({});
    require(base.tribute_resource(
        aoe::Player::blue, aoe::Player::red,
        aoe::ResourceKind::wood, 100
    ));
    require(base.economy(aoe::Player::blue).wood == 370);
    require(base.economy(aoe::Player::red).wood == 100);
    auto coinage = make_tribute({aoe::Technology::coinage});
    require(coinage.tribute_resource(
        aoe::Player::blue, aoe::Player::red,
        aoe::ResourceKind::wood, 100
    ));
    require(coinage.economy(aoe::Player::blue).wood == 380);
    auto coinage_rounding = make_tribute({aoe::Technology::coinage});
    require(coinage_rounding.tribute_resource(
        aoe::Player::blue, aoe::Player::red,
        aoe::ResourceKind::wood, 99
    ));
    require(coinage_rounding.economy(aoe::Player::blue).wood == 382);
    require(coinage_rounding.economy(aoe::Player::red).wood == 99);
    auto banking = make_tribute(
        {aoe::Technology::coinage, aoe::Technology::banking}
    );
    require(banking.tribute_resource(
        aoe::Player::blue, aoe::Player::red,
        aoe::ResourceKind::wood, 100
    ));
    require(banking.economy(aoe::Player::blue).wood == 400);

    auto replayed = make_tribute({aoe::Technology::coinage});
    aoe::Replay replay;
    replay.record(
        0,
        aoe::TributeResourceCommand{
            aoe::Player::blue, aoe::Player::red,
            aoe::ResourceKind::wood, 100
        }
    );
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-tribute.replay";
    aoe::save_replay(replay, replay_path);
    auto loaded_replay = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    loaded_replay.apply_current_tick(replayed);
    require(replayed.economy(aoe::Player::blue).wood == 380);
    require(replayed.economy(aoe::Player::red).wood == 100);

    auto overflow = make_tribute({aoe::Technology::coinage});
    const auto before_blue = overflow.economy(aoe::Player::blue);
    const auto before_red = overflow.economy(aoe::Player::red);
    aoe::Replay overflow_replay;
    overflow_replay.record(
        0,
        aoe::TributeResourceCommand{
            aoe::Player::blue, aoe::Player::red,
            aoe::ResourceKind::wood,
            std::numeric_limits<int>::max()
        }
    );
    bool overflow_rejected{};
    try {
        overflow_replay.apply_current_tick(overflow);
    } catch (const std::runtime_error&) {
        overflow_rejected = true;
    }
    require(overflow_rejected);
    require(
        overflow.economy(aoe::Player::blue).wood ==
        before_blue.wood
    );
    require(
        overflow.economy(aoe::Player::red).wood ==
        before_red.wood
    );
}

void defensive_infrastructure_is_exact_and_bounded() {
    const auto& outpost = aoe::rules_for(aoe::BuildingKind::outpost);
    require(outpost.hit_points == 500);
    require(outpost.wood_cost == 25 && outpost.stone_cost == 10);
    require(outpost.construction_ticks == 5 && outpost.vision_range == 6);
    const auto& watch = aoe::rules_for(aoe::Technology::town_watch);
    require(watch.food_cost == 75 && watch.research_ticks == 9);
    const auto& patrol = aoe::rules_for(aoe::Technology::town_patrol);
    require(patrol.food_cost == 300 && patrol.gold_cost == 200);
    require(patrol.research_ticks == 14);
    require(aoe::rules_for(aoe::Technology::masonry).wood_cost == 175);
    require(aoe::rules_for(aoe::Technology::architecture).research_ticks == 24);
    require(aoe::rules_for(aoe::Technology::ballistics).gold_cost == 175);
    require(aoe::rules_for(aoe::Technology::heated_shot).food_cost == 350);
    require(aoe::rules_for(aoe::Technology::hoardings).research_ticks == 25);
    require(aoe::rules_for(aoe::Technology::sappers).research_ticks == 4);

    aoe::Simulation simulation(aoe::GameMap(24, 8));
    const auto villager = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {22, 6}
    );
    simulation.replace_state(
        simulation.units(), simulation.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::outpost_gate
    ));
    require(simulation.construct_building_at(
        villager, aoe::BuildingKind::outpost, {3, 2}
    ));
    for (int tick = 0; tick < 5; ++tick) simulation.update();
    const auto& built = simulation.buildings().back();
    require(built.kind == aoe::BuildingKind::outpost && built.completed());
    require(simulation.is_visible(aoe::Player::blue, {9, 2}));
    require(!simulation.is_visible(aoe::Player::blue, {13, 2}));

    simulation.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::town_watch, aoe::Technology::town_patrol,
         aoe::Technology::masonry, aoe::Technology::architecture}
    );
    require(simulation.is_visible(aoe::Player::blue, {13, 2}));
    require(simulation.maximum_hit_points(built) == 605);
    require(simulation.melee_armor(built) == 2);
    require(simulation.pierce_armor(built) == 2);

    aoe::Simulation castle_hp(aoe::GameMap(12, 8));
    const auto castle = castle_hp.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {2, 2}
    );
    castle_hp.replace_technologies(
        aoe::Player::blue, {aoe::Technology::hoardings}
    );
    require(castle_hp.maximum_hit_points(
        *std::ranges::find_if(
            castle_hp.buildings(), [castle](const aoe::Building& building) {
                return building.id == castle;
            }
        )
    ) == aoe::rules_for(aoe::BuildingKind::castle).hit_points * 121 / 100);

    require(!aoe::civilization_has_technology(
        aoe::Civilization::aztecs, aoe::Technology::masonry
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::byzantines, aoe::Technology::masonry
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::mayans, aoe::Technology::masonry
    ));
    require(!aoe::civilization_has_technology(
        aoe::Civilization::japanese, aoe::Technology::heated_shot
    ));
    require(aoe::civilization_has_technology(
        aoe::Civilization::britons, aoe::Technology::ballistics
    ));

    aoe::GameMap class_map(30, 10);
    class_map.set_terrain({26, 1}, aoe::Terrain::water);
    aoe::Simulation classes(std::move(class_map));
    classes.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::town_watch, aoe::Technology::town_patrol,
         aoe::Technology::masonry, aoe::Technology::architecture,
         aoe::Technology::heated_shot, aoe::Technology::sappers}
    );
    classes.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    classes.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::blue, {8, 1}
    );
    classes.add_building(
        aoe::BuildingKind::farm, aoe::Player::blue, {15, 1}
    );
    classes.add_building(
        aoe::BuildingKind::stone_wall, aoe::Player::blue, {21, 1}
    );
    classes.add_building(
        aoe::BuildingKind::fish_trap, aoe::Player::blue, {26, 1}
    );
    const auto& house = classes.buildings()[0];
    const auto& tower = classes.buildings()[1];
    const auto& farm = classes.buildings()[2];
    const auto& wall = classes.buildings()[3];
    const auto& fish_trap = classes.buildings()[4];
    require(classes.maximum_hit_points(house) ==
        aoe::rules_for(aoe::BuildingKind::house).hit_points * 110 / 100 *
            110 / 100);
    require(classes.maximum_hit_points(tower) ==
        aoe::rules_for(aoe::BuildingKind::watch_tower).hit_points *
            110 / 100 * 110 / 100);
    require(classes.maximum_hit_points(farm) ==
        aoe::rules_for(aoe::BuildingKind::farm).hit_points);
    require(classes.maximum_hit_points(wall) ==
        aoe::rules_for(aoe::BuildingKind::stone_wall).hit_points);
    require(classes.maximum_hit_points(fish_trap) ==
        aoe::rules_for(aoe::BuildingKind::fish_trap).hit_points);
    require(classes.melee_armor(farm) ==
        aoe::rules_for(aoe::BuildingKind::farm).melee_armor);
    require(classes.melee_armor(wall) ==
        aoe::rules_for(aoe::BuildingKind::stone_wall).melee_armor);
    require(classes.pierce_armor(fish_trap) ==
        aoe::rules_for(aoe::BuildingKind::fish_trap).pierce_armor);
    require(classes.building_class_11_armor(house) == 6);
    require(classes.building_class_11_armor(tower) == 6);
    require(classes.building_class_11_armor(farm) == 0);
    require(classes.building_class_11_armor(wall) == 0);
    require(classes.building_class_11_armor(fish_trap) == 0);
    require(classes.is_visible(aoe::Player::blue, {7, 1}));
    require(!classes.is_visible(aoe::Player::blue, {29, 1}));

    require(classes.sappers_attack_bonus(
        aoe::Player::blue, aoe::BuildingKind::fish_trap
    ) == 0);
    require(classes.sappers_attack_bonus(
        aoe::Player::blue, aoe::BuildingKind::house
    ) == 15);
    require(classes.sappers_attack_bonus(
        aoe::Player::blue, aoe::BuildingKind::watch_tower
    ) == 30);
    require(classes.sappers_attack_bonus(
        aoe::Player::blue, aoe::BuildingKind::stone_wall
    ) == 30);
    require(classes.sappers_attack_bonus(
        aoe::Player::blue, aoe::BuildingKind::stone_gate_x
    ) == 15);
    require(classes.defensive_ship_bonus(
        aoe::Player::blue, aoe::BuildingKind::town_center
    ) == 0);
    require(classes.defensive_ship_bonus(
        aoe::Player::blue, aoe::BuildingKind::castle
    ) == 4);
    require(classes.defensive_ship_bonus(
        aoe::Player::blue, aoe::BuildingKind::watch_tower
    ) == 15);
    require(classes.defensive_ship_bonus(
        aoe::Player::blue, aoe::BuildingKind::bombard_tower
    ) == 90);

    const auto building_damage = [](
        aoe::UnitKind attacker_kind, bool masonry
    ) {
        aoe::Simulation combat(aoe::GameMap(10, 6));
        const auto attacker = combat.add_unit(
            attacker_kind, aoe::Player::blue, {2, 2}
        );
        combat.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {3, 2}
        );
        if (masonry) {
            combat.replace_technologies(
                aoe::Player::red, {aoe::Technology::masonry}
            );
        }
        const int before = combat.buildings()[0].hit_points;
        require(combat.command_unit(attacker, {3, 2}));
        combat.update();
        return before - combat.buildings()[0].hit_points;
    };
    require(building_damage(aoe::UnitKind::villager, false) -
        building_damage(aoe::UnitKind::villager, true) == 1);
    require(building_damage(aoe::UnitKind::long_swordsman, false) -
        building_damage(aoe::UnitKind::long_swordsman, true) == 4);

    const auto moving_target_damage = [](
        aoe::UnitKind attacker_kind, bool ballistics
    ) {
        aoe::Simulation combat(aoe::GameMap(14, 8));
        const auto attacker = combat.add_unit(
            attacker_kind, aoe::Player::blue, {1, 2}
        );
        const auto target = combat.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {5, 2}
        );
        if (ballistics) {
            combat.replace_technologies(
                aoe::Player::blue, {aoe::Technology::ballistics}
            );
        }
        const int before = combat.units()[1].hit_points;
        require(combat.command_unit(attacker, {5, 2}));
        combat.update();
        require(combat.command_unit(target, {5, 6}));
        for (int tick = 0; tick < 6; ++tick) combat.update();
        return before - combat.units()[1].hit_points;
    };
    require(moving_target_damage(aoe::UnitKind::archer, false) == 0);
    require(moving_target_damage(aoe::UnitKind::archer, true) > 0);
    require(moving_target_damage(aoe::UnitKind::scorpion, false) == 0);
    require(moving_target_damage(aoe::UnitKind::scorpion, true) == 0);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-defensive-infrastructure.save";
    aoe::save_game(simulation, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.has_technology(
        aoe::Player::blue, aoe::Technology::architecture
    ));
    require(restored.buildings().back().kind == aoe::BuildingKind::outpost);
}

void wonder_and_standard_victories_are_bounded_and_persistent() {
    const auto& wonder = aoe::rules_for(aoe::BuildingKind::wonder);
    require(wonder.hit_points == 4800);
    require(wonder.melee_armor == 3 && wonder.pierce_armor == 10);
    require(wonder.wood_cost == 1000 && wonder.gold_cost == 1000);
    require(wonder.stone_cost == 1000);
    require(wonder.construction_ticks == 1167);
    require(wonder.vision_range == 8);
    require(wonder.minimum_age == aoe::Age::imperial);
    require(aoe::civilization_has_building(
        aoe::Civilization::aztecs, aoe::BuildingKind::wonder
    ));

    const auto wonder_rules = [](int countdown) {
        aoe::MatchRules rules;
        rules.conquest_enabled = false;
        rules.wonder_enabled = true;
        rules.relic_enabled = false;
        rules.wonder_countdown_ticks = countdown;
        return rules;
    };
    aoe::Simulation interrupted(aoe::GameMap(20, 10));
    interrupted.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    interrupted.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 1}
    );
    interrupted.set_match_rules(wonder_rules(3));
    interrupted.update();
    require(interrupted.victory_countdown(aoe::Player::blue) == 2);
    const auto wonder_id = interrupted.buildings()[0].id;
    require(interrupted.delete_building(wonder_id));
    interrupted.update();
    require(interrupted.victory_countdown(aoe::Player::blue) == 0);
    require(interrupted.outcome() == aoe::MatchOutcome::ongoing);

    aoe::Simulation victory(aoe::GameMap(20, 10));
    victory.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    victory.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 1}
    );
    victory.set_match_rules(wonder_rules(3));
    victory.update();
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-wonder-countdown.save";
    aoe::save_game(victory, save_path);
    auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.victory_countdown(aoe::Player::blue) == 2);
    restored.update();
    restored.update();
    require(restored.outcome() == aoe::MatchOutcome::blue_victory);
    const auto frozen_tick = restored.tick_number();
    const auto frozen_rules = restored.match_rules();
    auto changed_rules = frozen_rules;
    changed_rules.wonder_countdown_ticks = 99;
    restored.set_match_rules(changed_rules);
    require(
        restored.match_rules().wonder_countdown_ticks ==
        frozen_rules.wonder_countdown_ticks
    );
    require(!restored.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    restored.update();
    require(restored.tick_number() == frozen_tick);

    aoe::Simulation simultaneous(aoe::GameMap(24, 12));
    simultaneous.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    simultaneous.add_building(
        aoe::BuildingKind::wonder, aoe::Player::red, {14, 1}
    );
    simultaneous.set_match_rules(wonder_rules(1));
    simultaneous.update();
    require(simultaneous.outcome() == aoe::MatchOutcome::draw);

    aoe::Simulation allied(aoe::GameMap(20, 10));
    allied.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    allied.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {12, 1}
    );
    allied.replace_diplomacy(aoe::Diplomacy::ally);
    allied.set_match_rules(wonder_rules(1));
    allied.update();
    require(allied.outcome() == aoe::MatchOutcome::allied_victory);

    aoe::Simulation allied_resign(aoe::GameMap(10, 6));
    allied_resign.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    allied_resign.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 4}
    );
    allied_resign.replace_diplomacy(aoe::Diplomacy::ally);
    require(allied_resign.resign(aoe::Player::blue));
    require(
        allied_resign.outcome() == aoe::MatchOutcome::red_victory
    );

    aoe::Simulation mixed(aoe::GameMap(30, 12));
    mixed.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    mixed.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {9, 1}
    );
    mixed.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {24, 1}
    );
    auto mixed_buildings = mixed.buildings();
    mixed_buildings[1].relic_count = 5;
    mixed.replace_state(
        mixed.units(), std::move(mixed_buildings),
        mixed.economy(aoe::Player::blue),
        mixed.economy(aoe::Player::red), 0
    );
    auto mixed_rules = wonder_rules(5);
    mixed_rules.relic_enabled = true;
    mixed_rules.relic_countdown_ticks = 7;
    mixed.set_match_rules(mixed_rules);
    mixed.update();
    require(mixed.countdown_kind(aoe::Player::blue) ==
        aoe::VictoryCountdownKind::wonder);
    require(mixed.victory_countdown(aoe::Player::blue) == 4);
    require(mixed.delete_building(mixed.buildings()[0].id));
    mixed.update();
    require(mixed.countdown_kind(aoe::Player::blue) ==
        aoe::VictoryCountdownKind::relic);
    require(mixed.victory_countdown(aoe::Player::blue) == 6);
    mixed.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {14, 5}
    );
    mixed.update();
    require(mixed.countdown_kind(aoe::Player::blue) ==
        aoe::VictoryCountdownKind::relic);
    require(mixed.victory_countdown(aoe::Player::blue) == 5);

    aoe::Simulation multiple(aoe::GameMap(32, 12));
    multiple.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    multiple.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {9, 1}
    );
    multiple.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {25, 1}
    );
    multiple.set_match_rules(wonder_rules(5));
    multiple.update();
    require(multiple.victory_countdown(aoe::Player::blue) == 4);
    require(multiple.delete_building(multiple.buildings()[0].id));
    multiple.update();
    require(multiple.countdown_kind(aoe::Player::blue) ==
        aoe::VictoryCountdownKind::wonder);
    require(multiple.victory_countdown(aoe::Player::blue) == 3);

    aoe::Simulation relics(aoe::GameMap(16, 8));
    relics.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {1, 1}
    );
    relics.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {11, 1}
    );
    auto relic_buildings = relics.buildings();
    relic_buildings[0].relic_count = 5;
    relics.replace_state(
        relics.units(), std::move(relic_buildings),
        relics.economy(aoe::Player::blue),
        relics.economy(aoe::Player::red), 0
    );
    aoe::MatchRules relic_rules;
    relic_rules.conquest_enabled = false;
    relic_rules.wonder_enabled = false;
    relic_rules.relic_enabled = true;
    relic_rules.relic_countdown_ticks = 2;
    relics.set_match_rules(relic_rules);
    relics.update();
    relics.update();
    require(relics.outcome() == aoe::MatchOutcome::blue_victory);

    aoe::Simulation timed(aoe::GameMap(12, 6));
    timed.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {1, 1}
    );
    timed.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 4}
    );
    aoe::MatchRules time_rules;
    time_rules.conquest_enabled = false;
    time_rules.wonder_enabled = false;
    time_rules.relic_enabled = false;
    time_rules.time_limit_ticks = 1;
    timed.set_match_rules(time_rules);
    timed.update();
    require(timed.outcome() == aoe::MatchOutcome::blue_victory);

    aoe::Simulation scored(aoe::GameMap(12, 6));
    scored.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {1, 1}
    );
    scored.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 4}
    );
    auto score_rules = time_rules;
    score_rules.time_limit_ticks = 0;
    score_rules.score_limit = scored.score(aoe::Player::blue);
    scored.set_match_rules(score_rules);
    scored.update();
    require(scored.outcome() == aoe::MatchOutcome::blue_victory);

    aoe::Simulation time_precedence(aoe::GameMap(12, 6));
    time_precedence.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {1, 1}
    );
    time_precedence.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 4}
    );
    auto precedence_rules = time_rules;
    precedence_rules.score_limit = 1;
    time_precedence.set_match_rules(precedence_rules);
    time_precedence.update();
    require(
        time_precedence.outcome() == aoe::MatchOutcome::blue_victory
    );

    aoe::Replay replay;
    replay.record(0, aoe::ResignCommand{aoe::Player::red});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-resign-victory.replay";
    aoe::save_replay(replay, replay_path);
    auto replay_copy = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation resignation(aoe::GameMap(8, 6));
    resignation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    resignation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {6, 4}
    );
    replay_copy.apply_current_tick(resignation);
    require(resignation.outcome() == aoe::MatchOutcome::blue_victory);

    aoe::Scenario scenario(20, 10);
    scenario.match_rules = wonder_rules(7);
    scenario.blue_age = aoe::Age::imperial;
    scenario.buildings.push_back({
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    });
    const auto scenario_path = std::filesystem::temp_directory_path() /
        "aoe-wonder-victory.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.match_rules.wonder_countdown_ticks == 7);
    require(scenario_copy.buildings[0].kind == aoe::BuildingKind::wonder);
}

void advanced_formations_are_oriented_stable_and_persistent() {
    aoe::Simulation geometry(aoe::GameMap(30, 20));
    std::vector<aoe::EntityId> infantry;
    for (int y = 3; y < 9; ++y) {
        infantry.push_back(geometry.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, y}
        ));
    }
    geometry.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {28, 18}
    );
    const auto line = geometry.formation_destinations(
        infantry, {20, 10}, aoe::FormationKind::line
    );
    require(std::ranges::all_of(line, [](aoe::TilePosition slot) {
        return slot.x == 20;
    }));
    std::vector<aoe::TilePosition> unique = line;
    std::ranges::sort(unique, {}, [](aoe::TilePosition slot) {
        return std::pair{slot.x, slot.y};
    });
    require(std::ranges::adjacent_find(unique) == unique.end());

    const auto box = geometry.formation_destinations(
        infantry, {20, 10}, aoe::FormationKind::box
    );
    require(std::ranges::any_of(box, [](aoe::TilePosition slot) {
        return slot.x < 20;
    }));
    const auto staggered = geometry.formation_destinations(
        infantry, {20, 10}, aoe::FormationKind::staggered
    );
    require(staggered != box);
    const auto flank = geometry.formation_destinations(
        infantry, {20, 10}, aoe::FormationKind::flank
    );
    require(std::ranges::any_of(flank, [](aoe::TilePosition slot) {
        return slot.y < 10;
    }));
    require(std::ranges::any_of(flank, [](aoe::TilePosition slot) {
        return slot.y > 10;
    }));

    aoe::Simulation roles(aoe::GameMap(24, 14));
    const auto swordsman = roles.add_unit(
        aoe::UnitKind::long_swordsman, aoe::Player::blue, {2, 2}
    );
    const auto archer = roles.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {2, 4}
    );
    const auto role_monk = roles.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 6}
    );
    const auto role_ram = roles.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {2, 8}
    );
    roles.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {22, 12}
    );
    const auto role_slots = roles.formation_destinations(
        {swordsman, archer, role_monk, role_ram},
        {18, 7}, aoe::FormationKind::box
    );
    require(role_slots[0].x >= role_slots[1].x);
    require(role_slots[1].x >= role_slots[2].x);
    require(role_slots[1].x >= role_slots[3].x);

    aoe::GameMap water_map(20, 12);
    for (int y = 0; y < water_map.height(); ++y) {
        for (int x = 0; x < water_map.width(); ++x) {
            water_map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    aoe::Simulation navy(std::move(water_map));
    std::vector<aoe::EntityId> ships{
        navy.add_unit(
            aoe::UnitKind::galley, aoe::Player::blue, {2, 2}
        ),
        navy.add_unit(
            aoe::UnitKind::transport_ship, aoe::Player::blue, {2, 5}
        ),
    };
    navy.add_unit(
        aoe::UnitKind::galley, aoe::Player::red, {18, 10}
    );
    const auto naval_slots = navy.formation_destinations(
        ships, {14, 6}, aoe::FormationKind::line
    );
    for (const auto slot : naval_slots) {
        require(navy.map().terrain_at(slot) == aoe::Terrain::water);
    }
    require(!navy.command_formation(
        {ships[0], navy.units().back().id},
        {12, 6}, aoe::FormationKind::compact
    ));

    aoe::Simulation mixed(aoe::GameMap(30, 12));
    const auto knight = mixed.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {2, 3}
    );
    const auto monk = mixed.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 6}
    );
    const auto ram = mixed.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {3, 8}
    );
    mixed.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {28, 10}
    );
    const std::vector<aoe::EntityId> group{knight, monk, ram};
    require(mixed.command_formation(
        group, {22, 6}, aoe::FormationKind::box
    ));
    require(!mixed.command_formation(
        {knight, knight}, {22, 6}, aoe::FormationKind::box
    ));
    require(mixed.formation_kind(aoe::Player::blue) ==
        aoe::FormationKind::box);
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [](const aoe::Unit& unit) {
            return unit.formation_move_interval >= 2 &&
                unit.formation_group_id != 0;
        }
    ));
    for (int tick = 0; tick < 4; ++tick) mixed.update();
    const int knight_steps =
        std::abs(mixed.units()[0].position.x - 2) +
        std::abs(mixed.units()[0].position.y - 3);
    const int monk_steps =
        std::abs(mixed.units()[1].position.x - 2) +
        std::abs(mixed.units()[1].position.y - 6);
    require(std::abs(knight_steps - monk_steps) <= 1);
    require(mixed.command_formation_order(
        group, {20, 5}, aoe::FormationKind::line,
        aoe::FormationOrderKind::attack_move
    ));
    for (aoe::EntityId id : group) {
        require(mixed.set_unit_stance(id, aoe::UnitStance::passive));
    }
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [](const aoe::Unit& unit) { return unit.attack_moving; }
    ));
    for (int tick = 0; tick < 40; ++tick) mixed.update();
    std::vector<aoe::TilePosition> reached_slots{
        mixed.units()[0].position,
        mixed.units()[1].position,
        mixed.units()[2].position,
    };
    std::ranges::sort(reached_slots, {}, [](aoe::TilePosition slot) {
        return std::pair{slot.x, slot.y};
    });
    require(std::ranges::adjacent_find(reached_slots) ==
        reached_slots.end());
    require(mixed.command_formation_order(
        group, {18, 5}, aoe::FormationKind::staggered,
        aoe::FormationOrderKind::patrol
    ));
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [](const aoe::Unit& unit) { return unit.patrolling; }
    ));
    for (int tick = 0; tick < 12; ++tick) mixed.update();
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [](const aoe::Unit& unit) {
            return unit.patrolling && unit.attack_moving;
        }
    ));
    const auto destination_before_queue = mixed.units()[0].destination;
    require(mixed.command_formation_order(
        group, {16, 5}, aoe::FormationKind::compact,
        aoe::FormationOrderKind::queued_waypoint
    ));
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [](const aoe::Unit& unit) {
            return !unit.formation_waypoints.empty();
        }
    ));
    require(mixed.units()[0].destination == destination_before_queue);
    const auto queued_group = mixed.units()[0].formation_group_id;
    for (int tick = 0; tick < 20; ++tick) mixed.update();
    require(mixed.units()[0].formation_group_id == queued_group);

    const auto guarded = mixed.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {10, 10}
    );
    require(mixed.command_formation_order(
        group, {10, 10}, aoe::FormationKind::line,
        aoe::FormationOrderKind::guard, guarded, false
    ));
    require(mixed.command_unit(guarded, {14, 10}));
    for (int tick = 0; tick < 16; ++tick) mixed.update();
    require(std::ranges::all_of(
        mixed.units().begin(), mixed.units().begin() + 3,
        [guarded](const aoe::Unit& unit) {
            return unit.guard_target_id == guarded &&
                unit.formation_group_id != 0;
        }
    ));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-advanced-formation.save";
    aoe::save_game(mixed, save_path);
    const auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.formation_kind(aoe::Player::blue) ==
        aoe::FormationKind::line);
    require(restored.units()[0].formation_group_id ==
        mixed.units()[0].formation_group_id);

    aoe::Replay replay;
    replay.record(0, aoe::SetFormationKindCommand{
        aoe::Player::blue, aoe::FormationKind::flank
    });
    aoe::MoveFormationCommand formation_replay{
        group, {22, 6}, aoe::FormationKind::flank
    };
    formation_replay.order = aoe::FormationOrderKind::attack_move;
    replay.record(0, formation_replay);
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-advanced-formation.replay";
    aoe::save_replay(replay, replay_path);
    auto replay_copy = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    aoe::Simulation replayed(aoe::GameMap(30, 12));
    replayed.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {2, 3}
    );
    replayed.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 6}
    );
    replayed.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {3, 8}
    );
    replayed.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {28, 10}
    );
    replay_copy.apply_current_tick(replayed);
    require(replayed.formation_kind(aoe::Player::blue) ==
        aoe::FormationKind::flank);
    require(replayed.units()[0].moving);
    require(replayed.units()[0].attack_moving);
    require(replayed.units()[0].attack_move_destination ==
        replayed.units()[0].formation_slot);

    aoe::Scenario scenario(12, 8);
    scenario.blue_formation = aoe::FormationKind::staggered;
    const auto scenario_path = std::filesystem::temp_directory_path() /
        "aoe-advanced-formation.scenario";
    aoe::save_scenario(scenario, scenario_path);
    const auto scenario_copy = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(scenario_copy.blue_formation ==
        aoe::FormationKind::staggered);
}

void formation_semantic_legs_and_boundary_are_durable() {
    aoe::Simulation simulation(aoe::GameMap(50, 20));
    const std::vector<aoe::EntityId> group{
        simulation.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, 5}
        ),
        simulation.add_unit(
            aoe::UnitKind::archer, aoe::Player::blue, {2, 7}
        ),
    };
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {48, 18}
    );
    require(!simulation.command_formation(
        {}, {10, 6}, aoe::FormationKind::compact
    ));
    require(!simulation.command_formation(
        group, {10, 6}, static_cast<aoe::FormationKind>(99)
    ));
    require(!simulation.command_formation_order(
        {}, {10, 6}, aoe::FormationKind::compact,
        aoe::FormationOrderKind::move
    ));
    require(!simulation.command_formation_order(
        group, {10, 6}, aoe::FormationKind::compact,
        static_cast<aoe::FormationOrderKind>(99)
    ));
    require(std::ranges::none_of(
        simulation.units().begin(), simulation.units().begin() + 2,
        [](const aoe::Unit& unit) {
            return unit.moving || unit.formation_group_id != 0;
        }
    ));

    const auto make_cavalry = [] {
        aoe::Simulation cavalry(aoe::GameMap(36, 12));
        cavalry.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::blue, {2, 3}
        );
        cavalry.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::blue, {2, 6}
        );
        cavalry.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {34, 10}
        );
        cavalry.replace_technologies(
            aoe::Player::blue, {aoe::Technology::husbandry}
        );
        return cavalry;
    };
    auto direct_pace = make_cavalry();
    require(direct_pace.command_formation(
        {1, 2}, {28, 5}, aoe::FormationKind::box
    ));
    const int expected_interval =
        direct_pace.units()[0].formation_move_interval;
    const int expected_numerator =
        direct_pace.units()[0].formation_speed_numerator;
    auto queued_pace = make_cavalry();
    require(queued_pace.command_unit(1, {10, 3}));
    require(queued_pace.command_unit(2, {18, 6}));
    require(queued_pace.command_formation_order(
        {1, 2}, {28, 5}, aoe::FormationKind::box,
        aoe::FormationOrderKind::queued_waypoint
    ));
    for (std::size_t index = 0; index < 2; ++index) {
        const aoe::Unit& unit = queued_pace.units()[index];
        require(unit.formation_waypoints.front().move_interval ==
            expected_interval);
        require(unit.formation_waypoints.front().speed_numerator ==
            expected_numerator);
    }

    bool short_member_waited = false;
    for (int tick = 0; tick < 300; ++tick) {
        queued_pace.update();
        const aoe::Unit& short_member = queued_pace.units()[0];
        const aoe::Unit& long_member = queued_pace.units()[1];
        if (!short_member.moving && long_member.moving) {
            short_member_waited = true;
            require(short_member.formation_group_id == 0);
            require(short_member.formation_waypoints.size() == 1);
        }
        if (short_member.formation_group_id != 0) {
            require(long_member.formation_group_id ==
                short_member.formation_group_id);
            require(short_member.formation_anchor ==
                aoe::TilePosition(28, 5));
            require(long_member.formation_anchor ==
                aoe::TilePosition(28, 5));
            require(short_member.formation_waypoints.empty());
            require(long_member.formation_waypoints.empty());
            break;
        }
    }
    require(short_member_waited);
    require(queued_pace.units()[0].formation_group_id != 0);

    require(simulation.command_formation_order(
        group, {14, 6}, aoe::FormationKind::line,
        aoe::FormationOrderKind::queued_waypoint
    ));
    require(std::ranges::all_of(
        simulation.units().begin(), simulation.units().begin() + 2,
        [](const aoe::Unit& unit) {
            return unit.moving && unit.formation_group_id != 0 &&
                unit.formation_waypoints.empty();
        }
    ));
    require(simulation.command_formation_order(
        group, {26, 8}, aoe::FormationKind::box,
        aoe::FormationOrderKind::queued_waypoint
    ));
    require(simulation.command_formation_order(
        group, {38, 10}, aoe::FormationKind::staggered,
        aoe::FormationOrderKind::queued_waypoint
    ));
    require(std::ranges::all_of(
        simulation.units().begin(), simulation.units().begin() + 2,
        [](const aoe::Unit& unit) {
            return unit.formation_waypoints.size() == 2;
        }
    ));
    const auto first_leg = simulation.units()[0].formation_waypoints[0];
    const auto second_leg = simulation.units()[0].formation_waypoints[1];
    require(first_leg.kind == aoe::FormationKind::box);
    require(second_leg.kind == aoe::FormationKind::staggered);
    require(first_leg.anchor == aoe::TilePosition(26, 8));
    require(second_leg.anchor == aoe::TilePosition(38, 10));

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-formation-legs-v96.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.units()[0].formation_waypoints.size() == 2);
    const auto& restored_leg =
        restored.units()[0].formation_waypoints.front();
    require(restored_leg.destination == first_leg.destination);
    require(restored_leg.anchor == first_leg.anchor);
    require(restored_leg.slot == first_leg.slot);
    require(restored_leg.kind == first_leg.kind);
    require(restored_leg.group_id == first_leg.group_id);
    require(restored_leg.move_interval == first_leg.move_interval);
    require(restored_leg.speed_numerator == first_leg.speed_numerator);

    bool entered_first_leg = false;
    bool entered_second_leg = false;
    for (int tick = 0; tick < 500; ++tick) {
        restored.update();
        const aoe::Unit& unit = restored.units()[0];
        if (unit.formation_anchor == first_leg.anchor) {
            entered_first_leg = true;
            require(unit.formation_group_id == first_leg.group_id);
            require(unit.formation_move_interval ==
                first_leg.move_interval);
            require(unit.formation_speed_numerator ==
                first_leg.speed_numerator);
            require(unit.formation_slot == first_leg.slot);
        }
        if (unit.formation_anchor == second_leg.anchor) {
            entered_second_leg = true;
            require(unit.formation_group_id == second_leg.group_id);
            require(unit.formation_move_interval ==
                second_leg.move_interval);
            require(unit.formation_speed_numerator ==
                second_leg.speed_numerator);
            require(unit.formation_slot == second_leg.slot);
            break;
        }
    }
    require(entered_first_leg);
    require(entered_second_leg);

    aoe::Simulation spacing(aoe::GameMap(30, 16));
    const std::vector<aoe::EntityId> ordinary{
        spacing.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, 2}
        ),
        spacing.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, 4}
        ),
        spacing.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, 6}
        ),
    };
    const auto ordinary_slots = spacing.formation_destinations(
        ordinary, {20, 8}, aoe::FormationKind::line
    );
    int ordinary_minimum = 100;
    for (std::size_t first = 0; first < ordinary_slots.size(); ++first) {
        for (std::size_t second = first + 1;
             second < ordinary_slots.size(); ++second) {
            ordinary_minimum = std::min(ordinary_minimum, std::max(
                std::abs(
                    ordinary_slots[first].x - ordinary_slots[second].x
                ),
                std::abs(
                    ordinary_slots[first].y - ordinary_slots[second].y
                )
            ));
        }
    }
    require(ordinary_minimum == 1);
    const aoe::EntityId ram = spacing.add_unit(
        aoe::UnitKind::battering_ram, aoe::Player::blue, {3, 6}
    );
    const auto siege_slots = spacing.formation_destinations(
        {ordinary[0], ram, ordinary[1]},
        {20, 8}, aoe::FormationKind::line
    );
    for (std::size_t other : {std::size_t{0}, std::size_t{2}}) {
        require(std::max(
            std::abs(siege_slots[1].x - siege_slots[other].x),
            std::abs(siege_slots[1].y - siege_slots[other].y)
        ) >= 2);
    }

    aoe::Simulation patrol(aoe::GameMap(40, 20));
    const std::vector<aoe::EntityId> patrol_group{
        patrol.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {4, 6}
        ),
        patrol.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {4, 8}
        ),
        patrol.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {4, 10}
        ),
    };
    patrol.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {38, 18}
    );
    for (aoe::EntityId id : patrol_group) {
        require(patrol.set_unit_stance(id, aoe::UnitStance::passive));
    }
    const auto origin_slots = patrol.formation_destinations(
        patrol_group, {4, 8}, aoe::FormationKind::staggered,
        aoe::TilePosition{-1, 0}
    );
    const auto far_slots = patrol.formation_destinations(
        patrol_group, {28, 8}, aoe::FormationKind::staggered
    );
    require(patrol.command_formation_order(
        patrol_group, {28, 8}, aoe::FormationKind::staggered,
        aoe::FormationOrderKind::patrol
    ));
    for (std::size_t index = 0; index < patrol_group.size(); ++index) {
        require(patrol.units()[index].patrol_origin ==
            origin_slots[index]);
        require(patrol.units()[index].patrol_destination ==
            far_slots[index]);
    }
    std::vector<bool> reversed_from_far(3, false);
    std::vector<bool> reversed_from_home(3, false);
    for (int tick = 0; tick < 500; ++tick) {
        patrol.update();
        for (std::size_t index = 0; index < 3; ++index) {
            const aoe::Unit& unit = patrol.units()[index];
            if (unit.attack_move_destination == origin_slots[index]) {
                reversed_from_far[index] = true;
            }
            if (reversed_from_far[index] &&
                unit.attack_move_destination == far_slots[index]) {
                reversed_from_home[index] = true;
            }
        }
        if (std::ranges::all_of(reversed_from_home, std::identity{})) break;
    }
    require(std::ranges::all_of(reversed_from_far, std::identity{}));
    require(std::ranges::all_of(reversed_from_home, std::identity{}));

    const auto verify_return_facing = [](aoe::FormationKind kind) {
        aoe::Simulation patrol(aoe::GameMap(44, 22));
        std::vector<aoe::EntityId> ids;
        for (int index = 0; index < 5; ++index) {
            ids.push_back(patrol.add_unit(
                aoe::UnitKind::militia, aoe::Player::blue,
                {4, 5 + index * 2}
            ));
            require(patrol.set_unit_stance(
                ids.back(), aoe::UnitStance::passive
            ));
        }
        patrol.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {42, 20}
        );
        const auto expected_origins = patrol.formation_destinations(
            ids, {4, 9}, kind, aoe::TilePosition{-1, 0}
        );
        const auto expected_far = patrol.formation_destinations(
            ids, {32, 9}, kind
        );
        require(patrol.command_formation_order(
            ids, {32, 9}, kind, aoe::FormationOrderKind::patrol
        ));
        for (std::size_t index = 0; index < ids.size(); ++index) {
            require(patrol.units()[index].patrol_origin ==
                expected_origins[index]);
            require(patrol.units()[index].patrol_destination ==
                expected_far[index]);
        }
        std::vector<bool> far_reversal(ids.size(), false);
        std::vector<bool> home_reversal(ids.size(), false);
        for (int tick = 0; tick < 700; ++tick) {
            patrol.update();
            for (std::size_t index = 0; index < ids.size(); ++index) {
                const aoe::Unit& unit = patrol.units()[index];
                if (unit.attack_move_destination ==
                    expected_origins[index]) {
                    far_reversal[index] = true;
                }
                if (far_reversal[index] &&
                    unit.attack_move_destination == expected_far[index]) {
                    home_reversal[index] = true;
                }
            }
            if (std::ranges::all_of(
                    home_reversal, std::identity{})) break;
        }
        require(std::ranges::all_of(far_reversal, std::identity{}));
        require(std::ranges::all_of(home_reversal, std::identity{}));
    };
    verify_return_facing(aoe::FormationKind::box);
    verify_return_facing(aoe::FormationKind::flank);

    aoe::Simulation stopped_queue(aoe::GameMap(64, 12));
    const std::vector<aoe::EntityId> stopped_group{
        stopped_queue.add_unit(
            aoe::UnitKind::militia, aoe::Player::blue, {2, 3}
        ),
        stopped_queue.add_unit(
            aoe::UnitKind::archer, aoe::Player::blue, {2, 7}
        ),
    };
    stopped_queue.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {62, 10}
    );
    require(stopped_queue.command_formation(
        stopped_group, {18, 5}, aoe::FormationKind::line
    ));
    require(stopped_queue.command_formation_order(
        stopped_group, {34, 5}, aoe::FormationKind::box,
        aoe::FormationOrderKind::queued_waypoint
    ));
    require(stopped_queue.command_formation_order(
        stopped_group, {52, 5}, aoe::FormationKind::flank,
        aoe::FormationOrderKind::queued_waypoint
    ));
    for (int tick = 0; tick < 7; ++tick) stopped_queue.update();
    require(stopped_queue.units()[0].formation_waypoints.size() == 2);
    const auto stopped_position = stopped_queue.units()[0].position;
    require(stopped_queue.stop_unit(stopped_group[0]));
    require(stopped_queue.units()[0].formation_group_id == 0);
    require(stopped_queue.units()[0].formation_waypoints.empty());

    const auto stopped_path =
        std::filesystem::temp_directory_path() /
        "aoe-stopped-formation-queue.save";
    aoe::save_game(stopped_queue, stopped_path);
    auto stopped_restored = aoe::load_game(stopped_path);
    std::filesystem::remove(stopped_path);
    for (int tick = 0; tick < 500; ++tick) {
        stopped_queue.update();
        stopped_restored.update();
    }
    require(stopped_queue.units()[0].position == stopped_position);
    require(stopped_restored.units()[0].position == stopped_position);
    require(stopped_queue.units()[0].formation_waypoints.empty());
    require(stopped_restored.units()[0].formation_waypoints.empty());
    require(
        stopped_queue.units()[1].position ==
        stopped_restored.units()[1].position
    );
    require(stopped_queue.units()[1].position.x > 40);
}

void formation_movement_credit_stays_in_its_denominator_domain() {
    const auto require_clean_cavalry_cadence = [](
        aoe::Simulation& simulation,
        aoe::EntityId unit_id,
        aoe::TilePosition destination
    ) {
        const aoe::TilePosition origin =
            simulation.units().front().position;
        require(simulation.command_unit(unit_id, destination));
        require(
            simulation.units().front().movement_speed_remainder == 0
        );
        simulation.update();
        require(simulation.units().front().position == origin);
        simulation.update();
        require(
            simulation.units().front().position ==
            aoe::TilePosition(origin.x + 1, origin.y)
        );
    };

    aoe::Simulation completed(aoe::GameMap(40, 6));
    const aoe::EntityId completed_scout = completed.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 2}
    );
    completed.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {39, 5}
    );
    require(completed.command_formation(
        {completed_scout}, {12, 2}, aoe::FormationKind::line
    ));
    bool held_formation_credit = false;
    for (int tick = 0; tick < 200; ++tick) {
        completed.update();
        held_formation_credit =
            held_formation_credit ||
            (completed.units().front().formation_group_id != 0 &&
             completed.units().front().movement_speed_remainder > 0);
        if (completed.units().front().formation_group_id == 0) break;
    }
    require(held_formation_credit);
    require(completed.units().front().formation_group_id == 0);
    require(completed.units().front().movement_speed_remainder == 0);
    require_clean_cavalry_cadence(
        completed, completed_scout, {34, 2}
    );

    aoe::Simulation redirected(aoe::GameMap(40, 6));
    const aoe::EntityId redirected_scout = redirected.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 2}
    );
    redirected.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {39, 5}
    );
    require(redirected.command_formation(
        {redirected_scout}, {30, 2}, aoe::FormationKind::line
    ));
    redirected.update();
    require(
        redirected.units().front().movement_speed_remainder > 320
    );
    require_clean_cavalry_cadence(
        redirected, redirected_scout, {34, 2}
    );

    aoe::Simulation stopped(aoe::GameMap(40, 6));
    const aoe::EntityId stopped_scout = stopped.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 2}
    );
    stopped.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {39, 5}
    );
    require(stopped.command_formation(
        {stopped_scout}, {30, 2}, aoe::FormationKind::line
    ));
    stopped.update();
    require(stopped.units().front().movement_speed_remainder > 320);
    require(stopped.stop_unit(stopped_scout));
    require(stopped.units().front().movement_speed_remainder == 0);

    aoe::Simulation individual(aoe::GameMap(40, 6));
    const aoe::EntityId individual_scout = individual.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::blue, {2, 2}
    );
    individual.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {39, 5}
    );
    require(individual.command_unit(individual_scout, {20, 2}));
    individual.update();
    individual.update();
    const int valid_individual_remainder =
        individual.units().front().movement_speed_remainder;
    require(valid_individual_remainder > 0);
    require(individual.command_unit(individual_scout, {34, 2}));
    require(
        individual.units().front().movement_speed_remainder ==
        valid_individual_remainder
    );

    aoe::Simulation unique(aoe::GameMap(40, 6));
    unique.replace_technologies(
        aoe::Player::blue,
        {aoe::Technology::wheelbarrow}
    );
    const aoe::EntityId wheelbarrow_villager = unique.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    unique.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {39, 5}
    );
    require(unique.command_formation(
        {wheelbarrow_villager}, {24, 2}, aoe::FormationKind::line
    ));
    unique.update();
    require(unique.units().front().movement_speed_remainder > 100);
    require(unique.command_unit(wheelbarrow_villager, {34, 2}));
    require(unique.units().front().movement_speed_remainder == 0);
    const int unique_origin_x = unique.units().front().position.x;
    unique.update();
    require(unique.units().front().position.x == unique_origin_x + 1);
    require(
        unique.units().front().movement_speed_remainder > 0 &&
        unique.units().front().movement_speed_remainder < 100
    );
}

void computer_strategy_is_configurable_visible_and_persistent() {
    bool invalid_difficulty_rejected = false;
    try {
        aoe::ComputerPlayer invalid(
            aoe::Player::red,
            static_cast<aoe::ComputerDifficulty>(99)
        );
    } catch (const std::invalid_argument&) {
        invalid_difficulty_rejected = true;
    }
    require(invalid_difficulty_rejected);

    require(aoe::computer_target_acquisition_radius(
        aoe::ComputerDifficulty::easiest, 7
    ) == 7);
    require(aoe::computer_target_acquisition_radius(
        aoe::ComputerDifficulty::easy, 7
    ) == 7);
    require(aoe::computer_target_acquisition_radius(
        aoe::ComputerDifficulty::moderate, 7
    ) == 14);
    require(aoe::computer_target_acquisition_radius(
        aoe::ComputerDifficulty::hard, 7
    ) == 14);
    require(aoe::computer_target_acquisition_radius(
        aoe::ComputerDifficulty::hardest, 7
    ) == 14);
    require(aoe::classic_ai_difficulty_profile(
        aoe::ComputerDifficulty::easiest
    ).enemy_sighted_response_percent == 10);
    require(aoe::classic_ai_difficulty_profile(
        aoe::ComputerDifficulty::moderate
    ).maintain_distance_error_percent == 50);
    require(aoe::classic_ai_difficulty_profile(
        aoe::ComputerDifficulty::hardest
    ).dodge_missile_error_percent == 0);
    require(aoe::classic_ai_gather_plan(
        aoe::Age::dark, 8, 0, false, true,
        aoe::ComputerDifficulty::moderate
    ).percentages == std::array<int, 4>{0, 100, 0, 0});
    require(aoe::classic_ai_gather_plan(
        aoe::Age::feudal, 20, 1, false, false,
        aoe::ComputerDifficulty::moderate
    ).percentages == std::array<int, 4>{45, 40, 15, 0});
    require(aoe::classic_ai_gather_plan(
        aoe::Age::feudal, 20, 1, true, false,
        aoe::ComputerDifficulty::moderate
    ).percentages == std::array<int, 4>{25, 55, 20, 0});
    require(aoe::classic_ai_gather_plan(
        aoe::Age::castle, 30, 2, false, true,
        aoe::ComputerDifficulty::moderate
    ).percentages == std::array<int, 4>{35, 35, 15, 15});
    require(aoe::classic_ai_villager_target(
        aoe::Age::dark, 200, aoe::ComputerDifficulty::moderate
    ) == 15);
    require(aoe::classic_ai_villager_target(
        aoe::Age::feudal, 100, aoe::ComputerDifficulty::hard
    ) == 40);
    require(aoe::classic_ai_villager_target(
        aoe::Age::castle, 200, aoe::ComputerDifficulty::hardest
    ) == 70);
    require(aoe::classic_ai_attack_profile(
        aoe::ComputerDifficulty::easiest
    ).initial_delay == 1800);
    require(aoe::classic_ai_attack_profile(
        aoe::ComputerDifficulty::easy
    ).repeat_interval == 120);
    require(aoe::classic_ai_attack_profile(
        aoe::ComputerDifficulty::hardest
    ).minimum_age == aoe::Age::feudal);
    {
        aoe::Simulation bonus(aoe::GameMap(8, 8));
        bonus.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {1, 1}
        );
        bonus.add_building(
            aoe::BuildingKind::house, aoe::Player::blue, {6, 6}
        );
        bonus.replace_ages(aoe::Age::dark, aoe::Age::imperial);
        bonus.replace_state(
            bonus.units(), bonus.buildings(),
            {100, 100, 100, 100}, {100, 100, 100, 100}, 0
        );
        aoe::ComputerPlayer bonus_ai(
            aoe::Player::red, aoe::ComputerDifficulty::hardest
        );
        auto bonus_state = bonus_ai.state();
        bonus_state.resource_bonus_timer_armed = true;
        bonus_state.next_resource_bonus_tick = 3;
        bonus_ai.restore_state(bonus_state);
        for (int tick = 0; tick < 3; ++tick) bonus.update();
        bonus_ai.update(bonus);
        require(bonus.economy(aoe::Player::red).wood == 600);
        require(bonus.economy(aoe::Player::red).food == 600);
        require(bonus_ai.state().next_resource_bonus_tick == 1203);
    }
    bool invalid_player_rejected = false;
    try {
        aoe::ComputerPlayer invalid_player(
            static_cast<aoe::Player>(99)
        );
    } catch (const std::invalid_argument&) {
        invalid_player_rejected = true;
    }
    require(invalid_player_rejected);
    invalid_player_rejected = false;
    try {
        aoe::ComputerPlayer valid_player(aoe::Player::red);
        auto state = valid_player.state();
        state.player = static_cast<aoe::Player>(99);
        valid_player.restore_state(state);
    } catch (const std::invalid_argument&) {
        invalid_player_rejected = true;
    }
    require(invalid_player_rejected);

    for (aoe::UnitKind survivor : {
             aoe::UnitKind::monk,
             aoe::UnitKind::missionary,
             aoe::UnitKind::transport_ship,
             aoe::UnitKind::fishing_ship,
             aoe::UnitKind::trade_cart,
             aoe::UnitKind::trade_cog,
         }) {
        aoe::GameMap map(10, 8);
        if (aoe::is_ship(survivor)) {
            map.set_terrain({7, 4}, aoe::Terrain::water);
        }
        aoe::Simulation survival(std::move(map));
        survival.add_unit(survivor, aoe::Player::red, {7, 4});
        survival.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
        );
        for (int tick = 0; tick < 5; ++tick) survival.update();
        aoe::ComputerPlayer survivor_ai(aoe::Player::red);
        survivor_ai.update(survival);
        require(survival.outcome() == aoe::MatchOutcome::ongoing);
    }
    aoe::ComputerPlayer validated(aoe::Player::red);
    invalid_difficulty_rejected = false;
    try {
        validated.set_difficulty(
            static_cast<aoe::ComputerDifficulty>(99)
        );
    } catch (const std::invalid_argument&) {
        invalid_difficulty_rejected = true;
    }
    require(invalid_difficulty_rejected);

    for (aoe::ComputerDifficulty difficulty : {
             aoe::ComputerDifficulty::easiest,
             aoe::ComputerDifficulty::easy,
             aoe::ComputerDifficulty::moderate,
             aoe::ComputerDifficulty::hard,
             aoe::ComputerDifficulty::hardest,
         }) {
        aoe::Simulation no_handicap(aoe::GameMap(8, 8));
        no_handicap.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {1, 1}
        );
        no_handicap.add_building(
            aoe::BuildingKind::house, aoe::Player::blue, {6, 6}
        );
        no_handicap.replace_state(
            no_handicap.units(), no_handicap.buildings(),
            {111, 222, 333, 444}, {123, 234, 345, 456}, 0
        );
        aoe::ComputerPlayer computer(aoe::Player::red, difficulty);
        for (int tick = 0; tick < 12; ++tick) {
            no_handicap.update();
            computer.update(no_handicap);
        }
        require(no_handicap.economy(aoe::Player::red).wood == 123);
        require(no_handicap.economy(aoe::Player::red).food == 234);
        require(no_handicap.economy(aoe::Player::red).gold == 345);
        require(no_handicap.economy(aoe::Player::red).stone == 456);
    }

    const auto make_scouting = [] {
        aoe::Simulation simulation(aoe::GameMap(32, 8));
        simulation.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::red, {28, 4}
        );
        simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue, {1, 4}
        );
        return simulation;
    };
    auto easy_simulation = make_scouting();
    aoe::ComputerPlayer easy(
        aoe::Player::red, aoe::ComputerDifficulty::easy
    );
    for (int tick = 0; tick < 5; ++tick) easy_simulation.update();
    easy.update(easy_simulation);
    require(!easy_simulation.units()[0].moving);
    auto expert_simulation = make_scouting();
    aoe::ComputerPlayer expert(
        aoe::Player::red, aoe::ComputerDifficulty::expert
    );
    for (int tick = 0; tick < 3; ++tick) expert_simulation.update();
    expert.update(expert_simulation);
    require(expert_simulation.units()[0].moving);
    require(
        expert_simulation.units()[0].destination !=
        expert_simulation.units()[1].position
    );
    require(expert_simulation.units()[0].destination.y > 0);
    require(expert_simulation.units()[0].destination.y < 7);
    require(expert.status().objective == aoe::ComputerObjective::scout);

    const auto ai_path = std::filesystem::temp_directory_path() /
        "aoe-computer-player.state";
    aoe::save_computer_player(expert, ai_path);
    aoe::ComputerPlayer restored_ai =
        aoe::load_computer_player(ai_path);
    std::filesystem::remove(ai_path);
    require(restored_ai.player() == aoe::Player::red);
    require(
        restored_ai.difficulty() == aoe::ComputerDifficulty::expert
    );
    require(
        restored_ai.state().last_command_tick ==
        expert.state().last_command_tick
    );
    require(
        restored_ai.state().strategy_epoch ==
        expert.state().strategy_epoch
    );

    const auto legacy_ai_path =
        std::filesystem::temp_directory_path() /
        "aoe-computer-player-v2.state";
    {
        std::ofstream legacy(legacy_ai_path);
        legacy << "AOE-COMPUTER-PLAYER 2\n"
               << static_cast<int>(aoe::Player::red)
               << " 0 11 12 13 99 -1 -1 -1 -1 0\n";
    }
    const aoe::ComputerPlayer migrated_legacy =
        aoe::load_computer_player(legacy_ai_path);
    std::filesystem::remove(legacy_ai_path);
    require(
        migrated_legacy.difficulty() == aoe::ComputerDifficulty::easy
    );
    require(migrated_legacy.state().last_target_id == 99);

    aoe::Simulation counter(aoe::GameMap(20, 10));
    const auto barracks = counter.add_building(
        aoe::BuildingKind::barracks, aoe::Player::red, {12, 2}
    );
    counter.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {15, 7}
    );
    counter.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    counter.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {10, 6}
    );
    counter.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {8, 6}
    );
    counter.replace_state(
        counter.units(), counter.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    counter.replace_ages(aoe::Age::dark, aoe::Age::feudal);
    counter.replace_technologies(
        aoe::Player::red, {aoe::Technology::man_at_arms}
    );
    for (int tick = 0; tick < 5; ++tick) counter.update();
    aoe::ComputerPlayer counter_ai(aoe::Player::red);
    counter_ai.update(counter);
    const auto producing = std::ranges::find_if(
        counter.buildings(), [barracks](const aoe::Building& building) {
            return building.id == barracks;
        }
    );
    require(!producing->production_queue.empty());
    require(producing->production_queue.front().kind ==
        aoe::UnitKind::spearman);
    require(counter_ai.status().desired_counter ==
        aoe::UnitKind::spearman);

    aoe::Simulation threshold(aoe::GameMap(40, 12));
    threshold.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {30, 2}
    );
    for (int index = 0; index < 3; ++index) {
        threshold.add_unit(
            aoe::UnitKind::militia, aoe::Player::red,
            {20, 4 + index}
        );
    }
    threshold.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {17, 5}
    );
    {
        auto passive = threshold.units();
        for (auto& unit : passive) unit.stance = aoe::UnitStance::passive;
        threshold.replace_state(
            std::move(passive), threshold.buildings(),
            threshold.economy(aoe::Player::blue),
            threshold.economy(aoe::Player::red), 0
        );
    }
    for (int tick = 0; tick < 5; ++tick) threshold.update();
    aoe::ComputerPlayer threshold_ai(aoe::Player::red);
    threshold_ai.update(threshold);
    require(threshold_ai.status().objective ==
        aoe::ComputerObjective::regroup);
    require(std::ranges::none_of(
        threshold.units().begin(), threshold.units().begin() + 3,
        [](const aoe::Unit& unit) {
            return unit.attack_target_id != 0 || unit.attack_moving;
        }
    ));

    aoe::Simulation relics(aoe::GameMap(24, 10));
    relics.add_building(
        aoe::BuildingKind::monastery, aoe::Player::red, {2, 2}
    );
    relics.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {21, 7}
    );
    relics.add_unit(
        aoe::UnitKind::monk, aoe::Player::red, {6, 4}
    );
    relics.add_unit(
        aoe::UnitKind::relic, aoe::Player::neutral, {8, 4}
    );
    relics.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {21, 6}
    );
    {
        auto passive = relics.units();
        passive.front().stance = aoe::UnitStance::passive;
        relics.replace_state(
            std::move(passive), relics.buildings(),
            relics.economy(aoe::Player::blue),
            relics.economy(aoe::Player::red), 0
        );
    }
    aoe::ComputerPlayer relic_ai(aoe::Player::red);
    for (int tick = 0; tick < 160; ++tick) {
        relics.update();
        relic_ai.update(relics);
    }
    require(relics.buildings()[0].relic_count == 1);

    aoe::Simulation spanish(aoe::GameMap(24, 10));
    const auto spanish_monastery = spanish.add_building(
        aoe::BuildingKind::monastery, aoe::Player::red, {2, 2}
    );
    spanish.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 2}
    );
    spanish.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {21, 7}
    );
    spanish.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {8, 4}
    );
    spanish.add_unit(
        aoe::UnitKind::relic, aoe::Player::neutral, {9, 4}
    );
    spanish.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {21, 6}
    );
    spanish.replace_state(
        spanish.units(), spanish.buildings(),
        {1000, 1000, 1000, 1000}, {1000, 1000, 1000, 1000}, 0
    );
    spanish.replace_civilizations(
        aoe::Civilization::generic, aoe::Civilization::spanish
    );
    spanish.replace_ages(aoe::Age::dark, aoe::Age::castle);
    for (int tick = 0; tick < 5; ++tick) spanish.update();
    aoe::ComputerPlayer spanish_ai(aoe::Player::red);
    spanish_ai.update(spanish);
    const auto spanish_queue = std::ranges::find_if(
        spanish.buildings(),
        [spanish_monastery](const aoe::Building& building) {
            return building.id == spanish_monastery;
        }
    );
    require(!spanish_queue->production_queue.empty());
    require(spanish_queue->production_queue.front().kind ==
        aoe::UnitKind::monk);
    for (int tick = 0; tick < 220; ++tick) {
        spanish.update();
        spanish_ai.update(spanish);
    }
    require(spanish.buildings()[0].relic_count == 1);

    aoe::GameMap disconnected_map(14, 8);
    disconnected_map.set_terrain({2, 3}, aoe::Terrain::water);
    disconnected_map.set_terrain({11, 3}, aoe::Terrain::water);
    aoe::Simulation disconnected(std::move(disconnected_map));
    const auto stranded_passenger = disconnected.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {1, 3}
    );
    const auto stranded_transport = disconnected.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::red, {2, 3}
    );
    disconnected.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {10, 4}
    );
    disconnected.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {12, 4}
    );
    require(disconnected.command_embark(
        stranded_passenger, stranded_transport
    ));
    for (int tick = 0; tick < 5; ++tick) disconnected.update();
    aoe::ComputerPlayer disconnected_ai(aoe::Player::red);
    disconnected_ai.update(disconnected);
    const auto released = std::ranges::find_if(
        disconnected.units(),
        [stranded_passenger](const aoe::Unit& unit) {
            return unit.id == stranded_passenger;
        }
    );
    require(released != disconnected.units().end());
    require(released->garrisoned_in == 0);
    require(disconnected.units()[1].position ==
        aoe::TilePosition(2, 3));

    aoe::GameMap crossing_map(20, 10);
    for (int y = 0; y < crossing_map.height(); ++y) {
        for (int x = 3; x < 17; ++x) {
            crossing_map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    aoe::Simulation crossing(std::move(crossing_map));
    const auto passenger = crossing.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {2, 5}
    );
    crossing.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::red, {3, 5}
    );
    crossing.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {17, 6}
    );
    crossing.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {18, 5}
    );
    {
        auto passive = crossing.units();
        for (auto& unit : passive) unit.stance = aoe::UnitStance::passive;
        crossing.replace_state(
            std::move(passive), crossing.buildings(),
            crossing.economy(aoe::Player::blue),
            crossing.economy(aoe::Player::red), 0
        );
    }
    aoe::ComputerPlayer transport_ai(aoe::Player::red);
    bool embarked = false;
    bool landed = false;
    for (int tick = 0; tick < 500; ++tick) {
        crossing.update();
        transport_ai.update(crossing);
        const auto unit = std::ranges::find_if(
            crossing.units(), [passenger](const aoe::Unit& candidate) {
                return candidate.id == passenger;
            }
        );
        if (unit == crossing.units().end()) break;
        embarked = embarked || unit->garrisoned_in != 0;
        if (embarked && unit->garrisoned_in == 0 &&
            unit->position.x >= 17) {
            landed = true;
            break;
        }
    }
    require(embarked);
    require(landed);

    aoe::Simulation army(aoe::GameMap(36, 14));
    army.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {24, 2}
    );
    for (int index = 0; index < 5; ++index) {
        army.add_unit(
            index == 4 ? aoe::UnitKind::archer
                       : aoe::UnitKind::militia,
            aoe::Player::red, {20 + index % 2, 4 + index}
        );
    }
    army.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {18, 6}
    );
    {
        std::vector<aoe::Unit> passive = army.units();
        for (aoe::Unit& unit : passive) {
            unit.stance = aoe::UnitStance::passive;
        }
        army.replace_state(
            std::move(passive), army.buildings(),
            army.economy(aoe::Player::blue),
            army.economy(aoe::Player::red), 0
        );
    }
    for (int tick = 0; tick < 5; ++tick) army.update();
    aoe::ComputerPlayer strategist(aoe::Player::red);
    strategist.update(army);
    require(strategist.status().objective ==
        aoe::ComputerObjective::defend);
    require(strategist.status().desired_counter ==
        aoe::UnitKind::spearman);
    require(strategist.status().melee_units == 4);
    require(strategist.status().ranged_units == 1);
    require(std::ranges::all_of(
        army.units().begin(), army.units().begin() + 5,
        [](const aoe::Unit& unit) {
            return unit.formation_group_id != 0 &&
                unit.attack_moving;
        }
    ));

    aoe::GameMap water(24, 10);
    for (int y = 0; y < water.height(); ++y) {
        for (int x = 0; x < water.width(); ++x) {
            water.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    aoe::Simulation naval(std::move(water));
    naval.add_unit(
        aoe::UnitKind::galley, aoe::Player::red, {15, 4}
    );
    naval.add_unit(
        aoe::UnitKind::galley, aoe::Player::blue, {11, 4}
    );
    for (int tick = 0; tick < 5; ++tick) naval.update();
    aoe::ComputerPlayer admiral(aoe::Player::red);
    admiral.update(naval);
    require(admiral.status().objective ==
        aoe::ComputerObjective::naval);
    require(admiral.status().naval_units == 1);
    require(naval.units()[0].attack_moving ||
        naval.units()[0].attack_target_id != 0);

    aoe::Simulation castle_ai(aoe::GameMap(20, 12));
    const aoe::EntityId castle = castle_ai.add_building(
        aoe::BuildingKind::castle, aoe::Player::red, {10, 2}
    );
    castle_ai.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    castle_ai.replace_state(
        castle_ai.units(), castle_ai.buildings(),
        {5000, 5000, 5000, 5000},
        {5000, 5000, 5000, 5000}, 0
    );
    castle_ai.replace_ages(aoe::Age::dark, aoe::Age::imperial);
    castle_ai.replace_technologies(
        aoe::Player::red,
        {aoe::Technology::hoardings, aoe::Technology::sappers}
    );
    for (int tick = 0; tick < 5; ++tick) castle_ai.update();
    aoe::ComputerPlayer castle_strategy(aoe::Player::red);
    castle_strategy.update(castle_ai);
    const auto built_castle = std::ranges::find_if(
        castle_ai.buildings(), [castle](const aoe::Building& building) {
            return building.id == castle;
        }
    );
    require(built_castle != castle_ai.buildings().end());
    require(built_castle->technology_research_ticks_remaining > 0);
    require(built_castle->technology_research_target ==
        aoe::Technology::conscription);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-ai-determinism.save";
    const auto state_path = std::filesystem::temp_directory_path() /
        "aoe-ai-determinism.state";
    aoe::save_game(army, save_path);
    aoe::save_computer_player(strategist, state_path);
    aoe::Simulation twin = aoe::load_game(save_path);
    aoe::ComputerPlayer twin_ai =
        aoe::load_computer_player(state_path);
    std::filesystem::remove(save_path);
    std::filesystem::remove(state_path);
    for (int tick = 0; tick < 80; ++tick) {
        army.update();
        strategist.update(army);
        twin.update();
        twin_ai.update(twin);
    }
    require(army.tick_number() == twin.tick_number());
    require(army.economy(aoe::Player::red).wood ==
        twin.economy(aoe::Player::red).wood);
    require(army.economy(aoe::Player::red).food ==
        twin.economy(aoe::Player::red).food);
    require(army.economy(aoe::Player::red).gold ==
        twin.economy(aoe::Player::red).gold);
    require(army.economy(aoe::Player::red).stone ==
        twin.economy(aoe::Player::red).stone);
    require(army.units().size() == twin.units().size());
    for (std::size_t index = 0; index < army.units().size(); ++index) {
        require(army.units()[index].id == twin.units()[index].id);
        require(
            army.units()[index].position == twin.units()[index].position
        );
        require(
            army.units()[index].hit_points ==
            twin.units()[index].hit_points
        );
    }
}

void computer_handles_mixed_domains_and_persists_deterministically() {
    aoe::GameMap map(24, 14);
    for (int y = 0; y < map.height(); ++y) {
        map.set_terrain({12, y}, aoe::Terrain::beach);
        map.set_terrain({13, y}, aoe::Terrain::shallows);
        for (int x = 14; x < map.width(); ++x) {
            map.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    map.set_terrain({17, 4}, aoe::Terrain::fish);
    map.set_resource_amount({17, 4}, 2);
    aoe::Simulation first(std::move(map));
    first.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {0, 0}
    );
    first.add_building(
        aoe::BuildingKind::mill, aoe::Player::red, {4, 5}
    );
    const aoe::EntityId farm = first.add_building(
        aoe::BuildingKind::farm, aoe::Player::red, {7, 5}
    );
    const aoe::EntityId shelter = first.add_building(
        aoe::BuildingKind::watch_tower, aoe::Player::red, {7, 9}
    );
    first.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {2, 5}
    );
    first.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {0, 10}
    );
    const aoe::EntityId farmer = first.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 5}
    );
    first.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {10, 4}
    );
    const aoe::EntityId refugee = first.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {8, 9}
    );
    const aoe::EntityId fisher = first.add_unit(
        aoe::UnitKind::fishing_ship, aoe::Player::red, {14, 4}
    );
    first.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {8, 11}
    );
    first.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {5, 12}
    );
    first.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {6, 12}
    );
    const aoe::EntityId spoiled = first.add_unit(
        aoe::UnitKind::deer, aoe::Player::blue, {6, 7}
    );
    std::vector<aoe::Building> buildings = first.buildings();
    buildings[2].resource_amount = 6;
    std::vector<aoe::Unit> units = first.units();
    units.back().hit_points = 0;
    units.back().food_remaining = 0;
    first.replace_state(
        std::move(units), std::move(buildings),
        first.economy(aoe::Player::blue),
        {500, 500, 500, 500}, 0
    );
    first.replace_ages(aoe::Age::dark, aoe::Age::feudal);
    first.replace_technologies(
        aoe::Player::red, {aoe::Technology::fish_trap_gate}
    );
    aoe::MatchRules stress_rules;
    stress_rules.conquest_enabled = false;
    stress_rules.wonder_enabled = false;
    stress_rules.relic_enabled = false;
    first.set_match_rules(stress_rules);
    require(first.command_unit(farmer, {7, 5}));

    aoe::ComputerPlayer first_ai(
        aoe::Player::red, aoe::ComputerDifficulty::hard
    );
    for (int tick = 0; tick < 4; ++tick) {
        first.update();
        first_ai.update(first);
    }
    require(first.farm_reseed_queue(aoe::Player::red) == 1);
    const auto refugee_state = std::ranges::find(
        first.units(), refugee, &aoe::Unit::id
    );
    require(refugee_state != first.units().end());
    require(
        refugee_state->garrison_target_id == shelter ||
        refugee_state->garrisoned_in == shelter
    );
    const auto fisher_state = std::ranges::find(
        first.units(), fisher, &aoe::Unit::id
    );
    require(fisher_state != first.units().end());
    require(fisher_state->has_resource_target);
    require(std::ranges::none_of(
        first.units(), [spoiled](const aoe::Unit& unit) {
            return unit.id == spoiled;
        }
    ));

    for (int tick = 0; tick < 30; ++tick) {
        first.update();
        first_ai.update(first);
    }
    require(std::ranges::any_of(
        first.buildings(), [](const aoe::Building& building) {
            return building.owner == aoe::Player::red &&
                building.kind == aoe::BuildingKind::dock;
        }
    ));
    require(first.farm_reseed_queue(aoe::Player::red) == 0);
    const auto replanted = std::ranges::find(
        first.buildings(), farm, &aoe::Building::id
    );
    require(replanted != first.buildings().end());
    require(replanted->resource_amount > 0);

    const auto game_path = std::filesystem::temp_directory_path() /
        "aoe-ai-mixed.save";
    const auto ai_path = std::filesystem::temp_directory_path() /
        "aoe-ai-mixed.state";
    aoe::save_game(first, game_path);
    aoe::save_computer_player(first_ai, ai_path);
    aoe::Simulation second = aoe::load_game(game_path);
    aoe::ComputerPlayer second_ai = aoe::load_computer_player(ai_path);
    std::filesystem::remove(game_path);
    std::filesystem::remove(ai_path);

    for (int tick = 0; tick < 120; ++tick) {
        first.update();
        first_ai.update(first);
        second.update();
        second_ai.update(second);
    }
    require(first.tick_number() == second.tick_number());
    require(first.economy(aoe::Player::red).wood ==
            second.economy(aoe::Player::red).wood);
    require(first.economy(aoe::Player::red).food ==
            second.economy(aoe::Player::red).food);
    require(first.units().size() == second.units().size());
    require(first.buildings().size() == second.buildings().size());
    require(first.farm_reseed_queue(aoe::Player::red) ==
            second.farm_reseed_queue(aoe::Player::red));
    require(first_ai.state().strategy_epoch ==
            second_ai.state().strategy_epoch);
    require(std::ranges::any_of(
        first.buildings(), [](const aoe::Building& building) {
            return building.owner == aoe::Player::red &&
                building.kind == aoe::BuildingKind::fish_trap;
        }
    ));
    for (std::size_t index = 0; index < first.units().size(); ++index) {
        require(first.units()[index].id == second.units()[index].id);
        require(first.units()[index].position ==
                second.units()[index].position);
        require(first.units()[index].resource_building_id ==
                second.units()[index].resource_building_id);
        require(first.units()[index].food_remaining ==
                second.units()[index].food_remaining);
    }

    aoe::ComputerPlayer first_blue_ai(
        aoe::Player::blue, aoe::ComputerDifficulty::standard
    );
    aoe::ComputerPlayer second_blue_ai = first_blue_ai;
    const std::uint64_t stress_start_tick = first.tick_number();
    for (int tick = 0; tick < 2000; ++tick) {
        first.update();
        first_ai.update(first);
        first_blue_ai.update(first);
        second.update();
        second_ai.update(second);
        second_blue_ai.update(second);
        if (tick == 999) {
            const auto stress_game =
                std::filesystem::temp_directory_path() /
                "aoe-ai-stress-mid.save";
            const auto stress_red =
                std::filesystem::temp_directory_path() /
                "aoe-ai-stress-red.state";
            const auto stress_blue =
                std::filesystem::temp_directory_path() /
                "aoe-ai-stress-blue.state";
            aoe::save_game(second, stress_game);
            aoe::save_computer_player(second_ai, stress_red);
            aoe::save_computer_player(second_blue_ai, stress_blue);
            second = aoe::load_game(stress_game);
            second_ai = aoe::load_computer_player(stress_red);
            second_blue_ai = aoe::load_computer_player(stress_blue);
            std::filesystem::remove(stress_game);
            std::filesystem::remove(stress_red);
            std::filesystem::remove(stress_blue);
        }
        if (tick % 100 != 99) continue;
        require(first.tick_number() == second.tick_number());
        require(first.outcome() == second.outcome());
        require(first.units().size() == second.units().size());
        require(first.buildings().size() == second.buildings().size());
        require(first.economy(aoe::Player::blue).food ==
                second.economy(aoe::Player::blue).food);
        require(first.economy(aoe::Player::red).wood ==
                second.economy(aoe::Player::red).wood);
        require(first_ai.state().strategy_epoch ==
                second_ai.state().strategy_epoch);
        require(first_blue_ai.state().strategy_epoch ==
                second_blue_ai.state().strategy_epoch);
        for (std::size_t index = 0; index < first.units().size(); ++index) {
            require(first.units()[index].id == second.units()[index].id);
            require(first.units()[index].kind ==
                    second.units()[index].kind);
            require(first.units()[index].position ==
                    second.units()[index].position);
            require(first.units()[index].hit_points ==
                    second.units()[index].hit_points);
            require(first.units()[index].carried_amount ==
                    second.units()[index].carried_amount);
        }
    }
    require(first.tick_number() == stress_start_tick + 2000);
    require(first.outcome() == aoe::MatchOutcome::ongoing);
}

}  // namespace

void executable_scenario_triggers_are_deterministic_and_persistent() {
    aoe::Scenario scenario(12, 12);
    scenario.blue_economy = {};
    scenario.red_economy = {};
    scenario.match_rules.conquest_enabled = false;
    scenario.match_rules.wonder_enabled = false;
    scenario.match_rules.relic_enabled = false;
    scenario.objectives = {{
        1, aoe::Player::blue, true, false, "Gather tribute.",
    }};
    scenario.triggers = {
        {1, 100, true, false, "elapsed_ticks >= 1",
         "add_resource blue food 25"},
        {2, 90, true, false, "resource blue food >= 25",
         "complete_objective 1"},
        {3, 80, true, true, "elapsed_ticks >= 1",
         "add_resource blue wood 2"},
        {4, 70, true, false, "elapsed_ticks >= 2",
         "create_unit villager blue 2 2"},
        {5, 60, true, false, "area_presence blue 0 0 4 4 >= 1",
         "message player=blue ticks=30 audio=\"voice.mp3\" "
         "text=\"villagers_present\""},
        {6, 50, true, false, "elapsed_ticks >= 2",
         "diplomacy ally"},
        {7, 0, true, false, "elapsed_ticks >= 4",
         "victory blue"},
    };

    aoe::Simulation simulation = aoe::create_simulation(scenario);
    simulation.update();
    require(simulation.economy(aoe::Player::blue).food == 25);
    require(simulation.economy(aoe::Player::blue).wood == 2);
    require(!simulation.objectives().front().completed);
    require(!simulation.triggers()[0].enabled);
    require(simulation.triggers()[2].enabled);

    simulation.update();
    require(simulation.units().size() == 1);
    require(simulation.scenario_messages().empty());
    require(simulation.objectives().front().completed);
    require(simulation.diplomacy(
        aoe::Player::blue, aoe::Player::red
    ) == aoe::Diplomacy::ally);
    require(simulation.economy(aoe::Player::blue).wood == 4);
    simulation.update();
    require(simulation.scenario_messages().size() == 1);
    require(
        simulation.scenario_messages().front().audio_file == "voice.mp3"
    );

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-trigger-runtime-v100.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.objectives().front().completed);
    require(loaded.scenario_messages() == simulation.scenario_messages());
    require(loaded.triggers().size() == simulation.triggers().size());

    require(loaded.economy(aoe::Player::blue).wood ==
            simulation.economy(aoe::Player::blue).wood);

    aoe::Scenario destroyed_reference(6, 6);
    destroyed_reference.match_rules = scenario.match_rules;
    destroyed_reference.buildings.push_back({
        aoe::BuildingKind::house, aoe::Player::red, {3, 3}
    });
    destroyed_reference.triggers = {{
        1, 1, true, false, "building_destroyed 1", "victory blue",
    }};
    aoe::Simulation destroyed = aoe::create_simulation(destroyed_reference);
    const auto destroyed_path = std::filesystem::temp_directory_path() /
        "aoe-destroyed-trigger-reference.save";
    aoe::save_game(destroyed, destroyed_path);
    {
        std::ifstream input(destroyed_path);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.starts_with("building ")) lines.push_back(line);
        }
        std::ofstream output(destroyed_path);
        for (const std::string& value : lines) output << value << '\n';
    }
    aoe::Simulation destroyed_loaded = aoe::load_game(destroyed_path);
    std::filesystem::remove(destroyed_path);
    destroyed_loaded.update();
    require(destroyed_loaded.outcome() == aoe::MatchOutcome::blue_victory);

    simulation.update();
    loaded.update();
    require(simulation.outcome() == aoe::MatchOutcome::blue_victory);
    require(loaded.outcome() == simulation.outcome());
    const auto frozen_tick = simulation.tick_number();
    simulation.update();
    require(simulation.tick_number() == frozen_tick);

    aoe::Scenario legacy = scenario;
    legacy.strict_trigger_syntax = false;
    legacy.triggers = {{
        11, 1, true, false, "unknown_legacy_condition",
        "unknown_legacy_effect",
    }};
    aoe::Simulation inert = aoe::create_simulation(legacy);
    require(!inert.triggers().front().executable);
    inert.update();
    require(inert.outcome() == aoe::MatchOutcome::ongoing);
    const auto inert_path = std::filesystem::temp_directory_path() /
        "aoe-inert-trigger-v102.save";
    aoe::save_game(inert, inert_path);
    aoe::Simulation inert_loaded = aoe::load_game(inert_path);
    std::filesystem::remove(inert_path);
    require(!inert_loaded.triggers().front().executable);

    aoe::Scenario periodic(6, 6);
    periodic.match_rules.conquest_enabled = false;
    periodic.match_rules.wonder_enabled = false;
    periodic.match_rules.relic_enabled = false;
    periodic.triggers = {
        {1, 10, true, true, "elapsed_ticks >= 2",
         "add_resource blue gold 1"},
        {2, 5, true, false, "elapsed_ticks >= 1",
         "message player=blue ticks=2 text=\"brief\""},
    };
    aoe::Simulation timed = aoe::create_simulation(periodic);
    timed.update();
    require(timed.economy(aoe::Player::blue).gold == 0);
    require(timed.scenario_messages().size() == 1);
    timed.update();
    require(timed.economy(aoe::Player::blue).gold == 1);
    require(timed.scenario_messages().size() == 1);
    timed.update();
    require(timed.economy(aoe::Player::blue).gold == 1);
    require(timed.scenario_messages().empty());
    timed.update();
    require(timed.economy(aoe::Player::blue).gold == 2);
    require(timed.triggers().front().fired_count == 2);

    aoe::Scenario overflow(6, 6);
    overflow.match_rules = periodic.match_rules;
    overflow.blue_economy.food = 1;
    overflow.triggers = {{
        1, 1, true, false, "elapsed_ticks >= 1",
        "add_resource blue food 2147483647",
    }};
    aoe::Simulation clamped = aoe::create_simulation(overflow);
    clamped.update();
    require(clamped.economy(aoe::Player::blue).food ==
            std::numeric_limits<int>::max());

    aoe::Simulation replay_first = aoe::create_simulation(scenario);
    aoe::Simulation replay_second = aoe::create_simulation(scenario);
    aoe::Replay first_replay;
    aoe::Replay second_replay;
    for (int tick = 0; tick < 5; ++tick) {
        first_replay.apply_current_tick(replay_first);
        second_replay.apply_current_tick(replay_second);
        replay_first.update();
        replay_second.update();
    }
    require(replay_first.outcome() == replay_second.outcome());
    require(replay_first.objectives() == replay_second.objectives());
    require(replay_first.scenario_messages() ==
            replay_second.scenario_messages());
    require(replay_first.triggers().size() ==
            replay_second.triggers().size());
    for (std::size_t index = 0; index < replay_first.triggers().size();
         ++index) {
        const auto& first = replay_first.triggers()[index];
        const auto& second = replay_second.triggers()[index];
        require(first.enabled == second.enabled);
        require(first.fired_count == second.fired_count);
        require(first.last_fired_tick == second.last_fired_tick);
    }

    aoe::Scenario malformed(6, 6);
    malformed.match_rules = periodic.match_rules;
    malformed.triggers = {{
        1, 1, true, false, "elapsed_ticks >= 1",
        "message player=blue ticks=2 text=\"unterminated",
    }};
    bool rejected = false;
    try {
        (void)aoe::create_simulation(malformed);
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected);
    const auto rejected_syntax =
        std::filesystem::temp_directory_path() /
        "aoe-rejected-trigger-syntax.scenario";
    std::filesystem::remove(rejected_syntax);
    rejected = false;
    try {
        aoe::save_scenario(malformed, rejected_syntax);
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected);
    require(!std::filesystem::exists(rejected_syntax));

    aoe::Scenario neutral_trigger(6, 6);
    neutral_trigger.match_rules = periodic.match_rules;
    neutral_trigger.triggers = {{
        1, 1, true, false, "resource neutral food >= 1",
        "victory neutral",
    }};
    const auto rejected_scenario =
        std::filesystem::temp_directory_path() /
        "aoe-rejected-neutral-trigger.scenario";
    std::filesystem::remove(rejected_scenario);
    rejected = false;
    try {
        aoe::save_scenario(neutral_trigger, rejected_scenario);
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected);
    require(!std::filesystem::exists(rejected_scenario));

    const auto malformed_scenario =
        std::filesystem::temp_directory_path() /
        "aoe-malformed-neutral-trigger.scenario";
    aoe::Scenario valid_trigger(6, 6);
    valid_trigger.match_rules = periodic.match_rules;
    valid_trigger.triggers = {{
        1, 1, true, false, "resource blue food >= 1",
        "victory blue",
    }};
    aoe::save_scenario(valid_trigger, malformed_scenario);
    {
        std::ifstream input(malformed_scenario);
        std::ostringstream contents;
        contents << input.rdbuf();
        std::string text = contents.str();
        const auto condition = text.find("resource blue food");
        require(condition != std::string::npos);
        text.replace(condition, std::string("resource blue food").size(),
                     "resource neutral food");
        std::ofstream output(malformed_scenario);
        output << text;
    }
    rejected = false;
    try {
        (void)aoe::load_scenario(malformed_scenario);
    } catch (const std::exception&) {
        rejected = true;
    }
    std::filesystem::remove(malformed_scenario);
    require(rejected);

    const auto malformed_save =
        std::filesystem::temp_directory_path() /
        "aoe-malformed-negative-trigger-v108.save";
    aoe::Simulation saved = aoe::create_simulation(valid_trigger);
    aoe::save_game(saved, malformed_save);
    {
        std::ifstream input(malformed_save);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line)) lines.push_back(line);
        bool changed = false;
        for (std::string& candidate : lines) {
            if (!candidate.starts_with("scenario-trigger ")) continue;
            std::istringstream tokens_input(candidate);
            std::vector<std::string> tokens;
            std::string token;
            while (tokens_input >> token) tokens.push_back(token);
            require(tokens.size() >= 36);
            tokens[14] = "-1";
            std::ostringstream rebuilt;
            for (std::size_t index = 0; index < tokens.size(); ++index) {
                if (index != 0) rebuilt << ' ';
                rebuilt << tokens[index];
            }
            candidate = rebuilt.str();
            changed = true;
        }
        require(changed);
        std::ofstream output(malformed_save);
        for (const std::string& value : lines) output << value << '\n';
    }
    rejected = false;
    try {
        (void)aoe::load_game(malformed_save);
    } catch (const std::exception&) {
        rejected = true;
    }
    std::filesystem::remove(malformed_save);
    require(rejected);

    const auto malformed_fired_count_save =
        std::filesystem::temp_directory_path() /
        "aoe-malformed-fired-count-trigger-v108.save";
    saved.update();
    aoe::save_game(saved, malformed_fired_count_save);
    {
        std::ifstream input(malformed_fired_count_save);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line)) lines.push_back(line);
        bool changed = false;
        for (std::string& candidate : lines) {
            if (!candidate.starts_with("scenario-trigger ")) continue;
            std::istringstream tokens_input(candidate);
            std::vector<std::string> tokens;
            std::string token;
            while (tokens_input >> token) tokens.push_back(token);
            require(tokens.size() >= 36);
            require(tokens[7] != "0");
            tokens[8] = "0";
            std::ostringstream rebuilt;
            for (std::size_t index = 0; index < tokens.size(); ++index) {
                if (index != 0) rebuilt << ' ';
                rebuilt << tokens[index];
            }
            candidate = rebuilt.str();
            changed = true;
        }
        require(changed);
        std::ofstream output(malformed_fired_count_save);
        for (const std::string& value : lines) output << value << '\n';
    }
    rejected = false;
    try {
        (void)aoe::load_game(malformed_fired_count_save);
    } catch (const std::exception&) {
        rejected = true;
    }
    std::filesystem::remove(malformed_fired_count_save);
    require(rejected);

    const auto old_save = std::filesystem::temp_directory_path() /
        "aoe-trigger-runtime-v100-compat.save";
    {
        std::ofstream output(old_save);
        output << "AOE-ARCHAEOLOGY-SAVE 100\n"
               << "tick 5\n"
               << "scenario-objective 1 0 1 0 0 \"old\"\n"
               << "scenario-trigger 1 1 0 0 1 "
               << "0 0 0 0 1 0 0 0 0 "
               << "2 0 2 0 3 2 1 0 0 \"\"\n"
               << "scenario-message \"old message\"\n";
    }
    aoe::Simulation migrated = aoe::load_game(old_save);
    std::filesystem::remove(old_save);
    require(migrated.triggers().front().activation_tick == 0);
    require(migrated.triggers().front().fired_count == 0);
    require(migrated.scenario_messages().front().expires_tick == 35);
    require(
        migrated.aztec_relic_gold_remainder(aoe::Player::blue) == 0
    );
}

void multi_action_triggers_are_atomic_and_persistent() {
    aoe::Scenario scenario(8, 8);
    scenario.match_rules.conquest_enabled = false;
    scenario.match_rules.wonder_enabled = false;
    scenario.match_rules.relic_enabled = false;
    scenario.blue_economy.wood = 20;
    scenario.blue_economy.food = 0;
    scenario.red_economy.wood = 0;
    scenario.units.push_back({
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    });
    scenario.objectives = {{
        1, aoe::Player::blue, true, true, "Reveal this.",
    }};
    scenario.triggers = {
        {
            1, 20, true, false,
            {"elapsed_ticks >= 1", "object_hp 1 >= 25"},
            {
                "tribute blue red wood 10",
                "research blue loom",
                "objective 1 shown",
                "activate_trigger 2",
                "message player=blue ticks=5 text=\"ready\"",
            },
        },
        {
            2, 10, false, false,
            std::vector<std::string>{"elapsed_ticks >= 1"},
            std::vector<std::string>{"add_resource blue food 7"},
        },
    };
    aoe::Simulation simulation = aoe::create_simulation(scenario);
    simulation.update();
    require(simulation.economy(aoe::Player::blue).wood == 10);
    require(simulation.economy(aoe::Player::red).wood == 10);
    require(simulation.has_technology(
        aoe::Player::blue, aoe::Technology::loom
    ));
    require(!simulation.objectives().front().hidden);
    require(simulation.scenario_messages().size() == 1);
    require(simulation.economy(aoe::Player::blue).food == 0);
    simulation.update();
    require(simulation.economy(aoe::Player::blue).food == 7);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-trigger-runtime-v108.save";
    aoe::save_game(simulation, path);
    aoe::Simulation loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.triggers().front().conditions.size() == 2);
    require(loaded.triggers().front().effects.size() == 5);
    require(state_fingerprint(loaded) == state_fingerprint(simulation));

    aoe::Scenario invalid(4, 4);
    invalid.match_rules = scenario.match_rules;
    invalid.blue_economy.wood = 1;
    invalid.blue_economy.food = 0;
    invalid.triggers = {{
        1, 1, true, false,
        std::vector<std::string>{"elapsed_ticks >= 1"},
        std::vector<std::string>{
            "add_resource blue food 99", "tribute blue red wood 2"
        },
    }};
    aoe::Simulation rolled_back = aoe::create_simulation(invalid);
    bool rejected = false;
    try {
        rolled_back.update();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected);
    require(rolled_back.economy(aoe::Player::blue).food == 0);
    require(rolled_back.economy(aoe::Player::blue).wood == 1);
}

void campaign_manifest_and_progress_are_bounded_and_atomic() {
    const auto directory = std::filesystem::temp_directory_path() /
        "aoe-campaign-contract-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory / "missions");
    for (const auto& name : {"one", "two", "three"}) {
        std::ofstream scenario(directory / "missions" /
                              (std::string(name) + ".scenario"));
        scenario << "AOE-ARCHAEOLOGY-SCENARIO 1\nmap 4 4\n";
    }
    const auto manifest = directory / "learning.campaign";
    {
        std::ofstream output(manifest);
        output << "aoe-campaign 1\n"
               << "id \"learning\"\n"
               << "name \"Learning Campaign\"\n"
               << "description \"Reconstruction-authored.\"\n"
               << "human-player blue\n"
               << "scenario 10 \"missions/one.scenario\" \"One\" "
                  "\"c1s1.mp3\" \"c1s1end.mp3\"\n"
               << "scenario 20 \"missions/two.scenario\" \"Two\"\n"
               << "scenario 30 \"missions/three.scenario\" \"Three\"\n";
    }
    const aoe::Campaign campaign = aoe::load_campaign(manifest);
    require(campaign.scenarios.size() == 3);
    require(campaign.scenarios[0].briefing_audio == "c1s1.mp3");
    require(campaign.scenarios[0].debrief_audio == "c1s1end.mp3");
    require(campaign.scenarios[1].id == 20);
    require(campaign.manifest_digest.starts_with("fnv1a64-v1:"));
    const auto canonical = directory / "canonical.campaign";
    aoe::save_campaign(campaign, canonical);
    const aoe::Campaign round_trip = aoe::load_campaign(canonical);
    require(round_trip.id == campaign.id);
    require(round_trip.name == campaign.name);
    require(round_trip.manifest_digest == campaign.manifest_digest);

    aoe::CampaignProgress progress =
        aoe::fresh_campaign_progress(campaign);
    require(progress.highest_unlocked == 10);
    require(aoe::current_campaign_scenario(campaign, progress).id == 10);
    const auto progress_path = directory / "learning.progress";
    require(!aoe::commit_campaign_outcome(
        campaign, 10, aoe::MatchOutcome::red_victory,
        progress, progress_path
    ));
    require(progress.completed.empty());
    require(aoe::commit_campaign_outcome(
        campaign, 10, aoe::MatchOutcome::blue_victory,
        progress, progress_path
    ));
    require(progress.completed == std::vector<int>{10});
    require(progress.highest_unlocked == 20);
    require(!aoe::commit_campaign_outcome(
        campaign, 30, aoe::MatchOutcome::blue_victory,
        progress, progress_path
    ));
    require(aoe::commit_campaign_outcome(
        campaign, 20, aoe::MatchOutcome::blue_victory,
        progress, progress_path
    ));
    require(aoe::commit_campaign_outcome(
        campaign, 30, aoe::MatchOutcome::blue_victory,
        progress, progress_path
    ));
    require(!aoe::next_campaign_scenario(campaign, progress));
    const auto loaded_progress =
        aoe::load_campaign_progress(campaign, progress_path);
    require(loaded_progress.status ==
            aoe::CampaignProgressStatus::current);
    require(loaded_progress.progress.completed ==
            std::vector<int>({10, 20, 30}));

    {
        std::ofstream changed(
            directory / "missions" / "two.scenario",
            std::ios::app
        );
        changed << "# changed\n";
    }
    const aoe::Campaign changed = aoe::load_campaign(manifest);
    require(changed.manifest_digest != campaign.manifest_digest);
    require(aoe::load_campaign_progress(changed, progress_path).status ==
            aoe::CampaignProgressStatus::stale);

    const auto reject_manifest = [&](const std::string& body) {
        const auto bad = directory / "bad.campaign";
        {
            std::ofstream output(bad);
            output << body;
        }
        try {
            (void)aoe::load_campaign(bad);
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };
    require(reject_manifest(
        "aoe-campaign 1\nid \"x\"\nname \"X\"\n"
        "human-player blue\nscenario 1 \"../escape.scenario\" \"X\"\n"
    ));
    require(reject_manifest(
        "aoe-campaign 1\nid \"x\"\nname \"X\"\n"
        "human-player blue\nscenario 1 \"/tmp/x.scenario\" \"X\"\n"
    ));
    require(reject_manifest(
        "aoe-campaign 1\nid \"x\"\nname \"X\"\n"
        "human-player blue\n"
        "scenario 1 \"missions/one.scenario\" \"Same\"\n"
        "scenario 1 \"missions/two.scenario\" \"Same\"\n"
    ));
    require(reject_manifest(
        "aoe-campaign 1\nid \"x\"\nname \"X\"\n"
        "human-player blue\n"
        "scenario 1 \"missions/one.scenario\" \"unterminated\n"
    ));
    require(reject_manifest(
        "aoe-campaign 1\nid \"x\"\nname \"X\"\n"
        "description \"\"\ndescription \"\"\nhuman-player blue\n"
        "scenario 1 \"missions/one.scenario\" \"One\"\n"
    ));
    aoe::Campaign invalid = campaign;
    invalid.description = "bad\nmetadata";
    bool rejected_save = false;
    try {
        aoe::save_campaign(invalid, directory / "invalid.campaign");
    } catch (const std::exception&) {
        rejected_save = true;
    }
    require(rejected_save);
    invalid = campaign;
    invalid.human_player = aoe::Player::neutral;
    rejected_save = false;
    try {
        aoe::save_campaign(invalid, directory / "invalid.campaign");
    } catch (const std::exception&) {
        rejected_save = true;
    }
    require(rejected_save);
    {
        std::ofstream duplicate(progress_path);
        duplicate << "aoe-campaign-progress 1\n"
                  << "campaign-id \"learning\"\n"
                  << "campaign-id \"learning\"\n"
                  << "manifest-digest "
                  << std::quoted(changed.manifest_digest) << '\n'
                  << "unlocked 10\n";
    }
    bool rejected_progress = false;
    try {
        (void)aoe::load_campaign_progress(changed, progress_path);
    } catch (const std::exception&) {
        rejected_progress = true;
    }
    require(rejected_progress);
    std::filesystem::remove_all(directory);
}

void campaign_trigger_save_replay_stress_is_deterministic() {
    const auto directory = std::filesystem::temp_directory_path() /
        "aoe-campaign-trigger-stress";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    for (int index = 1; index <= 3; ++index) {
        aoe::Scenario scenario(6, 6);
        scenario.match_rules.conquest_enabled = false;
        scenario.match_rules.wonder_enabled = false;
        scenario.match_rules.relic_enabled = false;
        scenario.triggers = {
            {1, 20, true, true, "elapsed_ticks >= 1",
             "add_resource blue gold 1"},
            {2, 10, true, false, "elapsed_ticks >= 3",
             "victory blue"},
        };
        aoe::save_scenario(
            scenario,
            directory / ("mission-" + std::to_string(index) + ".scenario")
        );
    }
    const auto manifest_path = directory / "stress.campaign";
    {
        std::ofstream output(manifest_path);
        output << "aoe-campaign 1\n"
               << "id \"stress\"\n"
               << "name \"Trigger Stress\"\n"
               << "human-player blue\n";
        for (int index = 1; index <= 3; ++index) {
            output << "scenario " << index << " \"mission-" << index
                   << ".scenario\" \"Mission " << index << "\"\n";
        }
    }
    const aoe::Campaign campaign = aoe::load_campaign(manifest_path);
    const auto progress_path = directory / "stress.progress";
    aoe::CampaignProgress progress =
        aoe::fresh_campaign_progress(campaign);

    require(!aoe::commit_campaign_outcome(
        campaign, 1, aoe::MatchOutcome::red_victory,
        progress, progress_path
    ));
    require(progress.highest_unlocked == 1);

    for (int scenario_id = 1; scenario_id <= 3; ++scenario_id) {
        const auto& entry =
            aoe::current_campaign_scenario(campaign, progress);
        require(entry.id == scenario_id);
        aoe::Simulation first =
            aoe::create_simulation(aoe::load_scenario(entry.path));
        first.update();
        const auto save_path = directory / "active.save";
        aoe::save_game(first, save_path);
        aoe::Simulation second = aoe::load_game(save_path);

        aoe::Replay replay;
        const auto replay_path = directory / "active.replay";
        aoe::save_replay(replay, replay_path);
        aoe::Replay loaded_replay = aoe::load_replay(replay_path);
        for (int tick = 0; tick < 4; ++tick) {
            replay.apply_current_tick(first);
            loaded_replay.apply_current_tick(second);
            first.update();
            second.update();
        }
        require(first.outcome() == aoe::MatchOutcome::blue_victory);
        require(second.outcome() == first.outcome());
        require(second.tick_number() == first.tick_number());
        require(second.economy(aoe::Player::blue).gold ==
                first.economy(aoe::Player::blue).gold);
        require(second.triggers().size() == first.triggers().size());
        for (std::size_t index = 0; index < first.triggers().size();
             ++index) {
            require(second.triggers()[index].enabled ==
                    first.triggers()[index].enabled);
            require(second.triggers()[index].fired_count ==
                    first.triggers()[index].fired_count);
            require(second.triggers()[index].last_fired_tick ==
                    first.triggers()[index].last_fired_tick);
        }
        require(aoe::commit_campaign_outcome(
            campaign, scenario_id, second.outcome(),
            progress, progress_path
        ));
        const auto reloaded =
            aoe::load_campaign_progress(campaign, progress_path);
        require(reloaded.status == aoe::CampaignProgressStatus::current);
        progress = reloaded.progress;
    }
    require(progress.completed == std::vector<int>({1, 2, 3}));
    require(!aoe::next_campaign_scenario(campaign, progress));
    std::filesystem::remove_all(directory);
}

void multiplayer_lockstep_handles_transport_disorder_deterministically() {
    const auto start_session = [](aoe::LockstepSession& session,
                                  const aoe::Simulation& simulation) {
        for (const aoe::Player player :
             {aoe::Player::blue, aoe::Player::red}) {
            const bool hello = session.receive({
                aoe::LockstepFrameKind::hello,
                aoe::lockstep_protocol_version,
                player, "scenario-v62",
            }, simulation);
            const bool ready = session.receive({
                aoe::LockstepFrameKind::ready,
                aoe::lockstep_protocol_version,
                player, "scenario-v62",
            }, simulation);
            if (!hello || !ready) {
                throw std::runtime_error("lockstep startup peer rejected");
            }
        }
        if (session.status() != aoe::LockstepStatus::ready ||
            !session.receive({
            aoe::LockstepFrameKind::start,
            aoe::lockstep_protocol_version,
            aoe::Player::blue, "scenario-v62",
        }, simulation) ||
            session.status() != aoe::LockstepStatus::running) {
            throw std::runtime_error("lockstep start rejected");
        }
    };

    aoe::Simulation first = aoe::Simulation::create_demo();
    aoe::Simulation second = aoe::Simulation::create_demo();
    aoe::LockstepSession ordered("scenario-v62", 4, 1);
    aoe::LockstepSession reordered("scenario-v62", 4, 1);
    start_session(ordered, first);
    start_session(reordered, second);
    const std::string initial_hash = aoe::deterministic_state_hash(first);
    aoe::LockstepFrame blue0{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "scenario-v62",
        0, 0, initial_hash,
        {aoe::MoveUnitCommand{1, {3, 7}}},
    };
    aoe::LockstepFrame red0{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::red,
        "scenario-v62",
        0, 0, initial_hash,
        {},
    };
    require(aoe::decode_lockstep_frame(
        aoe::encode_lockstep_frame(blue0)
    ).commands.size() == 1);
    aoe::LockstepFrame broad_codec = blue0;
    broad_codec.commands = {
        aoe::ConstructBuildingCommand{
            1, aoe::BuildingKind::house, {5, 5}
        },
    };
    require(std::holds_alternative<aoe::ConstructBuildingCommand>(
        aoe::decode_lockstep_frame(
            aoe::encode_lockstep_frame(broad_codec)
        ).commands.front()
    ));
    require(ordered.receive(blue0, first));
    require(ordered.receive(red0, first));
    require(reordered.receive(red0, second));
    require(reordered.receive(red0, second));
    require(!reordered.advance(second));
    require(reordered.receive(blue0, second));
    require(ordered.advance(first));
    require(reordered.advance(second));
    require(aoe::deterministic_state_hash(first) ==
            aoe::deterministic_state_hash(second));
    require(ordered.replay().commands().size() == 1);

    const std::string tick1_hash = aoe::deterministic_state_hash(first);
    aoe::LockstepFrame blue1{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "scenario-v62",
        1, 1, tick1_hash, {},
    };
    aoe::LockstepFrame red1 = blue1;
    red1.player = aoe::Player::red;
    require(reordered.receive(red1, second));
    require(reordered.receive(blue1, second));
    require(ordered.receive(blue1, first));
    require(ordered.receive(red1, first));
    require(ordered.advance(first));
    require(reordered.advance(second));
    require(aoe::deterministic_state_hash(first) ==
            aoe::deterministic_state_hash(second));

    aoe::Scenario future_scenario(6, 6);
    future_scenario.match_rules.conquest_enabled = false;
    future_scenario.match_rules.wonder_enabled = false;
    future_scenario.match_rules.relic_enabled = false;
    future_scenario.triggers = {{
        1, 1, true, false, "elapsed_ticks >= 1",
        "create_unit villager blue 2 2",
    }};
    aoe::Simulation future = aoe::create_simulation(future_scenario);
    aoe::LockstepSession buffered("scenario-v62", 4, 100);
    start_session(buffered, future);
    aoe::LockstepFrame future_blue{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "scenario-v62",
        1, 1, "",
        {aoe::MoveUnitCommand{1, {3, 2}}},
    };
    aoe::LockstepFrame future_red = future_blue;
    future_red.player = aoe::Player::red;
    future_red.commands.clear();
    require(buffered.receive(future_blue, future));
    require(buffered.receive(future_red, future));
    const std::string future_hash = aoe::deterministic_state_hash(future);
    aoe::LockstepFrame empty0{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "scenario-v62",
        0, 0, future_hash, {},
    };
    require(buffered.receive(empty0, future));
    empty0.player = aoe::Player::red;
    require(buffered.receive(empty0, future));
    require(buffered.advance(future));
    require(future.units().size() == 1);
    require(buffered.advance(future));
    require(buffered.status() == aoe::LockstepStatus::running);

    aoe::LockstepSession future_checkpoint("scenario-v62", 4, 2);
    start_session(future_checkpoint, future);
    aoe::LockstepFrame future_checkpoint_blue{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "scenario-v62",
        2, 2, "future-checkpoint", {},
    };
    aoe::LockstepFrame future_checkpoint_red = future_checkpoint_blue;
    future_checkpoint_red.player = aoe::Player::red;
    require(future_checkpoint.receive(future_checkpoint_blue, future));
    require(future_checkpoint.receive(future_checkpoint_red, future));
    aoe::LockstepFrame unexpected_future_hash = future_checkpoint_blue;
    unexpected_future_hash.tick = 1;
    unexpected_future_hash.sequence = 1;
    require(!future_checkpoint.receive(unexpected_future_hash, future));

    aoe::Simulation ownership_sim = aoe::Simulation::create_demo();
    aoe::LockstepSession ownership("scenario-v62", 4, 1);
    require(!ownership.connected(aoe::Player::blue));
    start_session(ownership, ownership_sim);
    aoe::LockstepFrame stolen = blue0;
    stolen.player = aoe::Player::red;
    aoe::LockstepFrame blue_empty = red0;
    blue_empty.player = aoe::Player::blue;
    require(ownership.receive(stolen, ownership_sim));
    require(ownership.receive(blue_empty, ownership_sim));
    require(!ownership.advance(ownership_sim));
    require(ownership.status() == aoe::LockstepStatus::invalid_command);

    aoe::Simulation mismatch_sim = aoe::Simulation::create_demo();
    aoe::LockstepSession mismatch("scenario-v62", 4, 1);
    start_session(mismatch, mismatch_sim);
    aoe::LockstepFrame bad_hash = red0;
    bad_hash.state_hash = "wrong";
    require(mismatch.receive(bad_hash, mismatch_sim));
    require(mismatch.receive(blue0, mismatch_sim));
    require(!mismatch.advance(mismatch_sim));
    require(mismatch.status() == aoe::LockstepStatus::desync);

    aoe::LockstepSession timeout("scenario-v62", 2, 1);
    aoe::Simulation timeout_sim = aoe::Simulation::create_demo();
    start_session(timeout, timeout_sim);
    timeout.elapse();
    aoe::LockstepFrame late_ready{
        aoe::LockstepFrameKind::ready,
        aoe::lockstep_protocol_version,
        aoe::Player::red,
        "scenario-v62",
        9, 9, "noise", {},
    };
    require(!timeout.receive(late_ready, timeout_sim));
    timeout.elapse();
    require(timeout.status() == aoe::LockstepStatus::timed_out);
    require(!timeout.advance(timeout_sim));

    aoe::LockstepSession protocol("scenario-v62");
    aoe::LockstepFrame wrong = blue0;
    wrong.kind = aoe::LockstepFrameKind::hello;
    wrong.protocol_version = aoe::lockstep_protocol_version + 1;
    require(!protocol.receive(wrong, timeout_sim));
    require(protocol.status() ==
            aoe::LockstepStatus::protocol_mismatch);

    aoe::LockstepSession digest("scenario-v62");
    wrong.protocol_version = aoe::lockstep_protocol_version;
    wrong.scenario_digest = "other";
    require(!digest.receive(wrong, timeout_sim));
    require(digest.status() == aoe::LockstepStatus::scenario_mismatch);

    aoe::LockstepSession disconnected("scenario-v62");
    start_session(disconnected, timeout_sim);
    disconnected.disconnect(aoe::Player::red);
    require(disconnected.status() == aoe::LockstepStatus::disconnected);
    require(!disconnected.connected(aoe::Player::red));
}

void lockstep_session_metadata_is_canonical_and_strict() {
    aoe::LockstepSessionConfig expected;
    expected.build_id = "build 2026.07";
    expected.scenario_digest = "scenario-v62";
    expected.content_rules_digest = "rules:original";
    expected.tick_cadence_ms = 125;
    expected.input_delay_ticks = 3;
    expected.deterministic_seed = 0x123456789abcdef0ULL;
    expected.blue = {
        "host player", aoe::Player::blue,
        aoe::Civilization::britons, 1
    };
    expected.red = {
        "join player", aoe::Player::red,
        aoe::Civilization::mayans, 2
    };

    const std::string canonical = aoe::canonical_lockstep_config(expected);
    require(canonical == aoe::canonical_lockstep_config(expected));
    require(aoe::lockstep_config_digest(expected) ==
            aoe::lockstep_config_digest(expected));

    aoe::LockstepFrame encoded;
    encoded.kind = aoe::LockstepFrameKind::hello;
    encoded.player = aoe::Player::blue;
    encoded.scenario_digest = expected.scenario_digest;
    encoded.config = expected;
    encoded.config_digest = aoe::lockstep_config_digest(expected);
    const aoe::LockstepFrame decoded =
        aoe::decode_lockstep_frame(aoe::encode_lockstep_frame(encoded));
    require(decoded.config.has_value());
    require(aoe::canonical_lockstep_config(*decoded.config) == canonical);
    require(aoe::lockstep_config_digest(*decoded.config) ==
            aoe::lockstep_config_digest(expected));
    require(decoded.config_digest == aoe::lockstep_config_digest(expected));

    aoe::Simulation simulation = aoe::Simulation::create_demo();
    aoe::LockstepSession tampered(expected);
    aoe::LockstepFrame bad_digest = encoded;
    bad_digest.config_digest.back() =
        bad_digest.config_digest.back() == '0' ? '1' : '0';
    require(!tampered.receive(bad_digest, simulation));
    require(tampered.status() == aoe::LockstepStatus::settings_mismatch);

    const auto mismatch = [&](aoe::LockstepSessionConfig received) {
        aoe::LockstepSession session(expected);
        aoe::LockstepFrame hello = encoded;
        hello.config = std::move(received);
        require(!session.receive(hello, simulation));
        return session.status();
    };

    auto changed = expected;
    changed.build_id = "other-build";
    require(mismatch(changed) == aoe::LockstepStatus::build_mismatch);
    changed = expected;
    changed.command_schema_version++;
    require(mismatch(changed) == aoe::LockstepStatus::schema_mismatch);
    changed = expected;
    changed.scenario_digest = "other-inner-scenario";
    require(mismatch(changed) == aoe::LockstepStatus::scenario_mismatch);
    changed = expected;
    changed.content_rules_digest = "other-rules";
    require(mismatch(changed) == aoe::LockstepStatus::content_mismatch);
    changed = expected;
    changed.input_delay_ticks++;
    require(mismatch(changed) == aoe::LockstepStatus::settings_mismatch);
    changed = expected;
    changed.red.team++;
    require(mismatch(changed) == aoe::LockstepStatus::roster_mismatch);

    aoe::LockstepSession strict(expected);
    aoe::LockstepFrame no_config = encoded;
    no_config.config.reset();
    no_config.config_digest.clear();
    require(!strict.receive(no_config, simulation));
    require(strict.status() == aoe::LockstepStatus::settings_mismatch);

    aoe::LockstepSession exact(expected);
    aoe::LockstepFrame blue = encoded;
    aoe::LockstepFrame red = encoded;
    red.player = aoe::Player::red;
    require(exact.receive(blue, simulation));
    require(exact.receive(red, simulation));
    blue.kind = aoe::LockstepFrameKind::ready;
    blue.config.reset();
    blue.config_digest.clear();
    red.kind = aoe::LockstepFrameKind::ready;
    red.config.reset();
    red.config_digest.clear();
    require(exact.receive(blue, simulation));
    require(exact.receive(red, simulation));
    require(exact.status() == aoe::LockstepStatus::ready);
    blue.kind = aoe::LockstepFrameKind::start;
    require(exact.receive(blue, simulation));
    require(exact.status() == aoe::LockstepStatus::running);

    auto delayed_config = expected;
    delayed_config.input_delay_ticks = 2;
    aoe::Simulation delayed_simulation = aoe::Simulation::create_demo();
    aoe::LockstepSession delayed(delayed_config, 50, 50);
    auto delayed_control = [&](aoe::Player player,
                               aoe::LockstepFrameKind kind) {
        aoe::LockstepFrame frame;
        frame.kind = kind;
        frame.player = player;
        frame.scenario_digest = delayed_config.scenario_digest;
        if (kind == aoe::LockstepFrameKind::hello) {
            frame.config = delayed_config;
            frame.config_digest =
                aoe::lockstep_config_digest(delayed_config);
        }
        return frame;
    };
    for (const auto player : {aoe::Player::blue, aoe::Player::red}) {
        require(delayed.receive(
            delayed_control(player, aoe::LockstepFrameKind::hello),
            delayed_simulation
        ));
        require(delayed.receive(
            delayed_control(player, aoe::LockstepFrameKind::ready),
            delayed_simulation
        ));
    }
    require(delayed.receive(
        delayed_control(
            aoe::Player::blue, aoe::LockstepFrameKind::start
        ),
        delayed_simulation
    ));
    const std::string delayed_hash =
        aoe::deterministic_state_hash(delayed_simulation);
    for (std::uint64_t tick = 0; tick <= 2; ++tick) {
        for (const auto player : {aoe::Player::red, aoe::Player::blue}) {
            aoe::LockstepFrame turn = delayed_control(
                player, aoe::LockstepFrameKind::turn
            );
            turn.tick = tick;
            turn.sequence = tick;
            if (tick == 0) turn.state_hash = delayed_hash;
            if (tick == 2 && player == aoe::Player::blue) {
                turn.commands.push_back(
                    aoe::MoveUnitCommand{1, {3, 7}}
                );
            }
            require(delayed.receive(turn, delayed_simulation));
        }
    }
    require(delayed.advance(delayed_simulation));
    require(delayed.replay().commands().empty());
    require(delayed.advance(delayed_simulation));
    require(delayed.replay().commands().empty());
    require(delayed.advance(delayed_simulation));
    require(delayed.replay().commands().size() == 1);
    require(delayed.replay().commands()[0].tick == 2);

    bool rejected_invalid{};
    changed = expected;
    changed.blue.team = 9;
    try {
        encoded.config = changed;
        (void)aoe::encode_lockstep_frame(encoded);
    } catch (const std::invalid_argument&) {
        rejected_invalid = true;
    }
    require(rejected_invalid);
}

void multiplayer_checkpoint_requires_matching_barrier_and_digests() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    aoe::LockstepSessionConfig config;
    config.scenario_digest = "checkpoint-scenario";
    const std::string hash = aoe::deterministic_state_hash(simulation);

    aoe::LockstepSaveBarrier barrier;
    require(barrier.begin(0, 0));
    require(barrier.should_pause(0));
    require(barrier.submit(
        aoe::Player::red, {0, hash, 7}
    ));
    require(barrier.submit(
        aoe::Player::blue, {0, hash, 6}
    ));
    require(barrier.status() == aoe::SaveBarrierStatus::matched);
    require(barrier.should_pause(0));

    const auto directory = std::filesystem::temp_directory_path();
    const auto save = directory / "aoe-multiplayer-checkpoint.save";
    const auto envelope =
        directory / "aoe-multiplayer-checkpoint.envelope";
    aoe::write_multiplayer_checkpoint_atomic(
        simulation, config, barrier, save, envelope,
        true, aoe::GameSpeed::fast
    );
    const auto resumed =
        aoe::load_multiplayer_checkpoint(save, envelope, config);
    require(resumed.envelope.barrier_tick == 0);
    require(resumed.envelope.blue_last_bundle_sequence == 6);
    require(resumed.envelope.red_last_bundle_sequence == 7);
    require(resumed.envelope.paused);
    require(resumed.envelope.game_speed == aoe::GameSpeed::fast);
    require(resumed.envelope.config_digest ==
            aoe::lockstep_config_digest(config));
    require(aoe::deterministic_state_hash(resumed.simulation) == hash);

    aoe::LockstepSaveBarrier mismatch;
    require(mismatch.begin(0, 0));
    require(mismatch.submit(aoe::Player::blue, {0, hash, 0}));
    require(mismatch.submit(
        aoe::Player::red, {0, "different", 0}
    ));
    require(mismatch.status() ==
            aoe::SaveBarrierStatus::hash_mismatch);
    bool rejected{};
    try {
        aoe::write_multiplayer_checkpoint_atomic(
            simulation, config, mismatch, save, envelope
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected);

    auto other_config = config;
    other_config.deterministic_seed++;
    rejected = false;
    try {
        (void)aoe::load_multiplayer_checkpoint(
            save, envelope, other_config
        );
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected);

    {
        std::ofstream tamper(save, std::ios::app);
        tamper << "tamper\n";
    }
    rejected = false;
    try {
        (void)aoe::load_multiplayer_checkpoint(
            save, envelope, config
        );
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected);
    std::filesystem::remove(save);
    std::filesystem::remove(envelope);
}

void localhost_tcp_frames_survive_fragmentation_and_disconnect() {
    aoe::TcpFrameListener listener;
    aoe::TcpFrameStream client = aoe::connect_localhost(listener.port());
    aoe::TcpFrameStream server = listener.accept();
    aoe::LockstepFrame hello{
        aoe::LockstepFrameKind::hello,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "loopback",
    };
    aoe::LockstepFrame ready = hello;
    ready.kind = aoe::LockstepFrameKind::ready;
    client.send_frame_fragmented(hello, 1);
    client.send_frame(ready);
    const auto received_hello = server.receive_frame();
    const auto received_ready = server.receive_frame();
    require(received_hello &&
            received_hello->kind == aoe::LockstepFrameKind::hello);
    require(received_ready &&
            received_ready->kind == aoe::LockstepFrameKind::ready);
    server.send_frame_fragmented(ready, 3);
    require(client.receive_frame()->kind ==
            aoe::LockstepFrameKind::ready);
    server.close();
    require(!client.receive_frame());

    aoe::Simulation driver_probe = aoe::Simulation::create_demo();
    aoe::TcpFrameListener mismatch_listener;
    aoe::LocalhostLockstepDriver mismatch_join(
        aoe::connect_localhost(mismatch_listener.port()),
        "digest-b",
        aoe::Player::red,
        false
    );
    aoe::LocalhostLockstepDriver mismatch_host(
        mismatch_listener.accept(),
        "digest-a",
        aoe::Player::blue,
        true
    );
    require(mismatch_host.send_hello(driver_probe));
    require(mismatch_join.send_hello(driver_probe));
    require(!mismatch_host.pump_one(driver_probe));
    require(!mismatch_join.pump_one(driver_probe));
    require(mismatch_host.status() ==
            aoe::LockstepStatus::scenario_mismatch);
    require(mismatch_join.status() ==
            aoe::LockstepStatus::scenario_mismatch);

    aoe::TcpFrameListener disconnect_listener;
    aoe::LocalhostLockstepDriver disconnect_join(
        aoe::connect_localhost(disconnect_listener.port()),
        "same",
        aoe::Player::red,
        false
    );
    aoe::LocalhostLockstepDriver disconnect_host(
        disconnect_listener.accept(),
        "same",
        aoe::Player::blue,
        true
    );
    disconnect_join.close();
    require(!disconnect_host.pump_one(driver_probe));
    require(disconnect_host.status() ==
            aoe::LockstepStatus::disconnected);

    aoe::TcpFrameListener managed_loss_listener;
    aoe::LocalhostLockstepDriver managed_loss_join(
        aoe::connect_localhost(managed_loss_listener.port()),
        "managed-loss", aoe::Player::red, false
    );
    aoe::LocalhostLockstepDriver managed_loss_host(
        managed_loss_listener.accept(),
        "managed-loss", aoe::Player::blue, true
    );
    managed_loss_host.set_managed_drop_flow(true);
    managed_loss_join.close();
    (void)managed_loss_host.pump_nonblocking(driver_probe);
    require(managed_loss_host.status() ==
            aoe::LockstepStatus::handshaking);
    require(managed_loss_host.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::suspended);
    require(managed_loss_host.reliability_reason() ==
            aoe::MultiplayerReliabilityReason::transport_lost);
    require(managed_loss_host.drop_peer());
    require(managed_loss_host.status() ==
            aoe::LockstepStatus::disconnected);

    aoe::TcpFrameListener explicit_disconnect_listener;
    aoe::LocalhostLockstepDriver explicit_disconnect_join(
        aoe::connect_localhost(explicit_disconnect_listener.port()),
        "explicit-disconnect", aoe::Player::red, false
    );
    aoe::LocalhostLockstepDriver explicit_disconnect_host(
        explicit_disconnect_listener.accept(),
        "explicit-disconnect", aoe::Player::blue, true
    );
    require(explicit_disconnect_join.send_disconnect());
    require(explicit_disconnect_host.pump_one(driver_probe));
    require(explicit_disconnect_host.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::disconnected);
    require(explicit_disconnect_host.reliability_reason() ==
            aoe::MultiplayerReliabilityReason::peer_disconnected);

    aoe::LockstepSessionConfig chat_config;
    chat_config.scenario_digest = "chat-loopback";
    chat_config.blue.team = 1;
    chat_config.red.team = 2;
    aoe::TcpFrameListener chat_listener;
    aoe::LocalhostLockstepDriver chat_join(
        aoe::connect_localhost(chat_listener.port()),
        chat_config, aoe::Player::red, false
    );
    aoe::LocalhostLockstepDriver chat_host(
        chat_listener.accept(),
        chat_config, aoe::Player::blue, true
    );
    require(chat_host.send_hello(driver_probe));
    require(chat_join.send_hello(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.send_ready(driver_probe));
    require(chat_join.send_ready(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.send_start(driver_probe));
    require(chat_join.pump_one(driver_probe));
    const std::string hash_before_chat =
        aoe::deterministic_state_hash(driver_probe);
    require(chat_host.send_chat("Host hello", aoe::ChatAudience::all));
    require(chat_join.pump_one(driver_probe));
    require(chat_join.chat_log().size() == 1);
    require(chat_join.chat_log()[0].sequence == 1);
    require(chat_join.chat_log()[0].sender == aoe::Player::blue);
    require(chat_join.send_chat(
        "Olá from join", aoe::ChatAudience::all
    ));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.chat_log().size() == 2);
    require(chat_join.chat_log().size() == 2);
    require(chat_host.chat_log()[1].sequence == 2);
    require(chat_join.chat_log()[1].text == "Olá from join");
    require(chat_host.send_chat(
        "Blue allies only", aoe::ChatAudience::allies
    ));
    require(chat_host.chat_log().size() == 3);
    require(chat_join.chat_log().size() == 2);
    require(!chat_host.send_chat(
        std::string(4097, 'x'), aoe::ChatAudience::all
    ));
    require(!chat_host.send_chat(
        std::string("\xc0\xaf", 2), aoe::ChatAudience::all
    ));
    require(aoe::deterministic_state_hash(driver_probe) ==
            hash_before_chat);
    require(chat_host.session().replay().commands().empty());
    require(aoe::latency_band_for_rtt(299) ==
            aoe::LatencyBand::green);
    require(aoe::latency_band_for_rtt(300) ==
            aoe::LatencyBand::yellow);
    require(aoe::latency_band_for_rtt(1001) ==
            aoe::LatencyBand::red);
    const auto heartbeat_time = std::chrono::steady_clock::now();
    chat_host.maintain_heartbeat(heartbeat_time);
    require(chat_join.pump_one(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_host.network_metrics().round_trip_ms.has_value());
    require(chat_host.network_metrics(
        heartbeat_time + std::chrono::seconds(6)
    ).waiting);
    require(!chat_join.propose_pause(true, 0));
    require(chat_host.propose_pause(true, 0));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.paused());
    require(chat_join.paused());
    require(chat_host.propose_speed(aoe::GameSpeed::fast, 0));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.game_speed() == aoe::GameSpeed::fast);
    require(chat_join.game_speed() == aoe::GameSpeed::fast);
    require(chat_host.propose_pause(false, 0));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(!chat_host.paused());
    require(!chat_join.paused());
    require(chat_host.request_save_barrier(0));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.save_barrier().should_pause(0));
    require(chat_join.save_barrier().should_pause(0));
    require(chat_host.submit_save_hash(driver_probe));
    require(chat_join.submit_save_hash(driver_probe));
    require(chat_host.pump_one(driver_probe));
    require(chat_join.pump_one(driver_probe));
    require(chat_host.save_barrier().status() ==
            aoe::SaveBarrierStatus::matched);
    require(chat_join.save_barrier().status() ==
            aoe::SaveBarrierStatus::matched);
    chat_host.update_reliability(
        heartbeat_time + std::chrono::seconds(6)
    );
    require(chat_host.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::waiting);
    require(chat_host.reliability_reason() ==
            aoe::MultiplayerReliabilityReason::peer_silent);
    chat_host.update_reliability(
        heartbeat_time + std::chrono::seconds(31)
    );
    require(chat_host.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::suspended);
    require(!chat_host.advance(driver_probe));
    require(chat_host.drop_peer());
    require(chat_host.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::dropped);
    require(chat_join.pump_one(driver_probe));
    require(chat_join.reliability_status() ==
            aoe::MultiplayerReliabilityStatus::dropped);
    require(chat_join.reliability_reason() ==
            aoe::MultiplayerReliabilityReason::host_dropped_peer);

#if !defined(_WIN32)
    aoe::TcpFrameListener process_listener;
    const pid_t child = ::fork();
    require(child >= 0);
    if (child == 0) {
        try {
            aoe::TcpFrameStream process_client =
                aoe::connect_localhost(process_listener.port());
            process_client.send_frame_fragmented(hello, 2);
            process_client.close();
            _exit(0);
        } catch (...) {
            _exit(1);
        }
    }
    aoe::TcpFrameStream process_server = process_listener.accept();
    const auto process_frame = process_server.receive_frame();
    require(process_frame &&
            process_frame->scenario_digest == "loopback");
    int child_status{};
    require(::waitpid(child, &child_status, 0) == child);
    require(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);

    aoe::TcpFrameListener driver_listener;
    const pid_t driver_child = ::fork();
    require(driver_child >= 0);
    if (driver_child == 0) {
        try {
            aoe::Simulation child_simulation =
                aoe::Simulation::create_demo();
            aoe::LocalhostLockstepDriver joiner(
                aoe::connect_localhost(driver_listener.port()),
                "driver-loopback",
                aoe::Player::red,
                false,
                10,
                1
            );
            if (!joiner.send_hello(child_simulation) ||
                !joiner.pump_one(child_simulation) ||
                !joiner.send_ready(child_simulation) ||
                !joiner.pump_one(child_simulation) ||
                !joiner.pump_one(child_simulation) ||
                !joiner.submit_turn(child_simulation) ||
                !joiner.pump_one(child_simulation) ||
                !joiner.advance(child_simulation) ||
                !joiner.submit_turn(child_simulation) ||
                !joiner.pump_one(child_simulation) ||
                !joiner.advance(child_simulation) ||
                joiner.status() != aoe::LockstepStatus::running) {
                _exit(2);
            }
            _exit(0);
        } catch (...) {
            _exit(3);
        }
    }
    aoe::Simulation host_simulation = aoe::Simulation::create_demo();
    aoe::LocalhostLockstepDriver host(
        driver_listener.accept(),
        "driver-loopback",
        aoe::Player::blue,
        true,
        10,
        1
    );
    require(host.send_hello(host_simulation));
    require(host.pump_one(host_simulation));
    require(host.send_ready(host_simulation));
    require(host.pump_one(host_simulation));
    require(host.send_start(host_simulation));
    require(host.submit_turn(
        host_simulation,
        {aoe::MoveUnitCommand{1, {3, 7}}}
    ));
    require(host.pump_one(host_simulation));
    require(host.advance(host_simulation));
    require(host.submit_turn(host_simulation));
    require(host.pump_one(host_simulation));
    require(host.advance(host_simulation));
    int driver_status{};
    require(::waitpid(driver_child, &driver_status, 0) == driver_child);
    require(WIFEXITED(driver_status) && WEXITSTATUS(driver_status) == 0);
    require(host.status() == aoe::LockstepStatus::running);

    aoe::LocalhostMultiplayerRuntime host_runtime =
        aoe::LocalhostMultiplayerRuntime::host(
            0, "runtime-loopback", 5000, 1
        );
    const pid_t runtime_child = ::fork();
    require(runtime_child >= 0);
    if (runtime_child == 0) {
        try {
            ::usleep(20000);
            aoe::Simulation runtime_simulation =
                aoe::Simulation::create_demo();
            auto join_runtime = aoe::LocalhostMultiplayerRuntime::join(
                host_runtime.port(), "runtime-loopback", 5000, 1
            );
            for (int step = 0;
                 step < 5000 && join_runtime.current_tick() < 3;
                 ++step) {
                join_runtime.pump(runtime_simulation);
                ::usleep(1000);
            }
            _exit(
                join_runtime.current_tick() >= 3 &&
                join_runtime.status() == aoe::LockstepStatus::running
                ? 0 : 4
            );
        } catch (...) {
            _exit(5);
        }
    }
    aoe::Simulation runtime_simulation =
        aoe::Simulation::create_demo();
    host_runtime.queue_command(
        aoe::MoveUnitCommand{1, {3, 7}}
    );
    for (int step = 0;
         step < 5000 && host_runtime.current_tick() < 3;
         ++step) {
        host_runtime.pump(runtime_simulation);
        ::usleep(1000);
    }
    int runtime_status{};
    require(::waitpid(runtime_child, &runtime_status, 0) == runtime_child);
    require(WIFEXITED(runtime_status) &&
            WEXITSTATUS(runtime_status) == 0);
    require(host_runtime.current_tick() >= 3);
    require(host_runtime.status() == aoe::LockstepStatus::running);
    const auto moved = std::ranges::find(
        runtime_simulation.units(), 1, &aoe::Unit::id
    );
    require(moved != runtime_simulation.units().end());
    require(moved->position == aoe::TilePosition(3, 7));
#endif
}

void missing_standard_technologies_follow_live_dat() {
    require(aoe::technology_count == 158);
    const auto& thumb = aoe::rules_for(aoe::Technology::thumb_ring);
    require(thumb.researched_at == aoe::BuildingKind::archery_range);
    require(thumb.minimum_age == aoe::Age::castle);
    require(thumb.wood_cost == 250 && thumb.food_cost == 300);
    require(thumb.research_ticks == 15);
    const auto& parthian =
        aoe::rules_for(aoe::Technology::parthian_tactics);
    require(parthian.minimum_age == aoe::Age::imperial);
    require(parthian.food_cost == 200 && parthian.gold_cost == 250);
    require(aoe::rules_for(aoe::Technology::tracking).food_cost == 75);
    require(aoe::rules_for(aoe::Technology::squires).food_cost == 200);
    require(
        aoe::rules_for(aoe::Technology::herbal_medicine).gold_cost == 350
    );
    require(
        aoe::rules_for(aoe::Technology::stone_cutting).wood_cost == 200
    );
    const auto& spies =
        aoe::rules_for(aoe::Technology::spy_technology);
    require(spies.researched_at == aoe::BuildingKind::castle);
    require(spies.minimum_age == aoe::Age::imperial);
    require(spies.gold_cost == 200 && spies.research_ticks == 1);

    aoe::Simulation simulation(aoe::GameMap(12, 8));
    const auto infantry_id = simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {2, 2}
    );
    const auto archer_id = simulation.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {3, 2}
    );
    const auto cavalry_archer_id = simulation.add_unit(
        aoe::UnitKind::cavalry_archer, aoe::Player::blue, {4, 2}
    );
    const auto& infantry = *std::ranges::find(
        simulation.units(), infantry_id, &aoe::Unit::id
    );
    const auto& archer = *std::ranges::find(
        simulation.units(), archer_id, &aoe::Unit::id
    );
    const auto& cavalry_archer = *std::ranges::find(
        simulation.units(), cavalry_archer_id, &aoe::Unit::id
    );
    const int base_vision =
        simulation.effective_unit_vision_range(infantry);
    const int base_speed =
        simulation.unique_unit_movement_numerator(infantry);
    const int base_interval =
        simulation.effective_attack_interval(archer);
    const int base_melee = simulation.melee_armor(cavalry_archer);
    const int base_pierce = simulation.pierce_armor(cavalry_archer);
    simulation.replace_technologies(aoe::Player::blue, {
        aoe::Technology::tracking,
        aoe::Technology::squires,
        aoe::Technology::thumb_ring,
        aoe::Technology::parthian_tactics,
    });
    require(
        simulation.effective_unit_vision_range(infantry) ==
        base_vision + 2
    );
    require(
        simulation.unique_unit_movement_numerator(infantry) ==
        base_speed * 110 / 100
    );
    require(
        simulation.effective_attack_interval(archer) ==
        std::max(1, base_interval * 85 / 100)
    );
    require(simulation.melee_armor(cavalry_archer) == base_melee + 1);
    require(simulation.pierce_armor(cavalry_archer) == base_pierce + 2);

    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-standard-techs.scenario";
    aoe::Scenario scenario(4, 4);
    scenario.blue_technologies = {
        aoe::Technology::thumb_ring,
        aoe::Technology::parthian_tactics,
        aoe::Technology::squires,
        aoe::Technology::tracking,
        aoe::Technology::herbal_medicine,
        aoe::Technology::stone_cutting,
        aoe::Technology::spy_technology,
    };
    aoe::save_scenario(scenario, scenario_path);
    const auto loaded = aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(loaded.blue_technologies == scenario.blue_technologies);

    aoe::Simulation saved_technologies(aoe::GameMap(6, 6));
    saved_technologies.replace_technologies(
        aoe::Player::blue, scenario.blue_technologies
    );
    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-standard-techs.save";
    aoe::save_game(saved_technologies, save_path);
    const auto loaded_technologies = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    for (aoe::Technology technology : scenario.blue_technologies) {
        require(loaded_technologies.has_technology(
            aoe::Player::blue, technology
        ));
    }

    aoe::Simulation spy_match(aoe::GameMap(24, 10));
    const auto castle = spy_match.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {2, 2}
    );
    for (int index = 0; index < 3; ++index) {
        spy_match.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {18 + index, 7}
        );
    }
    spy_match.replace_state(
        spy_match.units(),
        spy_match.buildings(),
        {0, 0, 1'000, 0},
        spy_match.economy(aoe::Player::red),
        0
    );
    spy_match.replace_ages(aoe::Age::imperial, aoe::Age::dark);
    require(!spy_match.is_visible(aoe::Player::blue, {18, 7}));
    require(spy_match.research_technology_at(
        castle, aoe::Technology::spy_technology
    ));
    require(spy_match.economy(aoe::Player::blue).gold == 400);
    spy_match.update();
    require(spy_match.has_technology(
        aoe::Player::blue, aoe::Technology::spy_technology
    ));
    require(spy_match.is_visible(aoe::Player::blue, {18, 7}));
}

void allied_victory_aggregates_objectives_deterministically() {
    aoe::Simulation relic_team(aoe::GameMap(24, 10));
    relic_team.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {1, 1}
    );
    relic_team.add_building(
        aoe::BuildingKind::monastery, aoe::Player::red, {16, 1}
    );
    auto relic_buildings = relic_team.buildings();
    relic_buildings[0].relic_count = 3;
    relic_buildings[1].relic_count = 2;
    relic_team.replace_state(
        relic_team.units(), std::move(relic_buildings),
        relic_team.economy(aoe::Player::blue),
        relic_team.economy(aoe::Player::red), 0
    );
    require(relic_team.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    aoe::MatchRules relic_rules;
    relic_rules.conquest_enabled = false;
    relic_rules.wonder_enabled = false;
    relic_rules.relic_enabled = true;
    relic_rules.relics_required = 5;
    relic_rules.relic_countdown_ticks = 3;
    relic_team.set_match_rules(relic_rules);
    relic_team.update();
    require(relic_team.countdown_kind(aoe::Player::blue) ==
        aoe::VictoryCountdownKind::relic);
    require(relic_team.countdown_kind(aoe::Player::red) ==
        aoe::VictoryCountdownKind::relic);
    require(relic_team.victory_countdown(aoe::Player::blue) == 2);
    require(
        relic_team.victory_countdown(aoe::Player::blue) ==
        relic_team.victory_countdown(aoe::Player::red)
    );
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-allied-relic-countdown.save";
    aoe::save_game(relic_team, save_path);
    auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    restored.update();
    restored.update();
    require(restored.outcome() == aoe::MatchOutcome::allied_victory);

    aoe::Simulation score_team(aoe::GameMap(18, 8));
    score_team.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    score_team.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 1}
    );
    require(score_team.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    aoe::MatchRules score_rules;
    score_rules.conquest_enabled = false;
    score_rules.wonder_enabled = false;
    score_rules.relic_enabled = false;
    const int blue_score = score_team.score(aoe::Player::blue);
    const int red_score = score_team.score(aoe::Player::red);
    score_rules.score_limit = std::max(blue_score, red_score) + 1;
    require(blue_score < score_rules.score_limit);
    require(red_score < score_rules.score_limit);
    require(blue_score + red_score >= score_rules.score_limit);
    score_team.set_match_rules(score_rules);
    score_team.update();
    require(score_team.outcome() == aoe::MatchOutcome::allied_victory);

    aoe::Simulation timed_team(aoe::GameMap(18, 8));
    timed_team.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    timed_team.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {16, 6}
    );
    require(timed_team.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    aoe::MatchRules time_rules;
    time_rules.conquest_enabled = false;
    time_rules.wonder_enabled = false;
    time_rules.relic_enabled = false;
    time_rules.time_limit_ticks = 1;
    timed_team.set_match_rules(time_rules);
    timed_team.update();
    require(timed_team.outcome() == aoe::MatchOutcome::allied_victory);

    const auto wonder_timer = [](bool atheism) {
        aoe::Simulation simulation(aoe::GameMap(20, 10));
        simulation.add_building(
            aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {15, 1}
        );
        if (atheism) {
            simulation.replace_technologies(
                aoe::Player::red, {aoe::Technology::atheism}
            );
        }
        aoe::MatchRules rules;
        rules.conquest_enabled = false;
        rules.wonder_enabled = true;
        rules.relic_enabled = false;
        rules.wonder_countdown_ticks = 5;
        simulation.set_match_rules(rules);
        simulation.update();
        simulation.update();
        return simulation.victory_countdown(aoe::Player::blue);
    };
    require(wonder_timer(false) == wonder_timer(true));

    aoe::Simulation diplomacy(aoe::GameMap(20, 10));
    diplomacy.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {1, 1}
    );
    diplomacy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {15, 1}
    );
    auto diplomacy_rules = relic_rules;
    diplomacy_rules.wonder_enabled = true;
    diplomacy_rules.relic_enabled = false;
    diplomacy_rules.wonder_countdown_ticks = 3;
    diplomacy.set_match_rules(diplomacy_rules);
    diplomacy.update();
    require(diplomacy.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    for (int tick = 0; tick < 3; ++tick) diplomacy.update();
    require(diplomacy.outcome() == aoe::MatchOutcome::allied_victory);
}

void garrison_ejection_and_defensive_contribution_are_bounded() {
    const auto projectile_count = [](aoe::UnitKind occupant) {
        aoe::Simulation simulation(aoe::GameMap(24, 10));
        const auto castle = simulation.add_building(
            aoe::BuildingKind::castle, aoe::Player::blue, {2, 2}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {20, 7}
        );
        const auto defender = simulation.add_unit(
            occupant, aoe::Player::blue, {8, 8}
        );
        simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {11, 3}
        );
        require(simulation.restore_garrison(defender, castle));
        simulation.update();
        return simulation.projectiles().size();
    };
    const auto melee_projectiles =
        projectile_count(aoe::UnitKind::militia);
    require(
        projectile_count(aoe::UnitKind::villager) ==
        melee_projectiles + 1
    );
    require(
        projectile_count(aoe::UnitKind::archer) ==
        melee_projectiles + 1
    );

    aoe::Simulation deletion(aoe::GameMap(20, 10));
    const auto castle = deletion.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {5, 2}
    );
    deletion.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {16, 7}
    );
    const auto first = deletion.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    const auto second = deletion.add_unit(
        aoe::UnitKind::archer, aoe::Player::blue, {1, 2}
    );
    require(deletion.restore_garrison(first, castle));
    require(deletion.restore_garrison(second, castle));
    require(deletion.delete_building(castle));
    require(std::ranges::none_of(
        deletion.buildings(), [castle](const aoe::Building& building) {
            return building.id == castle;
        }
    ));
    require(deletion.units()[0].garrisoned_in == 0);
    require(deletion.units()[1].garrisoned_in == 0);
    require(deletion.units()[0].position != deletion.units()[1].position);

    aoe::Simulation diplomacy(aoe::GameMap(24, 10));
    diplomacy.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {2, 2}
    );
    diplomacy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {20, 7}
    );
    diplomacy.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {11, 3}
    );
    aoe::MatchRules peaceful;
    peaceful.conquest_enabled = false;
    peaceful.wonder_enabled = false;
    peaceful.relic_enabled = false;
    diplomacy.set_match_rules(peaceful);
    const int hit_points = diplomacy.units()[0].hit_points;
    diplomacy.update();
    require(!diplomacy.projectiles().empty());
    require(diplomacy.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    for (int tick = 0; tick < 20; ++tick) diplomacy.update();
    require(diplomacy.units()[0].hit_points == hit_points);

    aoe::GameMap water_map(12, 8);
    water_map.set_terrain({4, 3}, aoe::Terrain::water);
    aoe::Simulation transport_loss(std::move(water_map));
    const auto passenger = transport_loss.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {3, 3}
    );
    const auto transport = transport_loss.add_unit(
        aoe::UnitKind::transport_ship, aoe::Player::blue, {4, 3}
    );
    transport_loss.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 5}
    );
    require(transport_loss.command_embark(passenger, transport));
    auto doomed = transport_loss.units();
    doomed[1].hit_points = 0;
    transport_loss.replace_state(
        std::move(doomed), transport_loss.buildings(),
        transport_loss.economy(aoe::Player::blue),
        transport_loss.economy(aoe::Player::red), 0
    );
    transport_loss.update();
    require(std::ranges::none_of(
        transport_loss.units(),
        [passenger, transport](const aoe::Unit& unit) {
            return unit.id == passenger || unit.id == transport;
        }
    ));
}

void trade_routes_invalidate_and_persist_deterministically() {
    const auto completed_trip = [](bool caravan) {
        aoe::Simulation simulation(aoe::GameMap(30, 10));
        simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::blue, {1, 1}
        );
        const auto target = simulation.add_building(
            aoe::BuildingKind::market, aoe::Player::red, {20, 1}
        );
        const auto cart = simulation.add_unit(
            aoe::UnitKind::trade_cart, aoe::Player::blue, {6, 5}
        );
        simulation.replace_state(
            simulation.units(), simulation.buildings(),
            {}, {}, 0
        );
        require(simulation.set_diplomacy(
            aoe::Player::blue, aoe::Player::red,
            aoe::Diplomacy::ally
        ));
        if (caravan) {
            simulation.replace_technologies(
                aoe::Player::blue, {aoe::Technology::caravan}
            );
        }
        aoe::MatchRules rules;
        rules.conquest_enabled = false;
        rules.wonder_enabled = false;
        rules.relic_enabled = false;
        simulation.set_match_rules(rules);
        require(simulation.command_trade_route(cart, target));
        int ticks{};
        while (simulation.economy(aoe::Player::blue).gold == 0 &&
               ticks < 500) {
            simulation.update();
            ++ticks;
        }
        return std::pair{
            ticks, simulation.economy(aoe::Player::blue).gold
        };
    };
    const auto ordinary = completed_trip(false);
    const auto caravan = completed_trip(true);
    require(ordinary.second == 38);
    require(caravan.second == 38);
    require(caravan.first < ordinary.first);

    aoe::Simulation persisted(aoe::GameMap(30, 10));
    persisted.add_building(
        aoe::BuildingKind::market, aoe::Player::blue, {1, 1}
    );
    const auto target = persisted.add_building(
        aoe::BuildingKind::market, aoe::Player::red, {20, 1}
    );
    const auto cart = persisted.add_unit(
        aoe::UnitKind::trade_cart, aoe::Player::blue, {6, 5}
    );
    persisted.replace_state(
        persisted.units(), persisted.buildings(), {}, {}, 0
    );
    require(!persisted.command_trade_route(cart, target));
    require(persisted.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    aoe::MatchRules rules;
    rules.conquest_enabled = false;
    rules.wonder_enabled = false;
    rules.relic_enabled = false;
    persisted.set_match_rules(rules);
    aoe::Replay replay;
    replay.record(0, aoe::TradeRouteCommand{cart, target});
    replay.apply_current_tick(persisted);
    for (int tick = 0; tick < 12; ++tick) persisted.update();
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-trade-route-midtrip.save";
    aoe::save_game(persisted, save_path);
    auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    for (int tick = 0; tick < 500 &&
         persisted.economy(aoe::Player::blue).gold == 0; ++tick) {
        persisted.update();
        restored.update();
    }
    require(persisted.economy(aoe::Player::blue).gold == 38);
    require(restored.economy(aoe::Player::blue).gold == 38);
    require(
        persisted.units()[0].trade_returning ==
        restored.units()[0].trade_returning
    );

    aoe::Simulation invalidated(aoe::GameMap(30, 10));
    invalidated.add_building(
        aoe::BuildingKind::market, aoe::Player::blue, {1, 1}
    );
    const auto invalid_target = invalidated.add_building(
        aoe::BuildingKind::market, aoe::Player::red, {20, 1}
    );
    const auto invalid_cart = invalidated.add_unit(
        aoe::UnitKind::trade_cart, aoe::Player::blue, {6, 5}
    );
    invalidated.set_match_rules(rules);
    require(invalidated.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::ally
    ));
    require(invalidated.command_trade_route(
        invalid_cart, invalid_target
    ));
    const int invalidation_gold =
        invalidated.economy(aoe::Player::blue).gold;
    require(invalidated.units()[0].moving);
    require(invalidated.set_diplomacy(
        aoe::Player::blue, aoe::Player::red, aoe::Diplomacy::enemy
    ));
    invalidated.update();
    require(invalidated.units()[0].trade_home_market_id == 0);
    require(invalidated.units()[0].trade_target_market_id == 0);
    require(!invalidated.units()[0].moving);
    require(
        invalidated.economy(aoe::Player::blue).gold ==
        invalidation_gold
    );

    require(invalidated.market_buy_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 130);
    require(invalidated.market_sell_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 70);
    invalidated.replace_technologies(
        aoe::Player::blue, {aoe::Technology::guilds}
    );
    require(invalidated.market_buy_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 115);
    require(invalidated.market_sell_price(
        aoe::Player::blue, aoe::MarketResource::food
    ) == 85);
}

void religious_conversion_resistance_and_group_policy_is_deterministic() {
    const auto conversion_ticks = [](
        bool shifted_target,
        bool faith
    ) {
        aoe::Simulation simulation(aoe::GameMap(18, 8));
        const auto monk = simulation.add_unit(
            aoe::UnitKind::monk, aoe::Player::blue, {3, 3}
        );
        if (shifted_target) {
            simulation.add_unit(
                aoe::UnitKind::relic, aoe::Player::neutral, {1, 6}
            );
        }
        const auto target = simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {7, 3}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {14, 4}
        );
        if (faith) {
            simulation.replace_technologies(
                aoe::Player::red, {aoe::Technology::faith}
            );
        }
        require(simulation.command_convert(monk, target));
        int ticks{};
        while (ticks < 20) {
            simulation.update();
            ++ticks;
            const auto converted = std::ranges::find(
                simulation.units(), target, &aoe::Unit::id
            );
            if (converted != simulation.units().end() &&
                converted->owner == aoe::Player::blue) {
                break;
            }
        }
        return ticks;
    };
    require(conversion_ticks(false, false) == 10);
    require(conversion_ticks(true, false) == 10);
    require(conversion_ticks(false, true) == 14);

    aoe::Simulation persisted(aoe::GameMap(18, 8));
    const auto persisted_monk = persisted.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {3, 3}
    );
    persisted.add_unit(
        aoe::UnitKind::relic, aoe::Player::neutral, {1, 6}
    );
    const auto persisted_target = persisted.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {7, 3}
    );
    persisted.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {14, 4}
    );
    aoe::Replay replay;
    replay.record(
        0, aoe::ConvertUnitCommand{persisted_monk, persisted_target}
    );
    replay.apply_current_tick(persisted);
    for (int tick = 0; tick < 7; ++tick) persisted.update();
    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-conversion-resistance.save";
    aoe::save_game(persisted, save_path);
    auto restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    for (int tick = 0; tick < 5; ++tick) {
        persisted.update();
        restored.update();
    }
    require(persisted.units()[2].owner == aoe::Player::blue);
    require(restored.units()[2].owner == aoe::Player::blue);
    require(
        persisted.units()[0].conversion_cooldown ==
        restored.units()[0].conversion_cooldown
    );

    const auto group_conversion = [](bool theocracy) {
        aoe::Simulation simulation(aoe::GameMap(18, 8));
        const auto first = simulation.add_unit(
            aoe::UnitKind::monk, aoe::Player::blue, {3, 2}
        );
        const auto second = simulation.add_unit(
            aoe::UnitKind::monk, aoe::Player::blue, {3, 4}
        );
        const auto target = simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::red, {7, 3}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {14, 4}
        );
        if (theocracy) {
            simulation.replace_technologies(
                aoe::Player::blue, {aoe::Technology::theocracy}
            );
        }
        require(simulation.command_convert(first, target));
        require(simulation.command_convert(second, target));
        for (int tick = 0; tick < 10; ++tick) simulation.update();
        require(simulation.units()[2].owner == aoe::Player::blue);
        simulation.update();
        return std::array{
            simulation.units()[0].conversion_cooldown,
            simulation.units()[1].conversion_cooldown,
        };
    };
    const auto shared_charge = group_conversion(false);
    require(shared_charge[0] > 0 && shared_charge[1] > 0);
    const auto retained_charge = group_conversion(true);
    require(
        (retained_charge[0] == 0) != (retained_charge[1] == 0)
    );

    aoe::Simulation heresy(aoe::GameMap(16, 8));
    const auto heresy_monk = heresy.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {3, 3}
    );
    const auto heresy_target = heresy.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {7, 3}
    );
    heresy.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {13, 4}
    );
    heresy.replace_technologies(
        aoe::Player::red, {aoe::Technology::heresy}
    );
    require(heresy.command_convert(heresy_monk, heresy_target));
    for (int tick = 0; tick < 10; ++tick) heresy.update();
    require(std::ranges::none_of(
        heresy.units(), [heresy_target](const aoe::Unit& unit) {
            return unit.id == heresy_target;
        }
    ));

    aoe::Simulation filters(aoe::GameMap(20, 10));
    const auto shelter = filters.add_building(
        aoe::BuildingKind::monastery, aoe::Player::blue, {1, 1}
    );
    const auto enemy_house = filters.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {9, 2}
    );
    const auto monk = filters.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {6, 3}
    );
    filters.replace_technologies(
        aoe::Player::blue, {aoe::Technology::redemption}
    );
    auto garrisoned = filters.units();
    garrisoned[0].garrisoned_in = shelter;
    filters.replace_state(
        std::move(garrisoned), filters.buildings(),
        filters.economy(aoe::Player::blue),
        filters.economy(aoe::Player::red), 0
    );
    require(!filters.command_convert(monk, enemy_house));
}

void computer_victory_and_team_policy_is_bounded_and_deterministic() {
    aoe::Simulation disabled(aoe::GameMap(24, 12));
    disabled.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {17, 2}
    );
    for (int index = 0; index < 5; ++index) {
        disabled.add_building(
            aoe::BuildingKind::house, aoe::Player::red,
            {12 + index * 2, 8}
        );
    }
    disabled.add_building(
        aoe::BuildingKind::house, aoe::Player::blue, {1, 1}
    );
    disabled.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {16, 6}
    );
    disabled.replace_state(
        disabled.units(), disabled.buildings(),
        {5000, 5000, 5000, 5000},
        {5000, 5000, 5000, 5000}, 0
    );
    disabled.replace_ages(aoe::Age::dark, aoe::Age::imperial);
    aoe::MatchRules disabled_rules;
    disabled_rules.wonder_enabled = false;
    disabled_rules.relic_enabled = false;
    disabled.set_match_rules(disabled_rules);
    for (int tick = 0; tick < 5; ++tick) disabled.update();
    aoe::ComputerPlayer disabled_ai(aoe::Player::red);
    disabled_ai.update(disabled);
    require(std::ranges::none_of(
        disabled.buildings(), [](const aoe::Building& building) {
            return building.owner == aoe::Player::red &&
                building.kind == aoe::BuildingKind::wonder;
        }
    ));

    aoe::Simulation denial(aoe::GameMap(36, 12));
    denial.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {28, 2}
    );
    denial.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {2, 2}
    );
    denial.add_unit(
        aoe::UnitKind::scout_cavalry, aoe::Player::red, {7, 3}
    );
    for (int index = 0; index < 4; ++index) {
        denial.add_unit(
            aoe::UnitKind::militia, aoe::Player::red,
            {24 + index % 2, 5 + index}
        );
    }
    denial.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {22, 6}
    );
    {
        auto passive = denial.units();
        for (aoe::Unit& unit : passive) {
            unit.stance = aoe::UnitStance::passive;
        }
        denial.replace_state(
            std::move(passive), denial.buildings(),
            denial.economy(aoe::Player::blue),
            denial.economy(aoe::Player::red), 0
        );
    }
    aoe::MatchRules denial_rules;
    denial_rules.conquest_enabled = false;
    denial_rules.wonder_enabled = true;
    denial_rules.relic_enabled = false;
    denial_rules.wonder_countdown_ticks = 100;
    denial.set_match_rules(denial_rules);
    aoe::ComputerPlayer denial_ai(aoe::Player::red);
    for (int tick = 0; tick < 5; ++tick) denial.update();
    denial_ai.update(denial);
    require(denial_ai.status().objective == aoe::ComputerObjective::wonder);
    require(
        denial_ai.status().target == aoe::TilePosition(2, 2)
    );

    aoe::Simulation allies(aoe::GameMap(18, 8));
    const auto allied_market = allies.add_building(
        aoe::BuildingKind::market, aoe::Player::blue, {4, 2}
    );
    allies.add_building(
        aoe::BuildingKind::market, aoe::Player::red, {11, 2}
    );
    const auto cart = allies.add_unit(
        aoe::UnitKind::trade_cart, aoe::Player::red, {8, 3}
    );
    require(allies.set_diplomacy(
        aoe::Player::red, aoe::Player::blue, aoe::Diplomacy::ally
    ));
    aoe::MatchRules team_rules;
    team_rules.conquest_enabled = false;
    team_rules.wonder_enabled = false;
    team_rules.relic_enabled = false;
    allies.set_match_rules(team_rules);
    for (int tick = 0; tick < 5; ++tick) allies.update();
    aoe::ComputerPlayer team_ai(aoe::Player::red);
    team_ai.update(allies);
    const auto routed = std::ranges::find(
        allies.units(), cart, &aoe::Unit::id
    );
    require(routed != allies.units().end());
    require(routed->trade_target_market_id == allied_market);
    require(team_ai.status().objective == aoe::ComputerObjective::trade);
}

void mayan_resource_duration_is_per_player_exact_and_persistent() {
    const auto make_single = [](aoe::Civilization civilization) {
        aoe::GameMap map(16, 8);
        map.set_terrain({8, 3}, aoe::Terrain::gold_mine);
        map.set_resource_amount({8, 3}, 1000);
        aoe::Simulation simulation(std::move(map));
        const auto worker = simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {7, 3}
        );
        simulation.add_building(
            aoe::BuildingKind::mining_camp,
            aoe::Player::blue, {2, 2}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {13, 5}
        );
        require(simulation.set_civilization(
            aoe::Player::blue, civilization
        ));
        require(simulation.command_unit(worker, {8, 3}));
        return simulation;
    };
    const auto totals = [](const aoe::Simulation& simulation) {
        const int credited =
            simulation.economy(aoe::Player::blue).gold - 200 +
            simulation.units().front().carried_amount;
        const int consumed =
            1000 - simulation.map().resource_amount_at({8, 3});
        return std::pair{credited, consumed};
    };

    auto generic = make_single(aoe::Civilization::generic);
    auto mayans = make_single(aoe::Civilization::mayans);
    for (int tick = 0; tick < 73; ++tick) {
        generic.update();
        mayans.update();
    }
    const auto [generic_credit, generic_consumed] = totals(generic);
    const auto [mayan_credit, mayan_consumed] = totals(mayans);
    require(generic_credit == generic_consumed);
    require(mayan_credit > mayan_consumed);
    require(
        mayan_credit * 100 ==
        mayan_consumed * 115 +
            mayans.mayan_resource_remainder(aoe::Player::blue)
    );

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-mayan-resource-duration.save";
    aoe::save_game(mayans, path);
    auto loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(
        loaded.mayan_resource_remainder(aoe::Player::blue) ==
        mayans.mayan_resource_remainder(aoe::Player::blue)
    );
    for (int tick = 0; tick < 200; ++tick) {
        mayans.update();
        loaded.update();
    }
    require(totals(loaded) == totals(mayans));
    require(
        loaded.mayan_resource_remainder(aoe::Player::blue) ==
        mayans.mayan_resource_remainder(aoe::Player::blue)
    );

    aoe::GameMap shared_map(20, 8);
    shared_map.set_terrain({10, 3}, aoe::Terrain::gold_mine);
    shared_map.set_resource_amount({10, 3}, 400);
    aoe::Simulation shared(std::move(shared_map));
    const auto blue = shared.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {9, 3}
    );
    const auto red = shared.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {11, 3}
    );
    shared.add_building(
        aoe::BuildingKind::mining_camp, aoe::Player::blue, {2, 2}
    );
    shared.add_building(
        aoe::BuildingKind::mining_camp, aoe::Player::red, {15, 2}
    );
    require(shared.set_civilization(
        aoe::Player::blue, aoe::Civilization::mayans
    ));
    require(shared.command_unit(blue, {10, 3}));
    require(shared.command_unit(red, {10, 3}));
    for (int tick = 0; tick < 100; ++tick) shared.update();
    const int blue_credit =
        shared.economy(aoe::Player::blue).gold - 200 +
        shared.units()[0].carried_amount;
    const int red_credit =
        shared.economy(aoe::Player::red).gold - 200 +
        shared.units()[1].carried_amount;
    const int total_consumed =
        400 - shared.map().resource_amount_at({10, 3});
    const int blue_consumed = total_consumed - red_credit;
    require(shared.mayan_resource_remainder(aoe::Player::red) == 0);
    require(
        blue_credit * 100 ==
        blue_consumed * 115 +
            shared.mayan_resource_remainder(aoe::Player::blue)
    );
}

void omitted_dat_civilization_economy_bonuses_are_exact_and_durable() {
    const auto mining_sample = [](
        aoe::Civilization civilization,
        aoe::Terrain terrain
    ) {
        aoe::GameMap map(8, 6);
        map.set_terrain({3, 2}, terrain);
        aoe::Simulation simulation(std::move(map));
        const auto worker = simulation.add_unit(
            aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {6, 4}
        );
        require(simulation.set_civilization(
            aoe::Player::blue, civilization
        ));
        require(simulation.command_unit(worker, {3, 2}));
        simulation.update();
        return std::pair{
            simulation.units().front().carried_amount,
            simulation.units().front().gather_work_remainder
        };
    };
    require(mining_sample(
        aoe::Civilization::generic, aoe::Terrain::gold_mine
    ) == std::pair{1, 0});
    require(mining_sample(
        aoe::Civilization::turks, aoe::Terrain::gold_mine
    ) == std::pair{1, 1500});
    require(mining_sample(
        aoe::Civilization::generic, aoe::Terrain::stone_mine
    ) == std::pair{1, 0});
    require(mining_sample(
        aoe::Civilization::koreans, aoe::Terrain::stone_mine
    ) == std::pair{1, 2000});

    aoe::Simulation hunt(aoe::GameMap(8, 6));
    const auto hunter = hunt.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    const auto deer = hunt.add_unit(
        aoe::UnitKind::deer, aoe::Player::neutral, {3, 2}
    );
    hunt.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {6, 4}
    );
    auto hunt_units = hunt.units();
    hunt_units[1].hit_points = 0;
    hunt.replace_state(
        std::move(hunt_units), hunt.buildings(),
        hunt.economy(aoe::Player::blue),
        hunt.economy(aoe::Player::red), 0
    );
    require(hunt.set_civilization(
        aoe::Player::blue, aoe::Civilization::mongols
    ));
    require(hunt.command_gather_unit(hunter, deer));
    hunt.update();
    require(hunt.units()[0].carried_amount == 1);
    require(hunt.units()[0].gather_work_remainder == 5000);

    const auto persian_progress = [](aoe::Age age) {
        aoe::Simulation simulation(aoe::GameMap(12, 8));
        const auto town_center = simulation.add_building(
            aoe::BuildingKind::town_center, aoe::Player::blue, {1, 1}
        );
        simulation.add_building(
            aoe::BuildingKind::house, aoe::Player::red, {9, 5}
        );
        require(simulation.set_civilization(
            aoe::Player::blue, aoe::Civilization::persians
        ));
        simulation.replace_ages(age, aoe::Age::dark);
        require(simulation.queue_unit_at(
            town_center, aoe::UnitKind::villager
        ));
        const int initial =
            simulation.buildings().front().production_queue.front()
                .ticks_remaining;
        for (int tick = 0; tick < 7; ++tick) simulation.update();
        return initial -
            simulation.buildings().front().production_queue.front()
                .ticks_remaining;
    };
    require(persian_progress(aoe::Age::dark) == 7);
    require(persian_progress(aoe::Age::feudal) == 7);
    require(persian_progress(aoe::Age::castle) == 8);
    require(persian_progress(aoe::Age::imperial) == 8);

    aoe::Simulation aztecs(aoe::GameMap(8, 6));
    aztecs.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 2}
    );
    require(aztecs.set_civilization(
        aoe::Player::blue, aoe::Civilization::aztecs
    ));
    aztecs.replace_technologies(aoe::Player::blue, {
        aoe::Technology::sanctity,
        aoe::Technology::fervor,
        aoe::Technology::atonement,
    });
    require(aztecs.maximum_hit_points(aztecs.units().front()) == 60);

    aoe::Simulation vikings(aoe::GameMap(8, 6));
    vikings.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {2, 2}
    );
    require(vikings.set_civilization(
        aoe::Player::blue, aoe::Civilization::vikings
    ));
    require(!vikings.has_technology(
        aoe::Player::blue, aoe::Technology::wheelbarrow
    ));
    vikings.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    require(vikings.has_technology(
        aoe::Player::blue, aoe::Technology::wheelbarrow
    ));
    require(!vikings.has_technology(
        aoe::Player::blue, aoe::Technology::hand_cart
    ));
    vikings.replace_ages(aoe::Age::castle, aoe::Age::dark);
    require(vikings.has_technology(
        aoe::Player::blue, aoe::Technology::hand_cart
    ));
    require(vikings.effective_carry_capacity(
        vikings.units().front()
    ) == 18);

    const auto path = std::filesystem::temp_directory_path() /
        "aoe-exact-civilization-economy.save";
    aoe::save_game(vikings, path);
    const auto loaded = aoe::load_game(path);
    std::filesystem::remove(path);
    require(loaded.civilization(aoe::Player::blue) ==
        aoe::Civilization::vikings);
    require(loaded.has_technology(
        aoe::Player::blue, aoe::Technology::hand_cart
    ));

    aoe::Replay replay;
    replay.record(0, aoe::SetCivilizationCommand{
        aoe::Player::blue, aoe::Civilization::turks
    });
    aoe::Simulation replayed(aoe::GameMap(8, 6));
    replay.apply_current_tick(replayed);
    require(replayed.civilization(aoe::Player::blue) ==
        aoe::Civilization::turks);
}

void slot_indexed_entity_ownership_and_diplomacy_are_checked() {
    const auto blue = *aoe::PlayerSlotId::from_index(0);
    const auto red = *aoe::PlayerSlotId::from_index(1);
    const auto green = *aoe::PlayerSlotId::from_index(2);
    const auto roster = aoe::MatchRoster::create({
        {
            blue, true, aoe::TeamId::none(), false,
            {{"blue", aoe::RosterControllerKind::human}},
        },
        {
            red, true, aoe::TeamId::none(), false,
            {{"red", aoe::RosterControllerKind::computer}},
        },
        {
            green, true, aoe::TeamId::none(), false,
            {{"green", aoe::RosterControllerKind::computer}},
        },
    });
    require(roster.has_value());
    auto diplomacy = *aoe::RosterDiplomacy::create(*roster);
    require(diplomacy.set_stance(blue, green, aoe::Diplomacy::ally));
    require(diplomacy.set_stance(green, blue, aoe::Diplomacy::enemy));

    aoe::Simulation simulation(aoe::GameMap(12, 8));
    simulation.replace_roster(*roster, diplomacy);
    auto green_state = simulation.player_state(green);
    green_state.economy = {311, 422, 533, 644};
    green_state.age = aoe::Age::castle;
    green_state.civilization = aoe::Civilization::mongols;
    green_state.controller = aoe::PlayerControllerState::active;
    green_state.victory_countdown = 77;
    green_state.countdown_kind = aoe::VictoryCountdownKind::wonder;
    simulation.replace_player_state(green, green_state);

    const auto green_unit = simulation.add_unit(
        aoe::UnitKind::militia, green, {6, 4}
    );
    simulation.add_unit(aoe::UnitKind::militia, blue, {3, 4});
    simulation.add_unit(aoe::UnitKind::militia, red, {9, 4});
    const auto found = std::ranges::find(
        simulation.units(), green_unit, &aoe::Unit::id
    );
    require(found != simulation.units().end());
    require(found->owner.stable_id() == 2);
    require(!found->owner.legacy_player().has_value());
    require(simulation.economy(green).wood == 311);
    require(simulation.economy(aoe::Player::blue).wood != 311);
    require(simulation.age(green) == aoe::Age::castle);
    require(simulation.civilization(green) == aoe::Civilization::mongols);
    require(simulation.victory_countdown(green) == 77);
    require(simulation.victory_countdown(aoe::Player::blue) != 77);
    require(simulation.diplomacy(blue, green) == aoe::Diplomacy::ally);
    require(simulation.diplomacy(green, blue) == aoe::Diplomacy::enemy);
    require(simulation.is_enemy(
        found->owner, aoe::EntityOwner{aoe::Player::red}
    ));
    require(!simulation.is_enemy(
        found->owner, aoe::EntityOwner{aoe::Player::neutral}
    ));
    bool neutral_rejected = false;
    try {
        (void)simulation.player_state(aoe::PlayerSlotId::neutral());
    } catch (const std::invalid_argument&) {
        neutral_rejected = true;
    }
    require(neutral_rejected);

    aoe::Simulation combat(aoe::GameMap(12, 8));
    combat.replace_roster(*roster, *aoe::RosterDiplomacy::create(*roster));
    combat.replace_player_state(green, green_state);
    const auto blue_id = combat.add_unit(
        aoe::UnitKind::militia, blue, {4, 4}
    );
    const auto green_id = combat.add_unit(
        aoe::UnitKind::champion, green, {6, 4}
    );
    const auto red_id = combat.add_unit(
        aoe::UnitKind::militia, red, {7, 4}
    );
    std::vector<aoe::Unit> combat_units = combat.units();
    auto green_unit_it = std::ranges::find(
        combat_units, green_id, &aoe::Unit::id
    );
    auto red_unit_it = std::ranges::find(
        combat_units, red_id, &aoe::Unit::id
    );
    require(green_unit_it != combat_units.end());
    require(red_unit_it != combat_units.end());
    green_unit_it->attack = 100;
    green_unit_it->attack_target_id = red_id;
    green_unit_it->attack_target_auto = false;
    red_unit_it->hit_points = 1;
    combat.replace_state(
        std::move(combat_units),
        {},
        combat.economy(aoe::Player::blue),
        combat.economy(aoe::Player::red),
        0
    );
    combat.update();
    require(combat.player_statistics(green).units_killed == 1);
    require(combat.player_statistics(red).units_lost == 1);
    require(std::ranges::find(
        combat.units(), blue_id, &aoe::Unit::id
    ) != combat.units().end());

    const auto house_id = combat.add_building(
        aoe::BuildingKind::house, red, {7, 4}
    );
    combat_units = combat.units();
    std::vector<aoe::Building> combat_buildings = combat.buildings();
    green_unit_it = std::ranges::find(
        combat_units, green_id, &aoe::Unit::id
    );
    auto house = std::ranges::find(
        combat_buildings, house_id, &aoe::Building::id
    );
    require(green_unit_it != combat_units.end());
    require(house != combat_buildings.end());
    green_unit_it->attack_cooldown = 0;
    green_unit_it->attack_target_id = house_id;
    green_unit_it->attack_target_is_building = true;
    house->hit_points = 1;
    combat.replace_state(
        std::move(combat_units),
        std::move(combat_buildings),
        combat.economy(aoe::Player::blue),
        combat.economy(aoe::Player::red),
        combat.tick_number()
    );
    combat.update();
    require(combat.player_statistics(green).buildings_razed == 1);
    require(combat.player_statistics(red).buildings_lost == 1);
    require(combat.roster_outcome().status ==
        aoe::RosterOutcomeStatus::ongoing);
    require(combat.resign(blue));
    const auto green_victory = combat.roster_outcome();
    require(green_victory.status == aoe::RosterOutcomeStatus::victory);
    require(green_victory.winning_slots ==
        std::vector<std::uint8_t>{2});
    require(!combat.legacy_roster_outcome().has_value());

    const auto team_one = *aoe::TeamId::numbered(1);
    const auto team_two = *aoe::TeamId::numbered(2);
    const auto teamed_roster = aoe::MatchRoster::create({
        {
            blue, true, team_one, false,
            {{"blue-team", aoe::RosterControllerKind::human}},
        },
        {
            red, true, team_two, false,
            {{"red-solo", aoe::RosterControllerKind::computer}},
        },
        {
            green, true, team_one, false,
            {{"green-team", aoe::RosterControllerKind::computer}},
        },
    });
    require(teamed_roster.has_value());
    aoe::Simulation two_vs_one(aoe::GameMap(10, 8));
    two_vs_one.replace_roster(
        *teamed_roster,
        *aoe::RosterDiplomacy::create(*teamed_roster)
    );
    two_vs_one.add_unit(aoe::UnitKind::militia, blue, {2, 3});
    two_vs_one.add_unit(aoe::UnitKind::militia, green, {7, 3});
    const auto team_victory = two_vs_one.roster_outcome();
    require(team_victory.status == aoe::RosterOutcomeStatus::victory);
    require(team_victory.winning_team == 1);
    require(team_victory.winning_slots ==
        std::vector<std::uint8_t>({0, 2}));
    require(!two_vs_one.legacy_roster_outcome().has_value());

    aoe::Simulation tie_order(aoe::GameMap(12, 8));
    tie_order.replace_roster(
        *roster, *aoe::RosterDiplomacy::create(*roster)
    );
    tie_order.replace_player_state(green, green_state);
    const auto deciding_unit = tie_order.add_unit(
        aoe::UnitKind::militia, green, {6, 4}
    );
    tie_order.add_unit(aoe::UnitKind::militia, red, {4, 4});
    const auto lower_slot_target = tie_order.add_unit(
        aoe::UnitKind::militia, blue, {8, 4}
    );
    tie_order.update();
    const auto decider = std::ranges::find(
        tie_order.units(), deciding_unit, &aoe::Unit::id
    );
    require(decider != tie_order.units().end());
    require(decider->attack_target_id == lower_slot_target);

    aoe::Scenario roster_scenario(12, 8);
    roster_scenario.roster_schema = true;
    roster_scenario.roster_entries = {
        {
            roster->slot(blue),
            {111, 222, 333, 444},
            aoe::Age::feudal,
            aoe::Civilization::britons,
            {aoe::Technology::fletching},
            aoe::FormationKind::line,
        },
        {
            roster->slot(red),
            {211, 322, 433, 544},
            aoe::Age::dark,
            aoe::Civilization::franks,
            {},
            aoe::FormationKind::compact,
        },
        {
            roster->slot(green),
            {311, 422, 533, 644},
            aoe::Age::castle,
            aoe::Civilization::mongols,
            {aoe::Technology::husbandry},
            aoe::FormationKind::flank,
        },
    };
    roster_scenario.directed_diplomacy = {
        {blue, red, aoe::Diplomacy::enemy},
        {red, blue, aoe::Diplomacy::enemy},
        {blue, green, aoe::Diplomacy::ally},
        {green, blue, aoe::Diplomacy::enemy},
        {red, green, aoe::Diplomacy::enemy},
        {green, red, aoe::Diplomacy::enemy},
    };
    roster_scenario.units.push_back({
        aoe::UnitKind::militia,
        aoe::entity_owner_from_slot(green),
        {6, 4},
    });
    roster_scenario.buildings.push_back({
        aoe::BuildingKind::house,
        aoe::entity_owner_from_slot(green),
        {8, 4},
    });
    const auto scenario_path =
        std::filesystem::temp_directory_path() /
        "aoe-roster-v67.scenario";
    aoe::save_scenario(roster_scenario, scenario_path);
    const aoe::Scenario loaded_roster =
        aoe::load_scenario(scenario_path);
    std::ifstream roster_input(scenario_path);
    const std::string roster_bytes(
        std::istreambuf_iterator<char>{roster_input},
        std::istreambuf_iterator<char>{}
    );
    std::filesystem::remove(scenario_path);
    require(roster_bytes.starts_with(
        "AOE-ARCHAEOLOGY-SCENARIO " +
        std::to_string(aoe::reconstruction_scenario_version) + '\n'
    ));
    require(loaded_roster.roster_schema);
    require(loaded_roster.roster_entries.size() == 3);
    const aoe::Simulation roster_simulation =
        aoe::create_simulation(loaded_roster);
    require(roster_simulation.economy(green).wood == 311);
    require(roster_simulation.age(green) == aoe::Age::castle);
    require(roster_simulation.civilization(green) ==
        aoe::Civilization::mongols);
    require(roster_simulation.units().front().owner.stable_id() == 2);
    require(roster_simulation.buildings().front().owner.stable_id() == 2);
    require(roster_simulation.diplomacy(
        blue, green
    ) == aoe::Diplomacy::ally);
    require(roster_simulation.diplomacy(
        green, blue
    ) == aoe::Diplomacy::enemy);

    const auto malformed_rejected = [&scenario_path](
        const std::string& bytes
    ) {
        {
            std::ofstream output(scenario_path);
            output << bytes;
        }
        bool rejected{};
        try {
            (void)aoe::load_scenario(scenario_path);
        } catch (const std::exception&) {
            rejected = true;
        }
        std::filesystem::remove(scenario_path);
        require(rejected);
    };
    const auto duplicate_position = roster_bytes.find(
        "player-slot 1 "
    );
    malformed_rejected(
        roster_bytes.substr(0, duplicate_position) +
        "player-slot 0 human \"duplicate\" 1 1 1 1 dark generic 0 0\n" +
        roster_bytes.substr(duplicate_position)
    );
    std::string neutral_roster = roster_bytes;
    neutral_roster.replace(
        neutral_roster.find("player-slot 2 "),
        std::string("player-slot 2 ").size(),
        "player-slot 8 "
    );
    malformed_rejected(neutral_roster);
    std::string unoccupied_owner = roster_bytes;
    unoccupied_owner.replace(
        unoccupied_owner.find("unit militia 2 "),
        std::string("unit militia 2 ").size(),
        "unit militia 7 "
    );
    malformed_rejected(unoccupied_owner);
    std::string incomplete_diplomacy = roster_bytes;
    const auto last_diplomacy =
        incomplete_diplomacy.find("diplomacy 2 1 enemy\n");
    incomplete_diplomacy.erase(
        last_diplomacy,
        std::string("diplomacy 2 1 enemy\n").size()
    );
    malformed_rejected(incomplete_diplomacy);

    {
        std::ofstream legacy(scenario_path);
        legacy <<
            "AOE-ARCHAEOLOGY-SCENARIO 64\n"
            "map 8 6\n"
            "economy blue 321 654 7 8\n"
            "economy red 123 456 9 10\n"
            "age blue castle\n"
            "age red feudal\n"
            "diplomacy enemy\n"
            "civilization blue britons\n"
            "civilization red franks\n"
            "unit militia blue 2 2\n"
            "unit sheep neutral 4 2 food 100\n";
    }
    const aoe::Scenario upgraded_legacy =
        aoe::load_scenario(scenario_path);
    std::filesystem::remove(scenario_path);
    require(upgraded_legacy.roster_schema);
    require(upgraded_legacy.roster_entries.size() == 2);
    require(upgraded_legacy.blue_economy.wood == 321);
    require(upgraded_legacy.red_economy.food == 456);
    require(upgraded_legacy.units[0].owner == aoe::Player::blue);
    require(upgraded_legacy.units[1].owner == aoe::Player::neutral);

    aoe::Replay sourced;
    sourced.record(0, blue, aoe::StopUnitCommand{blue_id});
    sourced.record(0, red, aoe::StopUnitCommand{red_id});
    sourced.record(0, green, aoe::StopUnitCommand{green_id});
    require(sourced.commands().size() == 3);
    require(sourced.commands()[0].source == blue);
    require(sourced.commands()[1].source == red);
    require(sourced.commands()[2].source == green);

    aoe::Replay unresolved_missing;
    unresolved_missing.record(0, aoe::StopUnitCommand{999999});
    aoe::Simulation unresolved_target(aoe::GameMap(8, 6));
    bool unresolved_rejected{};
    try {
        unresolved_missing.apply_current_tick(unresolved_target);
    } catch (const std::runtime_error&) {
        unresolved_rejected = true;
    }
    require(unresolved_rejected);

    aoe::Simulation command_validation(aoe::GameMap(8, 6));
    const aoe::EntityId validation_blue = command_validation.add_unit(
        aoe::UnitKind::militia, aoe::Player::blue, {1, 1}
    );
    aoe::Replay unoccupied_source;
    unoccupied_source.record(
        0, *aoe::PlayerSlotId::from_index(3),
        aoe::StopUnitCommand{validation_blue}
    );
    bool unoccupied_rejected{};
    try {
        unoccupied_source.apply_current_tick(command_validation);
    } catch (const std::runtime_error&) {
        unoccupied_rejected = true;
    }
    require(unoccupied_rejected);

    auto observer_state = command_validation.player_state(blue);
    observer_state.controller = aoe::PlayerControllerState::observer;
    command_validation.replace_player_state(blue, observer_state);
    aoe::Replay observer_source;
    observer_source.record(
        0, blue, aoe::StopUnitCommand{validation_blue}
    );
    bool observer_rejected{};
    try {
        observer_source.apply_current_tick(command_validation);
    } catch (const std::runtime_error&) {
        observer_rejected = true;
    }
    require(observer_rejected);

    const auto replay_path =
        std::filesystem::temp_directory_path() /
        "aoe-replay-v63-slot2.replay";
    aoe::save_replay(sourced, replay_path);
    const aoe::Replay loaded_sourced = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(loaded_sourced.commands().size() == 3);
    require(loaded_sourced.commands()[0].source == blue);
    require(loaded_sourced.commands()[1].source == red);
    require(loaded_sourced.commands()[2].source == green);

    auto native_statistics = simulation.match_statistics();
    native_statistics.players[2].units_created = 17;
    native_statistics.players[2].gathered.gold = 919;
    aoe::StatisticsTimelineSample native_sample;
    native_sample.tick = 7;
    native_sample.score[2] = 321;
    native_sample.population[2] = 4;
    native_sample.gathered[2].gold = 919;
    native_statistics.timeline.push_back(native_sample);
    simulation.replace_match_statistics(native_statistics);
    const auto save_path =
        std::filesystem::temp_directory_path() /
        "aoe-save-v109-roster.save";
    aoe::save_game(simulation, save_path);
    const aoe::Simulation restored = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(restored.roster().slot(green).occupied);
    require(restored.roster().slot(green).controllers.front().id ==
        "green");
    require(restored.economy(green).wood == 311);
    require(restored.age(green) == aoe::Age::castle);
    require(restored.civilization(green) ==
        aoe::Civilization::mongols);
    require(restored.victory_countdown(green) == 77);
    require(restored.countdown_kind(green) ==
        aoe::VictoryCountdownKind::wonder);
    require(restored.diplomacy(blue, green) ==
        aoe::Diplomacy::ally);
    require(restored.diplomacy(green, blue) ==
        aoe::Diplomacy::enemy);
    require(restored.units().front().owner.stable_id() == 2);
    require(restored.player_statistics(green).units_created == 17);
    require(restored.player_statistics(green).gathered.gold == 919);
    require(restored.match_statistics().timeline.back().score[2] == 321);
}

void town_bell_is_bounded_recallable_and_persistent() {
    // Alarm state belongs to selected Town Center, not global UI state.
    aoe::Simulation simulation(aoe::GameMap(40, 20));
    const auto town_center = simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {10, 10}
    );
    simulation.add_building(
        aoe::BuildingKind::town_center, aoe::Player::red, {22, 10}
    );
    const auto castle = simulation.add_building(
        aoe::BuildingKind::castle, aoe::Player::blue, {30, 10}
    );
    const auto first = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {5, 10}
    );
    const auto second = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {7, 12}
    );
    const auto remote = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {39, 18}
    );
    const auto near_castle = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {29, 10}
    );
    const auto enemy = simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::red, {9, 10}
    );
    require(simulation.command_unit(first, {4, 10}));
    require(aoe::execute(
        simulation, aoe::TownBellCommand{town_center}
    ));
    const auto active = std::ranges::find(
        simulation.buildings(), town_center, &aoe::Building::id
    );
    require(active != simulation.buildings().end());
    require(active->town_bell_source_id == town_center);
    require(std::ranges::find(
        simulation.units(), first, &aoe::Unit::id
    )->town_bell_source_id == town_center);
    require(std::ranges::find(
        simulation.units(), second, &aoe::Unit::id
    )->town_bell_source_id == town_center);
    require(std::ranges::find(
        simulation.units(), remote, &aoe::Unit::id
    )->town_bell_source_id == 0);
    require(std::ranges::find(
        simulation.units(), near_castle, &aoe::Unit::id
    )->garrison_target_id == castle);
    require(std::ranges::find(
        simulation.buildings(), castle, &aoe::Building::id
    )->town_bell_source_id == town_center);
    require(std::ranges::find(
        simulation.units(), enemy, &aoe::Unit::id
    )->town_bell_source_id == 0);

    const auto save_path = std::filesystem::temp_directory_path() /
        "aoe-town-bell.save";
    aoe::save_game(simulation, save_path);
    aoe::Simulation loaded = aoe::load_game(save_path);
    std::filesystem::remove(save_path);
    require(std::ranges::find(
        loaded.buildings(), town_center, &aoe::Building::id
    )->town_bell_source_id == town_center);
    require(std::ranges::find(
        loaded.units(), first, &aoe::Unit::id
    )->town_bell_source_id == town_center);

    for (int tick = 0; tick < 80 &&
         std::ranges::find(
             loaded.units(), first, &aoe::Unit::id
         )->garrisoned_in == 0; ++tick) {
        loaded.update();
    }
    require(std::ranges::find(
        loaded.units(), first, &aoe::Unit::id
    )->garrisoned_in == town_center);
    require(aoe::execute(loaded, aoe::TownBellCommand{town_center}));
    const auto recalled = std::ranges::find(
        loaded.units(), first, &aoe::Unit::id
    );
    require(recalled->garrisoned_in == 0);
    require(recalled->town_bell_source_id == 0);
    require(recalled->destination == aoe::TilePosition{4, 10});
    require(std::ranges::find(
        loaded.buildings(), town_center, &aoe::Building::id
    )->town_bell_source_id == 0);

    aoe::Replay replay;
    replay.record(0, aoe::TownBellCommand{town_center});
    const auto replay_path = std::filesystem::temp_directory_path() /
        "aoe-town-bell.replay";
    aoe::save_replay(replay, replay_path);
    const aoe::Replay decoded = aoe::load_replay(replay_path);
    std::filesystem::remove(replay_path);
    require(decoded.commands().size() == 1);
    require(std::get<aoe::TownBellCommand>(
        decoded.commands().front().command
    ).building == town_center);

    aoe::LockstepFrame frame{
        aoe::LockstepFrameKind::turn,
        aoe::lockstep_protocol_version,
        aoe::Player::blue,
        "town-bell-scenario",
        0,
        0,
        aoe::deterministic_state_hash(simulation),
        {aoe::TownBellCommand{town_center}},
    };
    const auto wire = aoe::decode_lockstep_frame(
        aoe::encode_lockstep_frame(frame)
    );
    require(wire.commands.size() == 1);
    require(std::get<aoe::TownBellCommand>(wire.commands.front()).building ==
            town_center);

    aoe::Simulation full(aoe::GameMap(30, 20));
    const auto full_center = full.add_building(
        aoe::BuildingKind::town_center, aoe::Player::blue, {10, 10}
    );
    for (int index = 0; index < 15; ++index) {
        const auto occupant = full.add_unit(
            aoe::UnitKind::archer, aoe::Player::blue, {3 + index, 2}
        );
        require(full.restore_garrison(occupant, full_center));
    }
    const auto overflow = full.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {8, 10}
    );
    require(full.command_town_bell(full_center));
    require(std::ranges::find(
        full.units(), overflow, &aoe::Unit::id
    )->town_bell_source_id == 0);
}

int main() {
    if (std::getenv("AOE_GATE_TEST") != nullptr) {
        gates_honor_allies_locks_and_enemy_exclusion();
        return 0;
    }
    if (std::getenv("AOE_ATTACK_REVEAL_TEST") != nullptr) {
        enemy_attackers_reveal_per_victim_through_attack_action();
        return 0;
    }
    if (std::getenv("AOE_SLOT_TEST") != nullptr) {
        slot_indexed_entity_ownership_and_diplomacy_are_checked();
        return 0;
    }
    const auto run = [](const char* name, auto test) {
        std::cout << "Running " << name << '\n' << std::flush;
        test();
    };
    run(
        "exact executable conversion arithmetic",
        executable_conversion_arithmetic_is_exact
    );
    run(
        "commercial conversion random stream and schedule",
        commercial_conversion_stream_and_schedule_are_exact
    );
    run("unit movement", unit_moves_deterministically);
    run(
        "persistent logical facing",
        logical_facing_persists_and_actions_turn_toward_targets
    );
    run(
        "land cliff routing progress",
        land_route_detours_around_cliff_and_makes_progress
    );
    run(
        "presentation elevation state lifecycle",
        presentation_elevation_state_initializes_and_replaces_safely
    );
    run(
        "replay loader validation",
        replay_loader_rejects_malformed_and_invalid_enums
    );
    run(
        "save production queue validation",
        save_loader_rejects_truncated_production_queue
    );
    run(
        "save loaded placement validation",
        save_loader_rejects_invalid_loaded_placements
    );
    run(
        "save loaded resource validation",
        loaded_resource_state_rejects_negative_and_over_cap_values
    );
    run(
        "Beach and Shallows DAT restrictions",
        beach_and_shallows_follow_live_dat_restrictions
    );
    run("relative movement speed", cavalry_moves_faster_than_foot_units);
    run(
        "blocked cavalry movement credit",
        blocked_cavalry_does_not_bank_movement_credit
    );
    run("movement cooldown save", save_preserves_movement_cooldown);
    run(
        "wheelbarrow movement speed",
        wheelbarrow_adds_exact_persisted_villager_speed
    );
    run("area selection", area_selection_selects_only_owned_units);
    run(
        "formation slots",
        formations_allocate_unique_reachable_slots_around_obstacles
    );
    run(
        "idle villagers",
        idle_villagers_exclude_every_active_work_order
    );
    run(
        "idle military",
        idle_military_excludes_persistent_combat_orders
    );
    run("vision and exploration", vision_reveals_and_remembers_explored_tiles);
    run(
        "temporary enemy attacker reveal",
        enemy_attackers_reveal_per_victim_through_attack_action
    );
    run(
        "exploration sweep equivalence",
        exploration_sweep_matches_per_tile_visibility
    );
    run(
        "exploration save",
        save_round_trip_preserves_exploration_memory
    );
    run(
        "per-viewer stale building memory",
        enemy_building_memory_is_stale_per_viewer_and_persistent
    );
    run(
        "allied starting Town Center reveal",
        allied_starting_town_centers_reveal_without_shared_vision
    );
    run(
        "radial building LOS",
        building_los_is_radial_from_nearest_footprint_and_persists
    );
    run("wood gathering", villager_gathers_wood);
    run(
        "temporary gathering route obstruction",
        gathering_retries_after_temporary_route_obstruction
    );
    run(
        "initial gathering route blockage",
        gathering_order_survives_initial_route_blockage
    );
    run(
        "gathering collision pause",
        gathering_collision_pauses_before_fresh_route
    );
    run(
        "blocked valid gathering drop off",
        returning_gatherer_retries_blocked_valid_drop_off
    );
    run(
        "depleted gathering retarget before arrival",
        gatherer_retargets_depleted_resource_before_arrival
    );
    run(
        "diagonal berry gathering",
        diagonal_berry_workers_gather_without_route_churn
    );
    run(
        "crowded berry gathering ring",
        crowded_berry_ring_allows_nearby_workers_to_gather
    );
    run(
        "temporarily unavailable gathering drop off",
        gathering_waits_for_a_temporarily_unavailable_drop_off
    );
    run(
        "shared gathering through repeated deposits",
        villagers_share_resource_through_repeated_deposit_cycles
    );
    run(
        "late shared-resource depletion retarget",
        late_arriving_villager_retargets_after_shared_depletion
    );
    run(
        "land gathering replay determinism",
        land_gathering_command_is_deterministic_through_replay
    );
    run(
        "double-bit axe wood rate",
        double_bit_axe_adds_exact_persisted_wood_rate
    );
    run("finite forest depletion", forest_depletes_after_finite_wood_is_delivered);
    run(
        "resource retasking",
        villagers_continue_to_nearest_same_resource_after_depletion
    );
    run(
        "sheep retask after gold",
        sheep_retask_after_gold_deposit_carries_food
    );
    run("four resource gathering", villagers_deliver_all_resource_types);
    run(
        "specialized drop offs",
        villagers_choose_compatible_specialized_drop_offs
    );
    run("drop off recovery", destroyed_drop_off_reroutes_carrier);
    run("farm lifecycle", farms_construct_harvest_exhaust_and_reseed);
    run(
        "farm reseed payment",
        farm_reseed_payment_is_atomic_and_civilization_aware
    );
    run(
        "horse collar farm capacity",
        horse_collar_upgrades_existing_future_and_reseeded_farms
    );
    run(
        "fortified wall upgrade",
        fortified_wall_upgrades_existing_future_walls_and_gates
    );
    run(
        "guard tower upgrade",
        guard_tower_upgrades_existing_future_towers_and_attack
    );
    run(
        "keep upgrade",
        keep_requires_guard_tower_and_upgrades_tower_line
    );
    run(
        "bodkin arrow upgrade",
        bodkin_arrow_requires_fletching_and_upgrades_arrow_attacks
    );
    run(
        "bracer upgrade",
        bracer_requires_bodkin_and_completes_missile_line
    );
    run(
        "iron casting upgrade",
        iron_casting_requires_forging_and_excludes_generic_villagers
    );
    run(
        "blast furnace upgrade",
        blast_furnace_requires_iron_casting_and_adds_two_attack
    );
    run(
        "scale mail armor upgrade",
        scale_mail_armor_upgrades_infantry_and_excludes_other_units
    );
    run(
        "chain mail armor upgrade",
        chain_mail_armor_requires_scale_mail_and_stacks_in_combat
    );
    run(
        "plate mail armor upgrade",
        plate_mail_armor_requires_chain_mail_and_finishes_infantry_armor
    );
    run(
        "scale barding armor upgrade",
        scale_barding_armor_upgrades_cavalry_and_applies_in_combat
    );
    run(
        "chain barding armor upgrade",
        chain_barding_armor_requires_scale_barding_and_stacks
    );
    run(
        "plate barding armor upgrade",
        plate_barding_armor_requires_chain_and_finishes_cavalry_armor
    );
    run(
        "padded archer armor upgrade",
        padded_archer_armor_upgrades_archers_and_reduces_projectile_damage
    );
    run(
        "leather archer armor upgrade",
        leather_archer_armor_requires_padded_and_stacks_against_arrows
    );
    run(
        "ring archer armor upgrade",
        ring_archer_armor_requires_leather_and_finishes_archer_armor
    );
    run(
        "bloodlines upgrade",
        bloodlines_adds_twenty_hit_points_to_current_and_future_cavalry
    );
    run(
        "husbandry upgrade",
        husbandry_adds_exact_persisted_cavalry_speed
    );
    run(
        "cavalier upgrade",
        cavalier_upgrade_converts_existing_queued_and_future_knights
    );
    run(
        "paladin upgrade",
        paladin_requires_cavalier_and_finishes_heavy_cavalry_line
    );
    run(
        "light cavalry upgrade",
        light_cavalry_upgrade_converts_scout_line
    );
    run(
        "hussar upgrade",
        hussar_requires_light_cavalry_and_finishes_scout_line
    );
    run(
        "two-handed swordsman upgrade",
        two_handed_swordsman_finishes_imperial_infantry_step
    );
    run(
        "champion upgrade",
        champion_completes_classic_militia_line
    );
    run(
        "arbalester upgrade",
        arbalester_completes_classic_foot_archer_line
    );
    run(
        "elite skirmisher upgrade",
        elite_skirmisher_completes_castle_counter_line
    );
    run("river routing", path_routes_through_river_crossing);
    run("save round trip", save_round_trip_preserves_state);
    run("villager production", town_center_produces_villager);
    run(
        "completed villager retry",
        completed_villager_order_retries_once_on_valid_land
    );
    run(
        "building and knight production",
        villager_constructs_stable_and_trains_knight
    );
    run(
        "active construction save",
        save_round_trip_preserves_active_construction
    );
    run(
        "construction pause and resume",
        construction_pauses_and_resumes_with_builder
    );
    run("named rules", named_rules_define_balance_values);
    run(
        "scout cavalry age bonuses",
        scout_cavalry_receives_original_automatic_age_bonuses
    );
    run(
        "scout cavalry age movement",
        scout_cavalry_uses_exact_persisted_age_movement_rates
    );
    run(
        "military resource costs",
        military_training_charges_food_and_gold_atomically
    );
    run("age progression", ages_gate_content_and_progress_deterministically);
    run("archer production", archery_range_trains_archer);
    run(
        "blacksmith construction",
        blacksmith_construction_requires_feudal_age
    );
    run("castle construction", castle_uses_atomic_stone_cost_and_persists);
    run(
        "castle defensive arrows",
        castle_defense_fires_delayed_persistent_arrows
    );
    run(
        "castle fletching",
        fletching_upgrades_castle_attack_and_range
    );
    run(
        "castle attacks buildings",
        castle_defense_targets_enemy_buildings
    );
    run(
        "castle minimum range",
        castle_minimum_range_protects_adjacent_attackers
    );
    run(
        "university murder holes",
        university_researches_persistent_murder_holes
    );
    run(
        "siege workshop ram production",
        siege_workshop_trains_persistent_battering_rams
    );
    run(
        "battering ram combat",
        battering_ram_counters_buildings_and_resists_arrows
    );
    run(
        "alternative imperial prerequisites",
        university_and_workshop_unlock_imperial_age
    );
    run(
        "building repair",
        villagers_repair_buildings_with_persistent_costs
    );
    run(
        "palisade walls",
        palisade_walls_construct_block_persist_and_fall
    );
    run(
        "palisade gates",
        palisade_gates_open_for_friendlies_block_enemies_and_persist
    );
    run(
        "gate diplomacy lock and enemy exclusion",
        gates_honor_allies_locks_and_enemy_exclusion
    );
    run(
        "stone gates",
        stone_gates_use_feudal_cost_armor_replay_and_persistence
    );
    run(
        "watch towers",
        watch_towers_construct_fire_upgrade_and_persist
    );
    run(
        "stone walls",
        stone_walls_gate_block_resist_and_persist
    );
    run(
        "skirmishers",
        skirmishers_train_counter_archers_and_persist
    );
    run(
        "fifteen-unit production queue",
        production_buildings_accept_fifteen_queued_units
    );
    run(
        "mangonel automatic target safety",
        mangonel_automatic_targets_avoid_friendly_splash
    );
    run(
        "mangonels",
        mangonels_train_splash_friendlies_and_persist
    );
    run(
        "mangonel attack ground",
        mangonel_attack_ground_moves_fires_splashes_and_replays
    );
    run(
        "man-at-arms upgrade",
        man_at_arms_research_upgrades_line_and_persists
    );
    run(
        "long swordsman upgrade",
        long_swordsman_research_upgrades_militia_line_and_persists
    );
    run(
        "crossbowman upgrade",
        crossbowman_research_upgrades_archer_line_and_persists
    );
    run(
        "pikeman upgrade",
        pikeman_research_upgrades_spear_line_and_persists
    );
    run(
        "castle population support",
        castle_provides_original_population_support
    );
    run(
        "castle footprint",
        castle_footprint_blocks_placement_and_routes_units
    );
    run(
        "castle footprint combat geometry",
        castle_footprint_drives_range_vision_and_projectile_edges
    );
    run(
        "explicit building target persistence",
        explicit_building_attack_persists_until_target_enters_vision
    );
    run("population houses", houses_raise_population_cap_only_after_completion);
    run("housing loss", housing_loss_stalls_completed_production);
    run(
        "ranged projectile",
        archer_projectile_has_delayed_persisted_impact
    );
    run(
        "projectile impact diplomacy",
        direct_projectiles_respect_diplomacy_at_impact
    );
    run(
        "radial combat collision borders",
        radial_combat_ranges_use_collision_box_borders
    );
    run(
        "radial minimum and religious ranges",
        radial_minimum_ranges_and_religious_ranges_are_circular
    );
    run(
        "radial target acquisition",
        radial_acquisition_prefers_true_nearest_target
    );
    run(
        "radial projectile travel",
        projectile_travel_uses_radial_distance_and_bounded_speed_rounding
    );
    run(
        "moving-target projectile tracking",
        archer_projectile_tracks_moving_target_deterministically
    );
    run("armor damage", armor_reduces_damage_but_preserves_minimum_hit);
    run(
        "unit death effects",
        unit_death_effect_collapses_persists_and_expires
    );
    run(
        "building rubble",
        destroyed_building_leaves_persistent_nonblocking_rubble
    );
    run("computer player", computer_player_moves_and_attacks);
    run(
        "computer player target persistence",
        computer_player_preserves_equal_distance_target_across_save
    );
    run(
        "computer scouting",
        computer_player_scouts_without_omniscient_targeting
    );
    run(
        "computer economy",
        computer_player_gathers_and_trains_without_cheats
    );
    run(
        "computer repeat housing",
        computer_player_repeats_housing_before_population_block
    );
    run(
        "computer age progression",
        computer_player_advances_and_builds_age_prerequisites_normally
    );
    run(
        "computer farming",
        computer_player_builds_harvests_and_reseeds_farms
    );
    run(
        "computer destroys final building",
        computer_player_targets_and_destroys_final_enemy_building
    );
    run(
        "large building footprint visibility",
        large_buildings_reveal_and_target_from_any_footprint_tile
    );
    run("unit victory", match_ends_after_last_enemy_is_destroyed);
    run(
        "resignation victory",
        resignation_immediately_awards_the_opponent_victory
    );
    run("building destruction", units_can_destroy_enemy_buildings);
    run("group attack", group_attackers_repath_and_focus_target);
    run("attack cooldown", attack_rate_obeys_unit_cooldown);
    run(
        "DAT attack release frame",
        ranged_release_waits_for_dat_frame_and_binds_target
    );
    run(
        "military auto acquisition",
        military_auto_acquires_only_visible_targets
    );
    run(
        "moving target pursuit",
        moving_target_pursuit_survives_save_and_replay
    );
    run("replay round trip", replay_round_trip_reproduces_state);
    run(
        "stable cavalry production",
        stable_owns_cavalry_production_and_persists_it
    );
    run("scout replay", scout_training_replay_is_deterministic);
    run(
        "barracks infantry production",
        barracks_trains_age_gated_infantry_and_persists_it
    );
    run("spearman cavalry bonus", spearman_bonus_damage_counters_cavalry);
    run("spearman replay", spearman_training_replay_is_deterministic);
    run(
        "loom survivability",
        loom_upgrades_villager_survivability_and_persists
    );
    run(
        "technology research",
        technologies_research_persist_and_apply_to_all_units
    );
    run(
        "technology replay",
        technology_replay_command_reproduces_research
    );
    run(
        "missing standard technologies",
        missing_standard_technologies_follow_live_dat
    );
    run("scenario round trip", scenario_resource_round_trip);
    run(
        "generated startup map original default",
        generated_startup_map_uses_original_default_and_ticks
    );
    run(
        "town center garrison",
        town_center_garrison_shelters_fires_and_persists
    );
    run(
        "defensive building garrisons",
        defensive_garrisons_use_dat_capacities_and_bounded_domains
    );
    run(
        "scenario garrison validation",
        scenario_garrisons_use_authoritative_domains_and_capacity
    );
    run(
        "garrison healing",
        garrison_healing_maps_dat_rates_to_deterministic_ticks
    );
    run(
        "garrison defensive volleys",
        garrisoned_archers_and_villagers_add_bounded_defensive_volleys
    );
    run(
        "town center construction",
        town_center_constructs_with_original_footprint_and_cost
    );
    run(
        "building rally points",
        building_rally_points_route_spawned_units_and_persist
    );
    run(
        "production cancellation",
        production_cancellation_refunds_last_order_and_preserves_progress
    );
    run(
        "stop command",
        stop_command_clears_all_orders_and_replays_for_groups
    );
    run(
        "attack move",
        attack_move_engages_visible_enemies_then_resumes_destination
    );
    run(
        "patrol",
        patrol_engages_enemies_and_loops_between_endpoints
    );
    run(
        "guard",
        guard_follows_protects_and_returns_to_friendly_target
    );
    run(
        "queued waypoints",
        queued_waypoints_run_multi_leg_routes_and_continue_after_combat
    );
    run(
        "unit stances",
        unit_stances_control_chasing_and_persist
    );
    run(
        "entity deletion",
        deleting_entities_refunds_unbuilt_cost_and_ejects_garrison
    );
    run(
        "multiple builders",
        multiple_builders_use_original_diminishing_returns_and_persist
    );
    run("sheep food", sheep_supply_food_without_using_population);
    run(
        "owned sheep player movement",
        owned_sheep_accepts_player_move_command
    );
    run(
        "visible neutral sheep capture and movement",
        visible_neutral_sheep_becomes_owned_alive_and_moves
    );
    run(
        "sheep native capture policy",
        sheep_capture_uses_native_radius_priority_and_chaining
    );
    run(
        "sheep movement groups replay and save",
        sheep_player_movement_groups_are_deterministic_and_persistent
    );
    run(
        "neutral sheep selection and gather",
        neutral_sheep_select_and_contextual_gather
    );
    run(
        "sheep persistence",
        sheep_state_round_trips_through_save_and_scenario
    );
    run(
        "sheep replay",
        sheep_gather_command_replays_deterministically
    );
    run(
        "sheep integration",
        sheep_integrate_with_pathing_vision_outcomes_and_ai
    );
    run("deer hunting", deer_are_passive_finite_huntable_food);
    run(
        "animal carcass decay",
        animal_carcass_decay_is_dat_rated_persistent_and_competitive
    );
    run(
        "boar hunting",
        boar_retaliate_and_hunt_state_persists_deterministically
    );
    run(
        "playthrough remote build hunt and terminal settle",
        playthrough_orders_support_remote_building_live_hunting_and_terminal_settle
    );
    run(
        "monk conversion",
        monks_convert_units_with_persisted_replayable_progress
    );
    run(
        "monk healing and relics",
        monks_heal_and_bank_neutral_relics_deterministically
    );
    run(
        "market exchange",
        markets_exchange_resources_at_shared_dynamic_prices
    );
    run(
        "diplomacy and allied trade",
        diplomacy_and_allied_trade_are_deterministic
    );
    run(
        "civilization bonuses",
        civilization_bonuses_are_scoped_and_persisted
    );
    run(
        "additional civilization bonuses",
        additional_civilization_bonuses_use_existing_systems
    );
    run(
        "exact omitted civilization economy bonuses",
        omitted_dat_civilization_economy_bonuses_are_exact_and_durable
    );
    run(
        "Mayan per-player resource duration",
        mayan_resource_duration_is_per_player_exact_and_persistent
    );
    run(
        "asian and saracen bonuses",
        asian_and_saracen_bonuses_are_exact_and_isolated
    );
    run(
        "final civilization bonuses",
        final_civilization_bonuses_use_supported_systems
    );
    run(
        "Conquerors civilization bonuses",
        conquerors_civilizations_use_supported_exact_bonuses
    );
    run(
        "new represented team bonuses",
        newly_represented_team_bonuses_follow_reciprocal_alliance
    );
    run(
        "represented team LOS bonuses",
        represented_team_los_bonuses_follow_alliance_without_stacking
    );
    run(
        "Dock shoreline and ship spawn geometry",
        dock_shoreline_and_ship_spawn_geometry_match_original
    );
    run(
        "naval fishing economy",
        docks_and_fishing_ships_form_a_water_only_food_economy
    );
    run(
        "naval combat and transport",
        galley_line_combat_and_transport_passengers_persist
    );
    run(
        "transport multi-passenger disembark",
        transport_disembark_spreads_all_passengers_on_connected_land
    );
    run(
        "fire and demolition ships",
        fire_and_demolition_ship_lines_are_dat_backed_and_persist
    );
    run(
        "cannon and dock technologies",
        cannon_galleons_and_dock_technologies_follow_live_dat
    );
    run(
        "civilization unique ships",
        civilization_unique_ship_lines_are_locked_and_deterministic
    );
    run(
        "castle unique units",
        castle_unique_lines_are_civilization_locked_and_upgrade
    );
    run(
        "civilization availability matrix",
        civilization_availability_matches_live_dat_matrix
    );
    run(
        "first unique technologies",
        first_unique_technologies_follow_live_dat_effects
    );
    run(
        "Conquerors siege eagles and trebuchets",
        conquerors_siege_eagles_and_trebuchets_are_vertical
    );
    run(
        "cavalry archer line",
        cavalry_archer_line_is_dat_backed_and_persistent
    );
    run("camel line", camel_line_is_exact_counter_and_persistent);
    run("ram upgrade line", ram_upgrades_are_exact_splashing_and_persistent);
    run("halberdier line", halberdier_line_is_exact_and_persistent);
    run("land gunpowder", chemistry_unlocks_exact_land_gunpowder);
    run(
        "broad siege and production technologies",
        broad_siege_and_production_technologies_are_bounded
    );
    run("petard", petards_unlock_and_explode_deterministically);
    run("bombard tower", bombard_towers_are_exact_unlocked_defenses);
    run(
        "missionary metadata and bounded religious effects",
        missionary_dat_metadata_and_bounded_religious_effects
    );
    run(
        "economy metadata and bounded rates",
        economy_technology_metadata_and_bounded_rates
    );
    run(
        "naval trade and fish traps",
        naval_trade_and_fish_traps_are_bounded_and_persistent
    );
    run(
        "caravan cartography and tribute",
        caravan_cartography_and_tribute_have_bounded_contracts
    );
    run(
        "defensive infrastructure",
        defensive_infrastructure_is_exact_and_bounded
    );
    run(
        "wonder and standard victories",
        wonder_and_standard_victories_are_bounded_and_persistent
    );
    run(
        "advanced formations",
        advanced_formations_are_oriented_stable_and_persistent
    );
    run(
        "durable formation semantic legs",
        formation_semantic_legs_and_boundary_are_durable
    );
    run(
        "formation movement remainder domain",
        formation_movement_credit_stays_in_its_denominator_domain
    );
    run(
        "configurable persistent computer strategy",
        computer_strategy_is_configurable_visible_and_persistent
    );
    run(
        "mixed-domain persistent computer strategy",
        computer_handles_mixed_domains_and_persists_deterministically
    );
    run(
        "bounded computer victory and team policy",
        computer_victory_and_team_policy_is_bounded_and_deterministic
    );
    run(
        "deterministic religious conversion policy",
        religious_conversion_resistance_and_group_policy_is_deterministic
    );
    run(
        "deterministic trade routes and invalidation",
        trade_routes_invalidate_and_persist_deterministically
    );
    run(
        "bounded garrison ejection and defense contribution",
        garrison_ejection_and_defensive_contribution_are_bounded
    );
    run(
        "deterministic allied victory aggregation",
        allied_victory_aggregates_objectives_deterministically
    );
    run(
        "executable persistent scenario triggers",
        executable_scenario_triggers_are_deterministic_and_persistent
    );
    run(
        "multi-action atomic scenario triggers",
        multi_action_triggers_are_atomic_and_persistent
    );
    run(
        "bounded campaign manifest and progress",
        campaign_manifest_and_progress_are_bounded_and_atomic
    );
    run(
        "campaign trigger save replay stress",
        campaign_trigger_save_replay_stress_is_deterministic
    );
    run(
        "multiplayer lockstep transport disorder",
        multiplayer_lockstep_handles_transport_disorder_deterministically
    );
    run(
        "lockstep negotiated session metadata",
        lockstep_session_metadata_is_canonical_and_strict
    );
    run(
        "multiplayer checkpoint barrier and envelope",
        multiplayer_checkpoint_requires_matching_barrier_and_digests
    );
    run(
        "localhost TCP multiplayer framing",
        localhost_tcp_frames_survive_fragmentation_and_disconnect
    );
    run(
        "checked slot entity ownership and diplomacy",
        slot_indexed_entity_ownership_and_diplomacy_are_checked
    );
    run(
        "Town Bell shelter recall and persistence",
        town_bell_is_bounded_recallable_and_persistent
    );
    std::cout << "All simulation tests passed\n";
}

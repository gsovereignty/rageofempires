#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "aoe/format_versions.hpp"
#include "aoe/match_statistics.hpp"
#include "aoe/save_game.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

aoe::MatchStatistics populated_statistics() {
    aoe::MatchStatistics statistics;
    auto& blue = statistics.players[0];
    blue.gathered = {101, 202, 303, 404};
    blue.tribute_sent = {11, 12, 13, 14};
    blue.tribute_received = {21, 22, 23, 24};
    blue.units_created = 31;
    blue.units_lost = 32;
    blue.units_killed = 33;
    blue.buildings_built = 41;
    blue.buildings_lost = 42;
    blue.buildings_razed = 43;
    blue.conversions = 51;
    blue.relics_collected = 52;
    blue.technologies_researched = 53;
    blue.wonders_built = 54;
    blue.age_times = {100, 200, 300};
    auto& red = statistics.players[1];
    red.gathered = {501, 502, 503, 504};
    red.tribute_sent = {61, 62, 63, 64};
    red.tribute_received = {71, 72, 73, 74};
    red.units_created = 81;
    red.units_lost = 82;
    red.units_killed = 83;
    red.buildings_built = 91;
    red.buildings_lost = 92;
    red.buildings_razed = 93;
    red.conversions = 101;
    red.relics_collected = 102;
    red.technologies_researched = 103;
    red.wonders_built = 104;
    red.age_times = {400, 500, 600};
    statistics.timeline.push_back({
        100, {10, 20}, {3, 4},
        {{{1, 2, 3, 4}, {5, 6, 7, 8}}},
    });
    return statistics;
}

void transition_boundaries_update_authoritative_counters() {
    aoe::Simulation simulation(aoe::GameMap(20, 20));
    simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {15, 15}
    );
    simulation.add_building(
        aoe::BuildingKind::market, aoe::Player::blue, {2, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::market, aoe::Player::red, {12, 3}
    );
    simulation.add_building(
        aoe::BuildingKind::wonder, aoe::Player::blue, {2, 10}
    );
    simulation.replace_diplomacy(aoe::Diplomacy::ally);
    require(
        simulation.tribute_resource(
            aoe::Player::blue, aoe::Player::red,
            aoe::ResourceKind::food, 20
        ),
        "tribute transition rejected"
    );
    for (int tick = 0; tick < 100; ++tick) simulation.update();
    const aoe::MatchStatistics statistics =
        simulation.match_statistics();
    require(
        statistics.for_player(aoe::Player::blue).units_created == 1 &&
        statistics.for_player(aoe::Player::red).units_created == 1,
        "unit creation not counted"
    );
    require(
        statistics.for_player(aoe::Player::blue).buildings_built == 2 &&
        statistics.for_player(aoe::Player::blue).wonders_built == 1 &&
        statistics.for_player(aoe::Player::red).buildings_built == 1,
        "building or Wonder creation not counted"
    );
    require(
        statistics.for_player(aoe::Player::blue).
                tribute_sent.food == 20 &&
        statistics.for_player(aoe::Player::red).
                tribute_received.food == 20,
        "tribute counters not paired"
    );
    require(
        statistics.timeline.size() == 1 &&
        statistics.timeline[0].tick == 100,
        "timeline cadence mismatch"
    );
    require(
        statistics.current_score[0] ==
            simulation.score(aoe::Player::blue) &&
        statistics.current_score[1] ==
            simulation.score(aoe::Player::red),
        "snapshot score is stale"
    );
}

void combat_removal_updates_loss_and_defeat_counters() {
    aoe::Simulation simulation(aoe::GameMap(12, 12));
    const aoe::EntityId blue_knight = simulation.add_unit(
        aoe::UnitKind::knight, aoe::Player::blue, {1, 1}
    );
    simulation.add_unit(
        aoe::UnitKind::militia, aoe::Player::red, {2, 1}
    );
    simulation.add_building(
        aoe::BuildingKind::house, aoe::Player::red, {4, 1}
    );

    std::vector<aoe::Unit> units = simulation.units();
    units[1].hit_points = 1;
    std::vector<aoe::Building> buildings = simulation.buildings();
    buildings[0].hit_points = 1;
    simulation.replace_state(
        std::move(units),
        std::move(buildings),
        simulation.economy(aoe::Player::blue),
        simulation.economy(aoe::Player::red),
        simulation.tick_number()
    );

    require(
        simulation.command_unit(blue_knight, {2, 1}),
        "unit attack command rejected"
    );
    simulation.update();
    require(
        simulation.command_unit(blue_knight, {4, 1}),
        "building attack command rejected"
    );
    for (int tick = 0;
         tick < 20 && !simulation.buildings().empty();
         ++tick) {
        simulation.update();
    }

    const aoe::MatchStatistics statistics =
        simulation.match_statistics();
    require(
        statistics.for_player(aoe::Player::red).units_lost == 1 &&
        statistics.for_player(aoe::Player::blue).units_killed == 1,
        "unit death transition not counted"
    );
    require(
        statistics.for_player(aoe::Player::red).buildings_lost == 1 &&
        statistics.for_player(aoe::Player::blue).buildings_razed == 1,
        "building destruction transition not counted"
    );
}

void save106_round_trip_preserves_every_counter() {
    aoe::Simulation simulation(aoe::GameMap(8, 8));
    aoe::MatchStatistics expected = populated_statistics();
    simulation.replace_match_statistics(expected);
    const auto path = std::filesystem::temp_directory_path() /
        "aoe-match-statistics-save106.save";
    aoe::save_game(simulation, path);
    aoe::Simulation restored = aoe::load_game(path);
    std::filesystem::remove(path);
    aoe::MatchStatistics actual = restored.match_statistics();
    expected.current_score = actual.current_score;
    expected.active_slots = actual.active_slots;
    expected.team_numbers = actual.team_numbers;
    require(actual == expected, "current save lost match statistics");
    require(
        aoe::reconstruction_save_version >= 105,
        "save schema predates match-statistics persistence"
    );
}

}  // namespace

int main() {
    try {
        transition_boundaries_update_authoritative_counters();
        combat_removal_updates_loss_and_defeat_counters();
        save106_round_trip_preserves_every_counter();
        std::cout << "All match statistics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

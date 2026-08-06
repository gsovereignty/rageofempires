#include "aoe/building_placement.hpp"

#include <iostream>

namespace {
int failures{};
void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    const aoe::Unit builder = simulation.units().front();
    const auto valid = aoe::evaluate_building_placement(
        simulation, builder.id, aoe::BuildingKind::house, {3, 7}
    );
    expect(valid.valid, "near clear grass should be valid");
    const auto occupied = aoe::evaluate_building_placement(
        simulation, builder.id, aoe::BuildingKind::house, {2, 7}
    );
    expect(!occupied.valid && occupied.reason == "FOOTPRINT OCCUPIED",
           "unit overlap not rejected");
    const auto far = aoe::evaluate_building_placement(
        simulation, builder.id, aoe::BuildingKind::house, {8, 7}
    );
    expect(far.valid, "reachable remote building order rejected");
    const auto segment =
        aoe::deterministic_wall_segment({1, 1}, {5, 3});
    expect(
        segment.front() == aoe::TilePosition{1, 1} &&
        segment.back() == aoe::TilePosition{5, 3} &&
        segment == aoe::deterministic_wall_segment({1, 1}, {5, 3}),
        "wall segment not deterministic"
    );

    aoe::GameMap coast(8, 8);
    for (int y = 2; y <= 4; ++y) {
        for (int x = 2; x <= 4; ++x) {
            coast.set_terrain({x, y}, aoe::Terrain::water);
        }
    }
    coast.set_terrain({3, 3}, aoe::Terrain::shallows);
    coast.set_terrain({3, 1}, aoe::Terrain::beach);
    aoe::Simulation dock_simulation(std::move(coast));
    const auto dock_builder = dock_simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {3, 0}
    );
    dock_simulation.replace_state(
        dock_simulation.units(), dock_simulation.buildings(),
        {500, 500, 500, 500}, {500, 500, 500, 500}, 0
    );
    const auto dock = aoe::evaluate_building_placement(
        dock_simulation, dock_builder, aoe::BuildingKind::dock, {2, 2}
    );
    expect(
        dock.valid && dock.footprint.size() == 9,
        "Dock 45 shoreline footprint rejected"
    );

    aoe::GameMap old_anchor(8, 8);
    old_anchor.set_terrain({5, 3}, aoe::Terrain::water);
    aoe::Simulation old_anchor_simulation(std::move(old_anchor));
    const auto old_anchor_builder = old_anchor_simulation.add_unit(
        aoe::UnitKind::villager, aoe::Player::blue, {1, 1}
    );
    const auto old_dock = aoe::evaluate_building_placement(
        old_anchor_simulation,
        old_anchor_builder,
        aoe::BuildingKind::dock,
        {2, 2}
    );
    expect(
        !old_dock.valid && old_dock.reason == "INVALID DOCK SHORELINE",
        "old all-Grass Dock coast anchor still accepted"
    );
    if (failures == 0) std::cout << "building placement tests passed\n";
    return failures == 0 ? 0 : 1;
}

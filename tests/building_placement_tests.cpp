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
    if (failures == 0) std::cout << "building placement tests passed\n";
    return failures == 0 ? 0 : 1;
}

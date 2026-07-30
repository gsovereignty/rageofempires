#include "aoe/command_panel.hpp"

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
    const auto unit = simulation.units().front();
    simulation.select_units({unit.id}, aoe::Player::blue);
    auto panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(panel.title != "NO SELECTION", "selected unit title");
    expect(panel.hit_points > 0 && panel.maximum_hit_points > 0, "unit HP");
    expect(panel.commands.size() == 11, "unit command grid");
    expect(
        std::ranges::all_of(panel.commands, [](const auto& button) {
            return !button.proven_archive_icon_id.has_value();
        }),
        "unit action command icon must remain unproved"
    );

    const auto building = simulation.buildings().front();
    simulation.select_building_at(building.position, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(panel.title != "NO SELECTION", "selected building title");
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::train_unit &&
                button.unit == aoe::UnitKind::villager &&
                button.proven_archive_icon_id == 15;
        }),
        "town center exact villager icon absent"
    );
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::cancel_production &&
                !button.enabled;
        }),
        "empty queue cancel should be disabled"
    );
    if (failures == 0) std::cout << "command panel tests passed\n";
    return failures == 0 ? 0 : 1;
}

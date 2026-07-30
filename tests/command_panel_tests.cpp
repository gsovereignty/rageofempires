#include "aoe/command_panel.hpp"
#include "aoe/game_rules.hpp"

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
    const auto scout = std::ranges::find_if(
        simulation.units(),
        [](const aoe::Unit& unit) {
            return unit.owner == aoe::Player::blue &&
                unit.kind == aoe::UnitKind::scout_cavalry;
        }
    );
    expect(scout != simulation.units().end(), "demo scout absent");
    if (scout == simulation.units().end()) return 1;
    simulation.select_units({scout->id}, aoe::Player::blue);
    auto panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(panel.title != "NO SELECTION", "selected unit title");
    expect(panel.hit_points > 0 && panel.maximum_hit_points > 0, "unit HP");
    expect(!panel.commands.empty(), "unit command grid");
    expect(
        std::ranges::all_of(panel.commands, [](const auto& button) {
            return button.action_archive_icon_id.has_value() &&
                !button.tooltip.empty();
        }),
        "unit actions need archive icons and tooltips"
    );
    expect(
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::attack_ground;
        }),
        "scout must not expose attack ground"
    );
    expect(
        std::ranges::count_if(panel.commands, [](const auto& button) {
            return button.command >=
                    aoe::PanelCommand::stance_aggressive &&
                button.command <= aoe::PanelCommand::stance_no_attack;
        }) == 4,
        "scout needs four explicit stances"
    );
    expect(
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::formation_line ||
                button.command == aoe::PanelCommand::formation_box ||
                button.command == aoe::PanelCommand::formation_staggered ||
                button.command == aoe::PanelCommand::formation_flank;
        }),
        "single scout must not expose formations"
    );
    const auto second_military = std::ranges::find_if(
        simulation.units(),
        [&scout](const aoe::Unit& unit) {
            return unit.owner == aoe::Player::blue &&
                unit.id != scout->id &&
                aoe::rules_for(unit.kind).attack > 0;
        }
    );
    expect(
        second_military != simulation.units().end(),
        "second demo military unit absent"
    );
    if (second_military != simulation.units().end()) {
        simulation.select_units(
            {scout->id, second_military->id}, aoe::Player::blue);
        panel = aoe::build_selection_panel(
            simulation, aoe::Player::blue);
        expect(
            std::ranges::count_if(panel.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::formation_line ||
                    button.command == aoe::PanelCommand::formation_box ||
                    button.command ==
                        aoe::PanelCommand::formation_staggered ||
                    button.command == aoe::PanelCommand::formation_flank;
            }) == 4,
            "group needs line, box, staggered, and flank"
        );
    }

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

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

aoe::EntityId add_at_empty_tile(
    aoe::Simulation& simulation,
    aoe::UnitKind kind
) {
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const aoe::TilePosition tile{x, y};
            try {
                return simulation.add_unit(
                    kind, aoe::Player::blue, tile);
            } catch (const std::invalid_argument&) {
            }
        }
    }
    throw std::runtime_error("no empty walkable tile");
}

aoe::EntityId add_building_at_empty_tile(
    aoe::Simulation& simulation,
    aoe::BuildingKind kind
) {
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            try {
                return simulation.add_building(
                    kind, aoe::Player::blue, {x, y});
            } catch (const std::invalid_argument&) {
            }
        }
    }
    throw std::runtime_error("no empty building tile");
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
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.icon.has_value() &&
                button.icon->sheet == aoe::ui_icons::command_sheet &&
                button.icon->evidence == aoe::ui_icons::Evidence::unknown &&
                !button.procedural_icon_fallback;
        }),
        "mapped unit actions need archive candidates with unknown evidence"
    );
    expect(
        std::ranges::all_of(panel.commands, [](const auto& button) {
            return !button.tooltip.empty();
        }),
        "unit actions need tooltips"
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
        const auto forward_commands = panel.commands;
        auto formation_count = std::ranges::count_if(
            panel.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::formation_line ||
                    button.command == aoe::PanelCommand::formation_box ||
                    button.command ==
                        aoe::PanelCommand::formation_staggered ||
                    button.command == aoe::PanelCommand::formation_flank;
            });
        if (panel.page_count > 1) {
            const auto continuation = aoe::build_selection_panel(
                simulation,
                aoe::Player::blue,
                aoe::PanelPage::root,
                1
            );
            formation_count += std::ranges::count_if(
                continuation.commands, [](const auto& button) {
                    return button.command ==
                            aoe::PanelCommand::formation_line ||
                        button.command == aoe::PanelCommand::formation_box ||
                        button.command ==
                            aoe::PanelCommand::formation_staggered ||
                        button.command == aoe::PanelCommand::formation_flank;
                });
        }
        expect(
            formation_count == 4,
            "group needs line, box, staggered, and flank"
        );
        simulation.select_units(
            {second_military->id, scout->id}, aoe::Player::blue);
        const auto reversed = aoe::build_selection_panel(
            simulation, aoe::Player::blue);
        expect(
            reversed.commands.size() == forward_commands.size() &&
                std::ranges::equal(
                    reversed.commands,
                    forward_commands,
                    [](const auto& left, const auto& right) {
                        return left.command == right.command &&
                            left.enabled == right.enabled &&
                            left.selected == right.selected;
                    }
                ),
            "selection order must not change command set"
        );
    }

    const auto building = simulation.buildings().front();
    simulation.select_building_at(building.position, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(panel.title != "NO SELECTION", "selected building title");
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::open_production;
        }),
        "town center production page entry absent"
    );
    panel = aoe::build_selection_panel(
        simulation,
        aoe::Player::blue,
        aoe::PanelPage::production
    );
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::train_unit &&
                button.unit == aoe::UnitKind::villager &&
                button.icon ==
                    aoe::ui_icons::training_unit(
                        aoe::UnitKind::villager);
        }),
        "town center exact villager icon absent"
    );
    expect(panel.commands.size() <= 15, "production page overflow");
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::cancel_production &&
                !button.enabled;
        }),
        "empty queue cancel should be disabled"
    );
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::open_research;
        }),
        "town center research page entry absent"
    );
    const auto research_panel = aoe::build_selection_panel(
        simulation,
        aoe::Player::blue,
        aoe::PanelPage::research
    );
    expect(
        research_panel.commands.size() <= 15 &&
            std::ranges::any_of(
                research_panel.commands,
                [](const auto& button) {
                    return button.command == aoe::PanelCommand::research &&
                        button.technology.has_value();
                }
            ),
        "town center research roster absent or overflowed"
    );
    expect(
        std::ranges::any_of(
            research_panel.commands,
            [](const auto& button) {
                return button.command == aoe::PanelCommand::research &&
                    button.icon &&
                    button.icon->sheet ==
                        aoe::ui_icons::technology_sheet &&
                    button.icon->evidence ==
                        aoe::ui_icons::Evidence::
                            exact_executable_dispatch;
            }
        ),
        "research technology exact DAT icon absent"
    );

    const aoe::TilePosition wall_position{1, 1};
    simulation.add_building(
        aoe::BuildingKind::stone_wall,
        aoe::Player::blue,
        wall_position
    );
    expect(
        simulation.select_building_at(
            wall_position, aoe::Player::blue),
        "wall selection"
    );
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::rally ||
                button.command == aoe::PanelCommand::ungarrison ||
                button.command == aoe::PanelCommand::cancel_production;
        }),
        "wall must not expose production or garrison commands"
    );

    const auto has_command = [](const aoe::SelectionPanelModel& model,
                                aoe::PanelCommand command) {
        return std::ranges::any_of(
            model.commands,
            [command](const auto& button) {
                return button.command == command;
            }
        );
    };
    const auto monk_id = simulation.add_unit(
        aoe::UnitKind::monk, aoe::Player::blue, {2, 2});
    simulation.select_units({monk_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::convert) &&
            has_command(panel, aoe::PanelCommand::heal) &&
            has_command(panel, aoe::PanelCommand::collect_relic) &&
            !has_command(panel, aoe::PanelCommand::deposit_relic),
        "normal monk command set"
    );

    const auto cart_id = simulation.add_unit(
        aoe::UnitKind::trade_cart, aoe::Player::blue, {3, 3});
    simulation.select_units({cart_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::trade_route),
        "trade route command absent"
    );

    const auto trebuchet_id = simulation.add_unit(
        aoe::UnitKind::trebuchet, aoe::Player::blue, {4, 4});
    simulation.select_units({trebuchet_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::pack_trebuchet) &&
            !has_command(panel, aoe::PanelCommand::unpack_trebuchet),
        "deployed trebuchet needs pack only"
    );
    const auto packed_id = simulation.add_unit(
        aoe::UnitKind::packed_trebuchet, aoe::Player::blue, {5, 5});
    simulation.select_units({packed_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::unpack_trebuchet) &&
            !has_command(panel, aoe::PanelCommand::pack_trebuchet),
        "packed trebuchet needs unpack only"
    );

    const auto villager_id = add_at_empty_tile(
        simulation, aoe::UnitKind::villager);
    simulation.select_units({villager_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(
            panel, aoe::PanelCommand::open_economic_buildings) &&
            has_command(
                panel, aoe::PanelCommand::open_military_buildings) &&
            has_command(panel, aoe::PanelCommand::repair),
        "villager build-page entry points absent"
    );
    panel = aoe::build_selection_panel(
        simulation,
        aoe::Player::blue,
        aoe::PanelPage::economic_buildings
    );
    expect(
        panel.commands.size() <= 15 &&
            std::ranges::any_of(panel.commands, [](const auto& button) {
                return button.command ==
                        aoe::PanelCommand::construct_building &&
                    button.building == aoe::BuildingKind::house;
            }) &&
            has_command(panel, aoe::PanelCommand::back),
        "economic build page contract"
    );

    simulation.replace_ages(aoe::Age::feudal, aoe::Age::dark);
    const auto fishing_id = add_at_empty_tile(
        simulation, aoe::UnitKind::fishing_ship);
    simulation.select_units({fishing_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command ==
                    aoe::PanelCommand::construct_building &&
                button.building == aoe::BuildingKind::fish_trap;
        }),
        "fishing ship Fish Trap command absent"
    );

    const auto transport_id = add_at_empty_tile(
        simulation, aoe::UnitKind::transport_ship);
    simulation.select_units({transport_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::disembark &&
                !button.enabled;
        }),
        "empty transport unload must be disabled"
    );
    const auto transport = std::ranges::find(
        simulation.units(), transport_id, &aoe::Unit::id);
    expect(transport != simulation.units().end(), "transport lookup");
    aoe::EntityId passenger_id{};
    if (transport != simulation.units().end()) {
        constexpr aoe::TilePosition offsets[] = {
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        };
        for (const auto offset : offsets) {
            try {
                passenger_id = simulation.add_unit(
                    aoe::UnitKind::villager,
                    aoe::Player::blue,
                    {
                        transport->position.x + offset.x,
                        transport->position.y + offset.y,
                    }
                );
                break;
            } catch (const std::invalid_argument&) {
            }
        }
    }
    expect(
        passenger_id != 0 &&
            simulation.command_embark(passenger_id, transport_id),
        "transport passenger setup"
    );
    simulation.select_units({transport_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::disembark &&
                button.enabled;
        }),
        "loaded transport unload must be enabled"
    );

    std::vector<aoe::EntityId> broad_selection{
        scout->id, monk_id, cart_id, trebuchet_id, packed_id, villager_id,
        fishing_id, transport_id,
    };
    if (second_military != simulation.units().end()) {
        broad_selection.push_back(second_military->id);
    }
    simulation.select_units(broad_selection, aoe::Player::blue);
    const auto first_page = aoe::build_selection_panel(
        simulation, aoe::Player::blue, aoe::PanelPage::root, 0);
    expect(
        first_page.page_count > 1 &&
            first_page.commands.size() <= 15 &&
            has_command(first_page, aoe::PanelCommand::next_page),
        "overflowing mixed selection needs bounded first page"
    );
    const auto last_page = aoe::build_selection_panel(
        simulation,
        aoe::Player::blue,
        aoe::PanelPage::root,
        first_page.page_count - 1
    );
    expect(
        last_page.page_index + 1 == last_page.page_count &&
            last_page.commands.size() <= 15 &&
            has_command(last_page, aoe::PanelCommand::previous_page),
        "overflowing mixed selection needs reachable last page"
    );

    for (std::size_t value = 0; value < aoe::unit_kind_count; ++value) {
        aoe::Simulation unit_matrix = aoe::Simulation::create_demo();
        const auto kind = static_cast<aoe::UnitKind>(value);
        const aoe::EntityId id = add_at_empty_tile(unit_matrix, kind);
        expect(
            unit_matrix.select_units({id}, aoe::Player::blue),
            "unit-kind matrix selection"
        );
        const auto model = aoe::build_selection_panel(
            unit_matrix, aoe::Player::blue);
        expect(
            model.title != "NO SELECTION" && model.commands.size() <= 15,
            "unit-kind matrix panel contract"
        );
    }

    for (std::size_t value = 0; value < aoe::building_kind_count; ++value) {
        aoe::Simulation building_matrix = aoe::Simulation::create_demo();
        const auto kind = static_cast<aoe::BuildingKind>(value);
        const aoe::EntityId id =
            add_building_at_empty_tile(building_matrix, kind);
        const auto found = std::ranges::find(
            building_matrix.buildings(), id, &aoe::Building::id);
        expect(
            found != building_matrix.buildings().end() &&
                building_matrix.select_building_at(
                    found->position, aoe::Player::blue),
            "building-kind matrix selection"
        );
        const auto model = aoe::build_selection_panel(
            building_matrix, aoe::Player::blue);
        expect(
            model.title != "NO SELECTION" && model.commands.size() <= 15,
            "building-kind matrix panel contract"
        );
    }

    if (failures == 0) std::cout << "command panel tests passed\n";
    return failures == 0 ? 0 : 1;
}

#include "aoe/command_panel.hpp"
#include "aoe/game_rules.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <span>
#include <utility>

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

aoe::Simulation make_building_matrix(aoe::BuildingKind kind) {
    aoe::GameMap map(16, 12);
    if (kind == aoe::BuildingKind::dock) {
        for (int y = 4; y < 7; ++y) {
            for (int x = 4; x < 7; ++x) {
                map.set_terrain({x, y}, aoe::Terrain::water);
            }
        }
        for (int x = 4; x < 7; ++x) {
            map.set_terrain({x, 3}, aoe::Terrain::beach);
        }
    } else if (kind == aoe::BuildingKind::fish_trap) {
        map.set_terrain({4, 4}, aoe::Terrain::water);
    }
    return aoe::Simulation(std::move(map));
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
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::stop;
        }),
        "idle scout must not expose a stop tile"
    );
    constexpr std::array scout_order{
        aoe::PanelCommand::stance_aggressive,
        aoe::PanelCommand::attack_move,
        aoe::PanelCommand::stance_stand_ground,
        aoe::PanelCommand::stance_no_attack,
        aoe::PanelCommand::garrison,
        aoe::PanelCommand::stance_defensive,
        aoe::PanelCommand::guard,
        aoe::PanelCommand::follow,
        aoe::PanelCommand::patrol,
    };
    expect(
        panel.commands.size() >= scout_order.size() &&
            std::ranges::equal(
                scout_order,
                std::span{
                    panel.commands.data(),
                    scout_order.size()
                },
                {},
                std::identity{},
                &aoe::CommandButtonModel::command
            ),
        "idle scout command order must match original HUD capture"
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
    const aoe::EntityId scout_id = scout->id;
    const aoe::EntityId second_military_id = add_at_empty_tile(
        simulation, aoe::UnitKind::militia);
    {
        simulation.select_units(
            {scout_id, second_military_id}, aoe::Player::blue);
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
            {second_military_id, scout_id}, aoe::Player::blue);
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
            return button.command == aoe::PanelCommand::train_unit &&
                button.unit == aoe::UnitKind::villager &&
                button.grid_slot == 0;
        }),
        "town center villager must occupy DAT button 1"
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
            return button.command == aoe::PanelCommand::ungarrison &&
                button.grid_slot == 4 &&
                button.icon && button.icon->frame == 45;
        }),
        "town center ungarrison must occupy original top-right slot"
    );
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::town_bell &&
                button.enabled && button.grid_slot == 14 &&
                button.icon && button.icon->frame == 49;
        }),
        "town center bell must occupy original bottom-right slot"
    );
    expect(
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::rally ||
                button.command == aoe::PanelCommand::delete_entity;
        }),
        "town center must not expose non-original root substitutes"
    );
    expect(
        simulation.command_town_bell(*simulation.selected_building()),
        "Town Bell activation rejected"
    );
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        panel.status == "TOWN BELL ACTIVE" &&
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::town_bell &&
                button.selected && button.grid_slot == 14 &&
                button.icon && button.icon->frame == 61 &&
                button.label == "SEND VILLAGERS BACK TO WORK";
        }),
        "active Town Bell must expose original frame 61 recall state"
    );
    expect(
        simulation.command_town_bell(*simulation.selected_building()),
        "Town Bell recall rejected"
    );
    expect(
        std::ranges::any_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::research &&
                button.technology.has_value() &&
                (button.grid_slot == 5 ||
                 button.grid_slot == 6 ||
                 button.grid_slot == 7);
        }),
        "town center direct DAT research slots absent"
    );
    expect(
        std::ranges::none_of(panel.commands, [](const auto& button) {
            return button.command == aoe::PanelCommand::open_production ||
                button.command == aoe::PanelCommand::open_research;
        }),
        "town center must not hide original commands behind submenus"
    );
    if (simulation.age(aoe::Player::blue) != aoe::Age::imperial) {
        expect(
            std::ranges::any_of(panel.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::advance_age &&
                    button.grid_slot == 10 &&
                    button.icon &&
                    button.icon->sheet ==
                        aoe::ui_icons::technology_sheet;
            }),
            "town center age-up must occupy DAT button 11"
        );
    }

    const auto villager = std::ranges::find_if(
        simulation.units(), [](const aoe::Unit& unit) {
            return unit.owner == aoe::Player::blue &&
                unit.kind == aoe::UnitKind::villager;
        });
    expect(villager != simulation.units().end(), "demo villager absent");
    if (villager != simulation.units().end()) {
        simulation.select_units({villager->id}, aoe::Player::blue);
        const auto villager_panel = aoe::build_selection_panel(
            simulation, aoe::Player::blue);
        expect(
            std::ranges::none_of(
                villager_panel.commands, [](const auto& button) {
                    return button.command == aoe::PanelCommand::attack_move ||
                        button.command == aoe::PanelCommand::patrol ||
                        button.command == aoe::PanelCommand::guard ||
                        (button.command >=
                            aoe::PanelCommand::stance_aggressive &&
                         button.command <=
                            aoe::PanelCommand::stance_no_attack);
                }),
            "villager must not expose military-only command tiles"
        );
        const auto stop = std::ranges::find_if(
            villager_panel.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::stop;
            });
        expect(
            stop != villager_panel.commands.end() && stop->icon &&
                stop->icon->frame == 3 && stop->grid_slot == 9,
            "villager stop must use installed stop artwork"
        );
        constexpr std::array villager_layout{
            std::pair{aoe::PanelCommand::open_economic_buildings, 0U},
            std::pair{aoe::PanelCommand::open_military_buildings, 1U},
            std::pair{aoe::PanelCommand::repair, 2U},
            std::pair{aoe::PanelCommand::delete_entity, 3U},
            std::pair{aoe::PanelCommand::garrison, 4U},
            std::pair{aoe::PanelCommand::stop, 9U},
        };
        for (const auto& [command, slot] : villager_layout) {
            const auto button = std::ranges::find(
                villager_panel.commands, command,
                &aoe::CommandButtonModel::command);
            expect(
                button != villager_panel.commands.end() &&
                    button->grid_slot == slot,
                "villager original sparse grid slot"
            );
        }
        simulation.select_building_at(
            building.position, aoe::Player::blue);
    }
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
    const auto has_valid_grid_slots =
        [](const aoe::SelectionPanelModel& model) {
            std::array<bool, 15> occupied{};
            for (const auto& button : model.commands) {
                if (button.grid_slot >= occupied.size() ||
                    occupied[button.grid_slot]) {
                    return false;
                }
                occupied[button.grid_slot] = true;
            }
            return true;
        };
    const auto monk_id = add_at_empty_tile(
        simulation, aoe::UnitKind::monk);
    simulation.select_units({monk_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::convert) &&
            has_command(panel, aoe::PanelCommand::heal) &&
            has_command(panel, aoe::PanelCommand::collect_relic) &&
            !has_command(panel, aoe::PanelCommand::deposit_relic),
        "normal monk command set"
    );

    const auto cart_id = add_at_empty_tile(
        simulation, aoe::UnitKind::trade_cart);
    simulation.select_units({cart_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::trade_route),
        "trade route command absent"
    );

    const auto trebuchet_id = add_at_empty_tile(
        simulation, aoe::UnitKind::trebuchet);
    simulation.select_units({trebuchet_id}, aoe::Player::blue);
    panel = aoe::build_selection_panel(simulation, aoe::Player::blue);
    expect(
        has_command(panel, aoe::PanelCommand::pack_trebuchet) &&
            !has_command(panel, aoe::PanelCommand::unpack_trebuchet),
        "deployed trebuchet needs pack only"
    );
    const auto packed_id = add_at_empty_tile(
        simulation, aoe::UnitKind::packed_trebuchet);
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
        scout_id, monk_id, cart_id, trebuchet_id, packed_id, villager_id,
        fishing_id, transport_id,
    };
    broad_selection.push_back(second_military_id);
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
            model.title != "NO SELECTION" && model.commands.size() <= 15 &&
                has_valid_grid_slots(model),
            "unit-kind matrix panel contract"
        );
    }

    for (aoe::BuildingKind kind : {
             aoe::BuildingKind::palisade_gate_x,
             aoe::BuildingKind::palisade_gate_y,
             aoe::BuildingKind::stone_gate_x,
             aoe::BuildingKind::stone_gate_y,
             aoe::BuildingKind::fortified_gate_x,
             aoe::BuildingKind::fortified_gate_y,
         }) {
        aoe::Simulation gate_panel(aoe::GameMap(16, 12));
        const aoe::EntityId gate = gate_panel.add_building(
            kind, aoe::Player::blue, {3, 3}
        );
        expect(
            gate_panel.select_building_at({3, 3}, aoe::Player::blue),
            "gate selection"
        );
        auto model = aoe::build_selection_panel(
            gate_panel, aoe::Player::blue
        );
        expect(
            std::ranges::any_of(model.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::lock_gate &&
                    button.label == "LOCK GATE" && button.hotkey == "L" &&
                    !button.selected;
            }),
            "unlocked gate needs Lock Gate button and hotkey"
        );
        expect(gate_panel.set_gate_locked(gate, true), "gate lock action");
        model = aoe::build_selection_panel(
            gate_panel, aoe::Player::blue
        );
        expect(
            model.status == "GATE LOCKED" &&
            std::ranges::any_of(model.commands, [](const auto& button) {
                return button.command == aoe::PanelCommand::unlock_gate &&
                    button.label == "UNLOCK GATE" &&
                    button.hotkey == "L" && button.selected;
            }),
            "locked gate needs Unlock Gate button and hotkey"
        );
    }

    if (std::getenv("AOE_GATE_PANEL_TEST") != nullptr) {
        return failures == 0 ? 0 : 1;
    }

    for (std::size_t value = 0; value < aoe::building_kind_count; ++value) {
        const auto kind = static_cast<aoe::BuildingKind>(value);
        aoe::Simulation building_matrix = make_building_matrix(kind);
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
            model.title != "NO SELECTION" && model.commands.size() <= 15 &&
                has_valid_grid_slots(model),
            "building-kind matrix panel contract"
        );
    }

    if (failures == 0) std::cout << "command panel tests passed\n";
    return failures == 0 ? 0 : 1;
}

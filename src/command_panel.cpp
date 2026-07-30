#include "aoe/command_panel.hpp"

#include <algorithm>
#include <array>
#include <string_view>

#include "aoe/game_rules.hpp"
#include "aoe/ui_icon_contract.hpp"

namespace aoe {
namespace {

void add(
    SelectionPanelModel& panel,
    PanelCommand command,
    std::string label,
    std::string hotkey,
    std::string tooltip,
    bool enabled = true,
    std::optional<UnitKind> unit = std::nullopt,
    bool selected = false,
    std::optional<std::int32_t> action_icon = std::nullopt
) {
    std::optional<ui_icons::Binding> exact_icon;
    if (unit) {
        exact_icon = ui_icons::training_unit(*unit);
    } else if (action_icon) {
        exact_icon = ui_icons::Binding{
            ui_icons::command_sheet,
            *action_icon,
            ui_icons::Evidence::unknown,
        };
    }
    panel.commands.push_back({
        command, std::move(label), std::move(hotkey), std::move(tooltip),
        enabled, selected,
        unit, std::nullopt, std::nullopt, exact_icon,
        !exact_icon.has_value(),
    });
}

void add_building(
    SelectionPanelModel& panel,
    BuildingKind building,
    std::string hotkey
) {
    panel.commands.push_back({
        PanelCommand::construct_building,
        std::string{name(building)},
        std::move(hotkey),
        "Place " + std::string{name(building)} + ".",
        true,
        false,
        std::nullopt,
        std::nullopt,
        building,
        std::nullopt,
        true,
    });
}

void add_technology(
    SelectionPanelModel& panel,
    Technology technology,
    bool enabled
) {
    const auto icon = ui_icons::technology_icon(technology);
    panel.commands.push_back({
        PanelCommand::research,
        std::string{name(technology)},
        "",
        "Research " + std::string{name(technology)} + ".",
        enabled,
        false,
        std::nullopt,
        technology,
        std::nullopt,
        icon,
        !icon.has_value(),
    });
}

void paginate_root_commands(
    SelectionPanelModel& panel,
    std::size_t requested_page
) {
    constexpr std::size_t page_capacity = 13;
    constexpr std::size_t grid_capacity = 15;
    if (panel.commands.size() <= grid_capacity) return;

    const std::vector<CommandButtonModel> commands = std::move(panel.commands);
    panel.page_count =
        (commands.size() + page_capacity - 1) / page_capacity;
    panel.page_index = std::min(requested_page, panel.page_count - 1);
    const std::size_t first = panel.page_index * page_capacity;
    const std::size_t last =
        std::min(first + page_capacity, commands.size());
    panel.commands.assign(commands.begin() + first, commands.begin() + last);
    if (panel.page_index > 0) {
        add(panel, PanelCommand::previous_page,
            "PREVIOUS", "<", "Previous command page.");
    }
    if (panel.page_index + 1 < panel.page_count) {
        add(panel, PanelCommand::next_page,
            "NEXT", ">", "Next command page.");
    }
}

std::string page_slot_hotkey(std::size_t slot) {
    static constexpr std::array<std::string_view, 12> hotkeys{
        "1", "2", "3", "4", "5", "6",
        "7", "8", "9", "0", "-", "=",
    };
    return slot < hotkeys.size() ? std::string{hotkeys[slot]} : "";
}

std::vector<UnitKind> production_roster(BuildingKind building) {
    switch (building) {
        case BuildingKind::town_center:
            return {UnitKind::villager};
        case BuildingKind::barracks:
            return {
                UnitKind::militia,
                UnitKind::spearman,
                UnitKind::eagle_warrior,
                UnitKind::huskarl,
            };
        case BuildingKind::archery_range:
            return {
                UnitKind::archer,
                UnitKind::skirmisher,
                UnitKind::cavalry_archer,
                UnitKind::hand_cannoneer,
            };
        case BuildingKind::stable:
            return {
                UnitKind::scout_cavalry,
                UnitKind::knight,
                UnitKind::camel_rider,
            };
        case BuildingKind::siege_workshop:
            return {
                UnitKind::battering_ram,
                UnitKind::mangonel,
                UnitKind::scorpion,
                UnitKind::bombard_cannon,
            };
        case BuildingKind::monastery:
            return {UnitKind::monk, UnitKind::missionary};
        case BuildingKind::market:
            return {UnitKind::trade_cart};
        case BuildingKind::dock:
            return {
                UnitKind::fishing_ship,
                UnitKind::galley,
                UnitKind::transport_ship,
                UnitKind::fire_ship,
                UnitKind::demolition_ship,
                UnitKind::cannon_galleon,
                UnitKind::longboat,
                UnitKind::turtle_ship,
                UnitKind::trade_cog,
            };
        case BuildingKind::castle:
            return {
                UnitKind::longbowman,
                UnitKind::throwing_axeman,
                UnitKind::huskarl,
                UnitKind::teutonic_knight,
                UnitKind::samurai,
                UnitKind::chu_ko_nu,
                UnitKind::cataphract,
                UnitKind::war_elephant,
                UnitKind::mameluke,
                UnitKind::janissary,
                UnitKind::berserk,
                UnitKind::mangudai,
                UnitKind::jaguar_warrior,
                UnitKind::plumed_archer,
                UnitKind::conquistador,
                UnitKind::tarkan,
                UnitKind::woad_raider,
                UnitKind::petard,
                UnitKind::trebuchet,
            };
        default:
            return {};
    }
}

bool hidden_technology_record(Technology technology) {
    switch (technology) {
        case Technology::longboat:
        case Technology::turtle_ship:
        case Technology::longbowman:
        case Technology::throwing_axeman:
        case Technology::huskarl:
        case Technology::teutonic_knight:
        case Technology::samurai:
        case Technology::chu_ko_nu:
        case Technology::cataphract:
        case Technology::war_elephant:
        case Technology::mameluke:
        case Technology::janissary:
        case Technology::berserk:
        case Technology::mangudai:
        case Technology::jaguar_warrior:
        case Technology::plumed_archer:
        case Technology::conquistador:
        case Technology::tarkan:
        case Technology::woad_raider:
        case Technology::hand_cannoneer_gate:
        case Technology::bombard_cannon_gate:
        case Technology::petard_gate:
            return true;
        default:
            return false;
    }
}

bool supports_attack_ground(UnitKind kind) {
    return kind == UnitKind::mangonel ||
        kind == UnitKind::onager ||
        kind == UnitKind::siege_onager ||
        kind == UnitKind::bombard_cannon;
}

bool ram(UnitKind kind) {
    return kind == UnitKind::battering_ram ||
        kind == UnitKind::capped_ram ||
        kind == UnitKind::siege_ram;
}

bool supports_military_orders(UnitKind kind) {
    return rules_for(kind).attack > 0 &&
        kind != UnitKind::trebuchet &&
        kind != UnitKind::packed_trebuchet;
}

bool supports_stance(UnitKind kind) {
    return supports_military_orders(kind) && !is_ship(kind) &&
        !ram(kind);
}

bool supports_garrison(UnitKind kind) {
    return !is_ship(kind) && !ram(kind) &&
        kind != UnitKind::mangonel &&
        kind != UnitKind::onager &&
        kind != UnitKind::siege_onager &&
        kind != UnitKind::scorpion &&
        kind != UnitKind::heavy_scorpion &&
        kind != UnitKind::packed_trebuchet &&
        kind != UnitKind::trebuchet &&
        kind != UnitKind::bombard_cannon &&
        kind != UnitKind::trade_cart &&
        !is_herdable(kind) &&
        !is_huntable(kind) &&
        !is_relic(kind);
}

bool supports_production(BuildingKind kind) {
    for (int value = 0;
         value <= static_cast<int>(UnitKind::elite_woad_raider);
         ++value) {
        if (can_train(kind, static_cast<UnitKind>(value))) return true;
    }
    return false;
}

bool supports_rally(BuildingKind kind) {
    return supports_production(kind);
}

bool supports_garrison(BuildingKind kind) {
    return kind == BuildingKind::town_center ||
        kind == BuildingKind::castle ||
        kind == BuildingKind::watch_tower ||
        kind == BuildingKind::bombard_tower;
}

std::string activity_status(
    const Simulation& simulation,
    const Unit& unit
) {
    if (unit.garrisoned_in != 0) return "GARRISONED";
    if (unit.trebuchet_transform_ticks_remaining > 0) {
        return unit.trebuchet_transform_to_packed
            ? "PACKING" : "UNPACKING";
    }
    if (unit.conversion_target_id != 0) return "CONVERTING";
    if (unit.healing_target_id != 0) return "HEALING";
    if (unit.relic_deposit_target_id != 0) return "RETURNING RELIC";
    if (unit.relic_target_id != 0) return "COLLECTING RELIC";
    if (unit.carrying_relic) return "CARRYING RELIC";
    if (unit.repair_target_id != 0) return "REPAIRING";
    if (std::ranges::any_of(
            simulation.buildings(),
            [&unit](const Building& building) {
                return !building.completed() &&
                    std::ranges::find(
                        building.builder_ids, unit.id
                    ) != building.builder_ids.end();
            }
        )) {
        return "BUILDING";
    }
    if (unit.trade_home_market_id != 0 ||
        unit.trade_target_market_id != 0) {
        return unit.trade_waiting ? "WAITING TO TRADE" :
            unit.trade_returning ? "RETURNING TRADE" : "TRADING";
    }
    if (unit.kind == UnitKind::fishing_ship &&
        unit.has_resource_target) {
        return unit.returning_resource
            ? "RETURNING FISH" : "FISHING";
    }
    if (unit.returning_resource) return "RETURNING RESOURCE";
    if (unit.has_resource_target) return "GATHERING";
    if (unit.kind == UnitKind::transport_ship &&
        std::ranges::any_of(
            simulation.units(),
            [&unit](const Unit& passenger) {
                return passenger.garrisoned_in == unit.id;
            }
        )) {
        return "TRANSPORTING";
    }
    if (unit.garrison_target_id != 0) return "ENTERING GARRISON";
    if (unit.attacking_ground) return "ATTACKING GROUND";
    if (unit.attack_moving) return "ATTACK MOVE";
    if (unit.patrolling) return "PATROLLING";
    if (unit.guard_target_id != 0) return "GUARDING";
    if (unit.attack_target_id != 0) return "ATTACKING";
    if (unit.moving) return "MOVING";
    return "IDLE";
}

}  // namespace

SelectionPanelModel build_selection_panel(
    const Simulation& simulation,
    Player player,
    PanelPage page,
    std::size_t page_index
) {
    SelectionPanelModel panel;
    if (simulation.selected_unit()) {
        std::vector<const Unit*> selected_units;
        for (const EntityId id : simulation.selected_units()) {
            const auto selected = std::ranges::find(
                simulation.units(), id, &Unit::id
            );
            if (selected != simulation.units().end() &&
                selected->owner == player) {
                selected_units.push_back(&*selected);
            }
        }
        if (selected_units.empty()) return panel;
        std::ranges::sort(selected_units, {}, &Unit::id);
        const Unit& representative = *selected_units.front();
        panel.title = selected_units.size() == 1
            ? std::string{name(representative.kind)}
            : std::to_string(selected_units.size()) + " UNITS";
        panel.hit_points = representative.hit_points;
        panel.maximum_hit_points = rules_for(representative.kind).hit_points;
        panel.carried_resource = representative.carried_resource;
        panel.carried_amount = representative.carried_amount;
        panel.status = activity_status(simulation, representative);
        const auto active = [](const Unit* unit) {
            return unit->garrisoned_in == 0;
        };
        const auto any = [&selected_units, &active](const auto capability) {
            return std::ranges::any_of(
                selected_units,
                [&active, &capability](const Unit* unit) {
                    return active(unit) && capability(unit->kind);
                }
            );
        };
        const auto any_state = [&selected_units, &active](const auto state) {
            return std::ranges::any_of(
                selected_units,
                [&active, &state](const Unit* unit) {
                    return active(unit) && state(*unit);
                }
            );
        };
        const bool any_active =
            std::ranges::any_of(selected_units, active);
        if (!any_active) {
            panel.status = "GARRISONED";
            return panel;
        }
        const bool any_villager = any([](UnitKind kind) {
            return kind == UnitKind::villager;
        });
        const bool any_fishing_ship = any([](UnitKind kind) {
            return kind == UnitKind::fishing_ship;
        });
        const auto building_available = [&simulation, player](
                                            BuildingKind kind) {
            return civilization_has_building(
                       simulation.civilization(player), kind
                   ) &&
                simulation.age(player) >= rules_for(kind).minimum_age;
        };
        if (any_villager && page != PanelPage::root) {
            const auto add_available = [
                &panel, &building_available
            ](BuildingKind kind, std::string hotkey) {
                if (building_available(kind)) {
                    add_building(panel, kind, std::move(hotkey));
                }
            };
            if (page == PanelPage::economic_buildings) {
                add_available(BuildingKind::house, "H");
                add_available(BuildingKind::mill, "M");
                add_available(BuildingKind::lumber_camp, "L");
                add_available(BuildingKind::mining_camp, "N");
                add_available(BuildingKind::farm, "F");
                add_available(BuildingKind::market, "K");
                add_available(BuildingKind::monastery, "Y");
                add_available(BuildingKind::town_center, "C");
                add_available(BuildingKind::wonder, "W");
            } else if (page == PanelPage::military_buildings) {
                add_available(BuildingKind::barracks, "B");
                add_available(BuildingKind::archery_range, "A");
                add_available(BuildingKind::stable, "S");
                add_available(BuildingKind::blacksmith, "K");
                add_available(BuildingKind::university, "U");
                add_available(BuildingKind::siege_workshop, "G");
                add_available(BuildingKind::castle, "C");
                add(panel, PanelCommand::open_defensive_buildings,
                    "DEFENSES", "D", "Open walls, gates, and towers.");
            } else {
                add_available(BuildingKind::palisade_wall, "P");
                add_available(BuildingKind::palisade_gate_x, "1");
                add_available(BuildingKind::palisade_gate_y, "2");
                add_available(BuildingKind::stone_wall, "W");
                add_available(BuildingKind::stone_gate_x, "3");
                add_available(BuildingKind::stone_gate_y, "4");
                add_available(BuildingKind::watch_tower, "T");
                add_available(BuildingKind::bombard_tower, "B");
                add_available(BuildingKind::outpost, "O");
            }
            add(panel, PanelCommand::back, "BACK", "ESC",
                "Return to unit commands.");
            return panel;
        }
        // btncmd (50721) is an action-icon sheet. Candidate semantic frame
        // mappings remain explicitly unknown; missing or undecodable frames
        // retain the procedural/text fallback.
        add(panel, PanelCommand::stop, "STOP", "S",
            "Stop all current orders.", true, std::nullopt, false, 4);
        if (any(supports_military_orders)) {
            add(panel, PanelCommand::attack_move, "ATTACK MOVE", "A",
                "Move to a location and attack enemies along the way.",
                true, std::nullopt,
                any_state([](const Unit& unit) {
                    return unit.attack_moving;
                }), 0);
            add(panel, PanelCommand::patrol, "PATROL", "P",
                "Patrol between the current position and a destination.",
                true, std::nullopt,
                any_state([](const Unit& unit) {
                    return unit.patrolling;
                }), 1);
            add(panel, PanelCommand::guard, "GUARD", "G",
                "Follow and protect a friendly unit or building.",
                true, std::nullopt,
                any_state([](const Unit& unit) {
                    return unit.guard_target_id != 0;
                }), 2);
        }
        if (any(supports_attack_ground)) {
            add(panel, PanelCommand::attack_ground, "ATTACK GROUND", "X",
                "Fire at a chosen ground location.",
                true, std::nullopt,
                any_state([](const Unit& unit) {
                    return unit.attacking_ground;
                }), 12);
        }
        if (any([](UnitKind kind) {
                return supports_garrison(kind);
            })) {
            add(panel, PanelCommand::garrison, "GARRISON", "H",
                "Enter a compatible friendly building.", true,
                std::nullopt, false, 3);
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::monk ||
                    kind == UnitKind::missionary;
            })) {
            add(panel, PanelCommand::convert, "CONVERT", "C",
                "Convert a target enemy unit.", true);
            add(panel, PanelCommand::heal, "HEAL", "E",
                "Heal a wounded friendly unit.", true);
            if (any_state([](const Unit& unit) {
                    return (unit.kind == UnitKind::monk ||
                            unit.kind == UnitKind::missionary) &&
                        unit.carrying_relic;
                })) {
                add(panel, PanelCommand::deposit_relic, "RETURN RELIC", "R",
                    "Deliver a carried relic to a friendly monastery.", true);
            }
            if (any_state([](const Unit& unit) {
                    return (unit.kind == UnitKind::monk ||
                            unit.kind == UnitKind::missionary) &&
                        !unit.carrying_relic;
                })) {
                add(panel, PanelCommand::collect_relic, "PICK UP RELIC", "R",
                    "Collect a relic from the map.", true);
            }
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::villager;
            })) {
            add(panel, PanelCommand::repair, "REPAIR", "R",
                "Repair a damaged friendly building.", true);
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::trade_cart ||
                    kind == UnitKind::trade_cog;
            })) {
            add(panel, PanelCommand::trade_route, "TRADE", "T",
                "Set a compatible allied trade endpoint.", true);
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::trebuchet;
            })) {
            add(panel, PanelCommand::pack_trebuchet, "PACK", "P",
                "Pack selected deployed trebuchets.", true);
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::packed_trebuchet;
            })) {
            add(panel, PanelCommand::unpack_trebuchet, "UNPACK", "P",
                "Deploy selected packed trebuchets.", true);
        }
        if (any_villager) {
            add(panel, PanelCommand::open_economic_buildings,
                "ECONOMIC", "E", "Open economic building commands.");
            add(panel, PanelCommand::open_military_buildings,
                "MILITARY", "V", "Open military building commands.");
        }
        if (any_fishing_ship &&
            building_available(BuildingKind::fish_trap)) {
            add_building(panel, BuildingKind::fish_trap, "F");
        }
        if (any([](UnitKind kind) {
                return !is_ship(kind);
            })) {
            add(panel, PanelCommand::embark, "ENTER TRANSPORT", "I",
                "Enter a nearby compatible friendly transport ship.", true);
        }
        if (any([](UnitKind kind) {
                return kind == UnitKind::transport_ship;
            })) {
            const bool has_passengers = std::ranges::any_of(
                simulation.units(),
                [&selected_units](const Unit& passenger) {
                    return std::ranges::any_of(
                        selected_units,
                        [&passenger](const Unit* transport) {
                            return transport->kind ==
                                       UnitKind::transport_ship &&
                                passenger.garrisoned_in == transport->id;
                        }
                    );
                }
            );
            add(panel, PanelCommand::disembark, "UNLOAD", "U",
                "Unload all passengers at a valid shore tile.",
                has_passengers);
        }
        if (any(supports_stance)) {
            add(panel, PanelCommand::stance_aggressive, "AGGRESSIVE", "O",
                "Pursue enemies at full sight range.", true, std::nullopt,
                any_state([](const Unit& unit) {
                    return supports_stance(unit.kind) &&
                        unit.stance == UnitStance::aggressive;
                }), 5);
            add(panel, PanelCommand::stance_defensive, "DEFENSIVE", "D",
                "Pursue nearby enemies, then return.", true, std::nullopt,
                any_state([](const Unit& unit) {
                    return supports_stance(unit.kind) &&
                        unit.stance == UnitStance::defensive;
                }), 6);
            add(panel, PanelCommand::stance_stand_ground, "STAND GROUND", "N",
                "Attack in range without moving.", true, std::nullopt,
                any_state([](const Unit& unit) {
                    return supports_stance(unit.kind) &&
                        unit.stance == UnitStance::stand_ground;
                }), 7);
            add(panel, PanelCommand::stance_no_attack, "NO ATTACK", "T",
                "Do not acquire or attack enemies.", true, std::nullopt,
                any_state([](const Unit& unit) {
                    return supports_stance(unit.kind) &&
                        unit.stance == UnitStance::passive;
                }), 8);
        }
        const auto military_count = std::ranges::count_if(
            selected_units,
            [&active](const Unit* unit) {
                return active(unit) && supports_military_orders(unit->kind);
            }
        );
        const bool group_formation = military_count > 1;
        if (group_formation) {
            const FormationKind formation = simulation.formation_kind(player);
            add(panel, PanelCommand::formation_line, "LINE", "L",
                "Arrange selected units in a line.", true, std::nullopt,
                formation == FormationKind::line, 9);
            add(panel, PanelCommand::formation_box, "BOX", "B",
                "Arrange selected units in a defensive box.", true,
                std::nullopt, formation == FormationKind::box, 10);
            add(panel, PanelCommand::formation_staggered, "STAGGERED", "C",
                "Spread selected units into a staggered formation.", true,
                std::nullopt, formation == FormationKind::staggered, 11);
            add(panel, PanelCommand::formation_flank, "FLANK", "F",
                "Split selected units into flanking groups.", true,
                std::nullopt, formation == FormationKind::flank, 13);
        }
        paginate_root_commands(panel, page_index);
        return panel;
    }
    if (simulation.selected_building()) {
        const auto selected = std::ranges::find_if(
            simulation.buildings(),
            [&simulation](const Building& building) {
                return building.id == *simulation.selected_building();
            }
        );
        if (selected == simulation.buildings().end()) return panel;
        panel.title = std::string{name(selected->kind)};
        panel.hit_points = selected->hit_points;
        panel.maximum_hit_points =
            rules_for(selected->kind).hit_points;
        panel.garrison_count = static_cast<int>(std::ranges::count_if(
            simulation.units(),
            [id = selected->id](const Unit& unit) {
                return unit.garrisoned_in == id;
            }
        ));
        if (!selected->completed()) {
            const int total =
                std::max(rules_for(selected->kind).construction_ticks, 1);
            panel.progress_percent = std::clamp(
                100 - selected->construction_ticks_remaining * 100 / total,
                0, 100
            );
            panel.status = "UNDER CONSTRUCTION";
        } else if (!selected->production_queue.empty()) {
            const ProductionOrder& order =
                selected->production_queue.front();
            const int total = std::max(rules_for(order.kind).training_ticks, 1);
            panel.progress_percent = std::clamp(
                100 - order.ticks_remaining * 100 / total, 0, 100
            );
            panel.status = "TRAINING " + std::string{name(order.kind)};
        } else if (selected->technology_research_ticks_remaining > 0) {
            panel.status = "RESEARCHING " +
                std::string{name(selected->technology_research_target)};
            const int total = std::max(
                rules_for(selected->technology_research_target).research_ticks,
                1
            );
            panel.progress_percent = std::clamp(
                100 - selected->technology_research_ticks_remaining *
                    100 / total,
                0, 100
            );
        } else if (selected->age_research_ticks_remaining > 0) {
            panel.status = "ADVANCING AGE";
            const int total = std::max(
                rules_for(selected->age_research_target).research_ticks, 1
            );
            panel.progress_percent = std::clamp(
                100 - selected->age_research_ticks_remaining * 100 / total,
                0, 100
            );
        } else {
            panel.status = "READY";
        }
        if (!selected->completed()) return panel;

        std::vector<UnitKind> units;
        for (const UnitKind kind : production_roster(selected->kind)) {
            const bool anarchy_huskarl =
                selected->kind == BuildingKind::barracks &&
                kind == UnitKind::huskarl &&
                simulation.civilization(player) == Civilization::goths &&
                simulation.has_technology(player, Technology::anarchy);
            if ((can_train(selected->kind, kind) || anarchy_huskarl) &&
                civilization_has_unit(
                    simulation.civilization(player), kind
                ) &&
                simulation.age(player) >= rules_for(kind).minimum_age) {
                units.push_back(kind);
            }
        }
        std::vector<Technology> technologies;
        for (std::size_t value = 0; value < technology_count; ++value) {
            const auto technology = static_cast<Technology>(value);
            const TechnologyRules& technology_rules =
                rules_for(technology);
            if (!hidden_technology_record(technology) &&
                technology_rules.researched_at == selected->kind &&
                civilization_has_technology(
                    simulation.civilization(player), technology
                ) &&
                simulation.age(player) >=
                    technology_rules.minimum_age &&
                !simulation.has_technology(player, technology)) {
                technologies.push_back(technology);
            }
        }
        constexpr std::size_t page_capacity = 12;
        const auto add_navigation = [
            &panel, page_index
        ](std::size_t count) {
            panel.page_count = std::max<std::size_t>(
                1, (count + page_capacity - 1) / page_capacity);
            panel.page_index = std::min(
                page_index, panel.page_count - 1);
            if (panel.page_index > 0) {
                add(panel, PanelCommand::previous_page,
                    "PREVIOUS", "<", "Previous command page.");
            }
            if (panel.page_index + 1 < panel.page_count) {
                add(panel, PanelCommand::next_page,
                    "NEXT", ">", "Next command page.");
            }
            add(panel, PanelCommand::back, "BACK", "ESC",
                "Return to building commands.");
        };
        if (page == PanelPage::production) {
            const std::size_t first =
                std::min(page_index * page_capacity, units.size());
            const std::size_t last =
                std::min(first + page_capacity, units.size());
            for (std::size_t index = first; index < last; ++index) {
                const UnitKind kind = units[index];
                Simulation probe = simulation;
                const bool enabled =
                    probe.queue_unit_at(selected->id, kind);
                add(panel, PanelCommand::train_unit,
                    std::string{name(kind)}, page_slot_hotkey(index - first),
                    "Train " + std::string{name(kind)} + ".",
                    enabled, kind);
            }
            add_navigation(units.size());
            return panel;
        }
        if (page == PanelPage::research) {
            const std::size_t first =
                std::min(page_index * page_capacity, technologies.size());
            const std::size_t last =
                std::min(first + page_capacity, technologies.size());
            for (std::size_t index = first; index < last; ++index) {
                const Technology technology = technologies[index];
                Simulation probe = simulation;
                const bool enabled = probe.research_technology_at(
                    selected->id, technology);
                add_technology(panel, technology, enabled);
                panel.commands.back().hotkey =
                    page_slot_hotkey(index - first);
            }
            add_navigation(technologies.size());
            return panel;
        }

        if (supports_rally(selected->kind)) {
            add(panel, PanelCommand::rally, "RALLY POINT", "R",
                "Set destination for newly trained units.", true,
                std::nullopt, false, 14);
        }
        if (supports_garrison(selected->kind)) {
            add(panel, PanelCommand::ungarrison, "UNGARRISON", "U",
                "Release all units garrisoned in this building.",
                panel.garrison_count > 0, std::nullopt, false, 15);
        }
        if (supports_production(selected->kind)) {
            add(panel, PanelCommand::cancel_production, "CANCEL LAST", "Q",
                "Cancel last queued unit and refund its cost.",
                !selected->production_queue.empty(), std::nullopt, false, 16);
        }
        if (!units.empty()) {
            add(panel, PanelCommand::open_production,
                "PRODUCTION", "P", "Open unit production.");
        }
        if (!technologies.empty()) {
            add(panel, PanelCommand::open_research,
                "RESEARCH", "T", "Open available technologies.");
        }
    }
    return panel;
}

}  // namespace aoe

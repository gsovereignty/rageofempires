#include "aoe/command_panel.hpp"

#include <algorithm>

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
    if (panel.commands.size() >= 15) return;
    std::optional<std::int32_t> exact_icon = action_icon;
    if (unit) {
        if (const auto binding = ui_icons::training_unit(*unit)) {
            exact_icon = binding->frame;
        }
    }
    panel.commands.push_back({
        command, std::move(label), std::move(hotkey), std::move(tooltip),
        enabled, selected,
        unit, std::nullopt, unit ? exact_icon : std::nullopt,
        unit ? std::nullopt : action_icon,
    });
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
        kind != UnitKind::trade_cart;
}

}  // namespace

SelectionPanelModel build_selection_panel(
    const Simulation& simulation,
    Player player
) {
    SelectionPanelModel panel;
    if (simulation.selected_unit()) {
        const auto selected = std::ranges::find_if(
            simulation.units(),
            [&simulation](const Unit& unit) {
                return unit.id == *simulation.selected_unit();
            }
        );
        if (selected == simulation.units().end()) return panel;
        panel.title = std::string{name(selected->kind)};
        panel.hit_points = selected->hit_points;
        panel.maximum_hit_points = rules_for(selected->kind).hit_points;
        panel.carried_resource = selected->carried_resource;
        panel.carried_amount = selected->carried_amount;
        panel.status = selected->garrisoned_in != 0 ? "GARRISONED" :
            selected->attacking_ground ? "ATTACKING GROUND" :
            selected->attack_moving ? "ATTACK MOVE" :
            selected->patrolling ? "PATROLLING" :
            selected->guard_target_id != 0 ? "GUARDING" :
            selected->moving ? "MOVING" : "IDLE";
        // btncmd (50721) is an action-icon sheet. Each command uses its own
        // frame; frames 36/37 are artwork, not reusable button chrome.
        add(panel, PanelCommand::stop, "STOP", "S",
            "Stop all current orders.", true, std::nullopt, false, 4);
        if (supports_military_orders(selected->kind)) {
            add(panel, PanelCommand::attack_move, "ATTACK MOVE", "A",
                "Move to a location and attack enemies along the way.",
                true, std::nullopt, selected->attack_moving, 0);
            add(panel, PanelCommand::patrol, "PATROL", "P",
                "Patrol between the current position and a destination.",
                true, std::nullopt, selected->patrolling, 1);
            add(panel, PanelCommand::guard, "GUARD", "G",
                "Follow and protect a friendly unit or building.",
                true, std::nullopt, selected->guard_target_id != 0, 2);
        }
        if (supports_attack_ground(selected->kind)) {
            add(panel, PanelCommand::attack_ground, "ATTACK GROUND", "X",
                "Fire at a chosen ground location.",
                true, std::nullopt, selected->attacking_ground, 12);
        }
        if (supports_garrison(selected->kind)) {
            add(panel, PanelCommand::garrison, "GARRISON", "H",
                "Enter a compatible friendly building.", true,
                std::nullopt, selected->garrisoned_in != 0, 3);
        }
        if (supports_stance(selected->kind)) {
            add(panel, PanelCommand::stance_aggressive, "AGGRESSIVE", "O",
                "Pursue enemies at full sight range.", true, std::nullopt,
                selected->stance == UnitStance::aggressive, 5);
            add(panel, PanelCommand::stance_defensive, "DEFENSIVE", "D",
                "Pursue nearby enemies, then return.", true, std::nullopt,
                selected->stance == UnitStance::defensive, 6);
            add(panel, PanelCommand::stance_stand_ground, "STAND GROUND", "N",
                "Attack in range without moving.", true, std::nullopt,
                selected->stance == UnitStance::stand_ground, 7);
            add(panel, PanelCommand::stance_no_attack, "NO ATTACK", "T",
                "Do not acquire or attack enemies.", true, std::nullopt,
                selected->stance == UnitStance::passive, 8);
        }
        const bool group_formation =
            simulation.selected_units().size() > 1 &&
            supports_military_orders(selected->kind);
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
        add(panel, PanelCommand::rally, "RALLY POINT", "R",
            "Set destination for newly trained units.", true,
            std::nullopt, false, 14);
        add(panel, PanelCommand::ungarrison, "UNGARRISON", "U",
            "Release all units garrisoned in this building.",
            panel.garrison_count > 0, std::nullopt, false, 15);
        add(panel, PanelCommand::cancel_production, "CANCEL LAST", "Q",
            "Cancel last queued unit and refund its cost.",
            !selected->production_queue.empty(), std::nullopt, false, 16);
        const auto train = [&](UnitKind kind, std::string key) {
            if (civilization_has_unit(
                    simulation.civilization(player), kind
                ) &&
                simulation.age(player) >= rules_for(kind).minimum_age) {
                add(panel, PanelCommand::train_unit,
                    std::string{name(kind)}, std::move(key),
                    "Train " + std::string{name(kind)} + ".", true, kind);
            }
        };
        switch (selected->kind) {
            case BuildingKind::town_center:
                train(UnitKind::villager, "V"); break;
            case BuildingKind::barracks:
                train(UnitKind::militia, "M");
                train(UnitKind::spearman, "Z"); break;
            case BuildingKind::archery_range:
                train(UnitKind::archer, "A");
                train(UnitKind::skirmisher, "S"); break;
            case BuildingKind::stable:
                train(UnitKind::scout_cavalry, "Q");
                train(UnitKind::knight, "K");
                train(UnitKind::camel_rider, "C"); break;
            case BuildingKind::siege_workshop:
                train(UnitKind::battering_ram, "3");
                train(UnitKind::mangonel, "O");
                train(UnitKind::scorpion, "X"); break;
            case BuildingKind::monastery:
                train(UnitKind::monk, "M"); break;
            case BuildingKind::dock:
                train(UnitKind::fishing_ship, "F");
                train(UnitKind::galley, "G");
                train(UnitKind::transport_ship, "T"); break;
            case BuildingKind::castle:
                train(UnitKind::woad_raider, "W"); break;
            default: break;
        }
    }
    return panel;
}

}  // namespace aoe

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
    bool enabled = true,
    std::optional<UnitKind> unit = std::nullopt
) {
    if (panel.commands.size() >= 12) return;
    std::optional<std::int32_t> exact_icon;
    if (unit) {
        if (const auto binding = ui_icons::training_unit(*unit)) {
            exact_icon = binding->frame;
        }
    }
    panel.commands.push_back({
        command, std::move(label), std::move(hotkey), enabled,
        unit, std::nullopt, exact_icon,
    });
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
        add(panel, PanelCommand::stop, "STOP", "S");
        add(panel, PanelCommand::attack_move, "ATTACK MOVE", "A");
        add(panel, PanelCommand::attack_ground, "ATTACK GROUND", "X");
        add(panel, PanelCommand::patrol, "PATROL", "P");
        add(panel, PanelCommand::guard, "GUARD", "G");
        add(panel, PanelCommand::garrison, "GARRISON", "H");
        add(panel, PanelCommand::stance, "STANCE", "O");
        add(panel, PanelCommand::formation_compact, "COMPACT", "C");
        add(panel, PanelCommand::formation_line, "LINE", "L");
        add(panel, PanelCommand::formation_box, "BOX", "B");
        add(panel, PanelCommand::formation_flank, "FLANK", "F");
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
        add(panel, PanelCommand::rally, "RALLY POINT", "R");
        add(panel, PanelCommand::ungarrison, "UNGARRISON", "U",
            panel.garrison_count > 0);
        add(panel, PanelCommand::cancel_production, "CANCEL LAST", "Q",
            !selected->production_queue.empty());
        const auto train = [&](UnitKind kind, std::string key) {
            if (civilization_has_unit(
                    simulation.civilization(player), kind
                ) &&
                simulation.age(player) >= rules_for(kind).minimum_age) {
                add(panel, PanelCommand::train_unit,
                    std::string{name(kind)}, std::move(key), true, kind);
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

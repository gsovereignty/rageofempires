#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aoe/simulation.hpp"
#include "aoe/ui_icon_contract.hpp"

namespace aoe {

enum class PanelCommand {
    stop, delete_entity, attack_move, attack_ground, patrol, guard, follow,
    garrison, advance_age,
    stance_aggressive, stance_defensive, stance_stand_ground,
    stance_no_attack, formation_line, formation_box,
    formation_staggered, formation_flank, rally, ungarrison, town_bell,
    lock_gate, unlock_gate,
    cancel_production,
    train_unit, research, convert, trade_route, pack_trebuchet,
    unpack_trebuchet, open_economic_buildings, open_military_buildings,
    open_defensive_buildings, construct_building, back,
    repair, heal, collect_relic, deposit_relic,
    embark, disembark,
    open_production, open_research, previous_page, next_page,
};

enum class PanelPage {
    root,
    economic_buildings,
    military_buildings,
    defensive_buildings,
    production,
    research,
};

struct CommandButtonModel {
    PanelCommand command{PanelCommand::stop};
    std::string label;
    std::string hotkey;
    std::string tooltip;
    bool enabled{true};
    bool selected{};
    std::optional<UnitKind> unit;
    std::optional<Technology> technology;
    std::optional<BuildingKind> building;
    std::optional<ui_icons::Binding> icon;
    bool procedural_icon_fallback{true};
    std::size_t grid_slot{};
};

struct SelectionPanelModel {
    std::string title{"NO SELECTION"};
    int hit_points{};
    int maximum_hit_points{};
    int progress_percent{-1};
    int garrison_count{};
    ResourceKind carried_resource{ResourceKind::none};
    int carried_amount{};
    std::string status;
    std::size_t page_index{};
    std::size_t page_count{1};
    std::vector<CommandButtonModel> commands;
};

SelectionPanelModel build_selection_panel(
    const Simulation& simulation,
    Player player,
    PanelPage page = PanelPage::root,
    std::size_t page_index = 0
);

}  // namespace aoe

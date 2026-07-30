#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aoe/simulation.hpp"

namespace aoe {

enum class PanelCommand {
    stop, attack_move, attack_ground, patrol, guard, garrison,
    stance_aggressive, stance_defensive, stance_stand_ground,
    stance_no_attack, formation_line, formation_box,
    formation_staggered, formation_flank, rally, ungarrison, cancel_production,
    train_unit, research,
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
    std::optional<std::int32_t> proven_archive_icon_id;
    std::optional<std::int32_t> action_archive_icon_id;
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
    std::vector<CommandButtonModel> commands;
};

SelectionPanelModel build_selection_panel(
    const Simulation& simulation,
    Player player
);

}  // namespace aoe

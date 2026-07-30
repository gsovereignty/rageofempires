#pragma once

#include <optional>
#include <span>
#include <vector>

#include "aoe/types.hpp"

namespace aoe {

struct SelectionControlGroup {
    std::vector<EntityId> units;
    std::optional<EntityId> building;

    bool operator==(const SelectionControlGroup&) const = default;
};

void toggle_selected_id(std::vector<EntityId>& selection, EntityId id);
void prune_control_group(
    SelectionControlGroup& group,
    std::span<const Unit> units,
    std::span<const Building> buildings,
    Player owner
);
std::vector<EntityId> same_visible_owned_type(
    std::span<const Unit> units,
    const Unit& clicked,
    Player owner,
    std::span<const std::uint8_t> visible
);
std::vector<EntityId> filter_drag_selection(
    std::span<const Unit> units,
    Player owner,
    std::span<const EntityId> contained
);
std::optional<int> exact_health_fill_pixels(
    double current_hit_points,
    int maximum_hit_points
);

}  // namespace aoe

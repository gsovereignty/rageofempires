#include "aoe/selection_controls.hpp"

#include <algorithm>
#include <cmath>

namespace aoe {

std::optional<int> exact_health_fill_pixels(
    double current_hit_points,
    int maximum_hit_points
) {
    if (!std::isfinite(current_hit_points) ||
        current_hit_points <= 0.0 ||
        maximum_hit_points <= 0) {
        return std::nullopt;
    }
    const int current = static_cast<int>(std::clamp(
        current_hit_points,
        0.0,
        static_cast<double>(maximum_hit_points)
    ));
    return 1 + static_cast<int>(
        static_cast<long long>(current) * 24LL /
        static_cast<long long>(maximum_hit_points)
    );
}

void toggle_selected_id(std::vector<EntityId>& selection, EntityId id) {
    const auto found = std::ranges::find(selection, id);
    if (found == selection.end()) selection.push_back(id);
    else selection.erase(found);
}

void prune_control_group(
    SelectionControlGroup& group,
    std::span<const Unit> units,
    std::span<const Building> buildings,
    Player owner
) {
    std::erase_if(group.units, [&](EntityId id) {
        return std::ranges::none_of(units, [&](const Unit& unit) {
            return unit.id == id && unit.owner == owner &&
                unit.garrisoned_in == 0;
        });
    });
    if (group.building &&
        std::ranges::none_of(buildings, [&](const Building& building) {
            return building.id == *group.building &&
                building.owner == owner;
        })) {
        group.building.reset();
    }
    if (!group.units.empty()) group.building.reset();
}

std::vector<EntityId> same_visible_owned_type(
    std::span<const Unit> units,
    const Unit& clicked,
    Player owner,
    std::span<const std::uint8_t> visible
) {
    std::vector<EntityId> result;
    for (std::size_t index = 0; index < units.size(); ++index) {
        const Unit& unit = units[index];
        if (unit.owner == owner && unit.kind == clicked.kind &&
            unit.garrisoned_in == 0 &&
            index < visible.size() && visible[index]) {
            result.push_back(unit.id);
        }
    }
    return result;
}

std::vector<EntityId> filter_drag_selection(
    std::span<const Unit> units,
    Player owner,
    std::span<const EntityId> contained
) {
    std::vector<EntityId> result;
    for (EntityId id : contained) {
        const auto found = std::ranges::find(units, id, &Unit::id);
        if (found == units.end() || found->owner != owner ||
            found->garrisoned_in != 0 ||
            found->kind == UnitKind::sheep ||
            found->kind == UnitKind::deer ||
            found->kind == UnitKind::boar ||
            found->kind == UnitKind::relic) {
            continue;
        }
        if (std::ranges::find(result, id) == result.end()) {
            result.push_back(id);
        }
    }
    return result;
}

}  // namespace aoe

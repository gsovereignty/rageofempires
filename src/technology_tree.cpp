#include "aoe/technology_tree.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace aoe {
namespace {

int age_column(Age age) {
    return static_cast<int>(age);
}

std::string age_requirement(Age age) {
    return std::string{name(age)} + " Age";
}

}

TechnologyTreeLayout build_technology_tree(
    Civilization civilization,
    const std::vector<Technology>& researched
) {
    TechnologyTreeLayout layout;
    layout.civilization = civilization;
    constexpr std::size_t unit_count =
        static_cast<std::size_t>(UnitKind::elite_woad_raider) + 1;
    constexpr std::size_t building_count =
        static_cast<std::size_t>(BuildingKind::wonder) + 1;
    std::array<int, 4> rows{};
    auto position = [&](Age age, int group) {
        const int column = age_column(age);
        const int y = 86 + rows[column]++ * 62 + group * 12;
        return std::pair{80 + column * 310, y};
    };
    for (std::size_t index = 0; index < unit_count; ++index) {
        const UnitKind unit = static_cast<UnitKind>(index);
        const UnitRules& rules = rules_for(unit);
        const auto [x, y] = position(rules.minimum_age, 0);
        layout.nodes.push_back({
            TechnologyTreeNodeKind::unit, index,
            std::string{name(unit)}, rules.minimum_age,
            civilization_has_unit(civilization, unit)
                ? TechnologyTreeNodeState::available
                : TechnologyTreeNodeState::disabled,
            x, y, rules.wood_cost, rules.food_cost, rules.gold_cost, 0,
            age_requirement(rules.minimum_age),
        });
    }
    for (std::size_t index = 0; index < building_count; ++index) {
        const BuildingKind building = static_cast<BuildingKind>(index);
        const BuildingRules& rules = rules_for(building);
        const auto [x, y] = position(rules.minimum_age, 1);
        layout.nodes.push_back({
            TechnologyTreeNodeKind::building, index,
            std::string{name(building)}, rules.minimum_age,
            civilization_has_building(civilization, building)
                ? TechnologyTreeNodeState::available
                : TechnologyTreeNodeState::disabled,
            x, y, rules.wood_cost, 0, rules.gold_cost, rules.stone_cost,
            age_requirement(rules.minimum_age),
        });
    }
    for (std::size_t index = 0; index < technology_count; ++index) {
        const Technology technology = static_cast<Technology>(index);
        const TechnologyRules& rules = rules_for(technology);
        const auto [x, y] = position(rules.minimum_age, 2);
        TechnologyTreeNodeState state =
            civilization_has_technology(civilization, technology)
            ? TechnologyTreeNodeState::available
            : TechnologyTreeNodeState::disabled;
        if (std::ranges::find(researched, technology) != researched.end()) {
            state = TechnologyTreeNodeState::upgraded;
        }
        layout.nodes.push_back({
            TechnologyTreeNodeKind::technology, index,
            std::string{name(technology)}, rules.minimum_age,
            state, x, y,
            rules.wood_cost, rules.food_cost,
            rules.gold_cost, rules.stone_cost,
            std::string{"Research at "} +
                std::string{name(rules.researched_at)} +
                "; " + age_requirement(rules.minimum_age),
        });
    }
    const std::size_t building_base = unit_count;
    const std::size_t technology_base = unit_count + building_count;
    for (std::size_t index = 0; index < unit_count; ++index) {
        layout.dependencies.emplace_back(
            building_base + static_cast<std::size_t>(
                rules_for(static_cast<UnitKind>(index)).trained_at),
            index
        );
    }
    for (std::size_t index = 0; index < technology_count; ++index) {
        layout.dependencies.emplace_back(
            building_base + static_cast<std::size_t>(
                rules_for(static_cast<Technology>(index)).researched_at),
            technology_base + index
        );
    }
    layout.width = 4 * 310 + 160;
    layout.height =
        120 + *std::max_element(rows.begin(), rows.end()) * 62;
    return layout;
}

std::size_t navigate_technology_tree(
    const TechnologyTreeLayout& layout,
    std::size_t current,
    TechnologyTreeDirection direction
) {
    if (layout.nodes.empty()) return 0;
    current = std::min(current, layout.nodes.size() - 1);
    if (direction == TechnologyTreeDirection::previous) {
        return current == 0 ? layout.nodes.size() - 1 : current - 1;
    }
    if (direction == TechnologyTreeDirection::next) {
        return (current + 1) % layout.nodes.size();
    }

    const TechnologyTreeNode& origin = layout.nodes[current];
    std::size_t best = current;
    long long best_score = std::numeric_limits<long long>::max();
    for (std::size_t index = 0; index < layout.nodes.size(); ++index) {
        if (index == current) continue;
        const TechnologyTreeNode& candidate = layout.nodes[index];
        const int dx = candidate.x - origin.x;
        const int dy = candidate.y - origin.y;
        const bool valid =
            (direction == TechnologyTreeDirection::left && dx < 0) ||
            (direction == TechnologyTreeDirection::right && dx > 0) ||
            (direction == TechnologyTreeDirection::up && dy < 0) ||
            (direction == TechnologyTreeDirection::down && dy > 0);
        if (!valid) continue;
        const bool horizontal =
            direction == TechnologyTreeDirection::left ||
            direction == TechnologyTreeDirection::right;
        const long long primary = horizontal
            ? static_cast<long long>(std::abs(dx))
            : static_cast<long long>(std::abs(dy));
        const long long secondary = horizontal
            ? static_cast<long long>(std::abs(dy))
            : static_cast<long long>(std::abs(dx));
        const long long score = primary * 100000LL + secondary;
        if (score < best_score) {
            best_score = score;
            best = index;
        }
    }
    return best;
}

}  // namespace aoe

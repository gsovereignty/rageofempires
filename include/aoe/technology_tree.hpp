#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aoe/game_rules.hpp"

namespace aoe {

enum class TechnologyTreeNodeKind { unit, building, technology };
enum class TechnologyTreeNodeState { available, disabled, upgraded };
enum class TechnologyTreeDirection { previous, next, left, right, up, down };

struct TechnologyTreeNode {
    TechnologyTreeNodeKind kind;
    std::size_t id{};
    std::string label;
    Age age{Age::dark};
    TechnologyTreeNodeState state{TechnologyTreeNodeState::disabled};
    int x{};
    int y{};
    int wood{};
    int food{};
    int gold{};
    int stone{};
    std::string requirement;
    // Producer-family lane. Stable across civilizations and used for
    // original-style row navigation and graph verification.
    BuildingKind producer{BuildingKind::town_center};
};

struct TechnologyTreeLayout {
    Civilization civilization{Civilization::britons};
    std::vector<TechnologyTreeNode> nodes;
    std::vector<std::pair<std::size_t, std::size_t>> dependencies;
    int width{};
    int height{};
};

[[nodiscard]] TechnologyTreeLayout build_technology_tree(
    Civilization civilization,
    const std::vector<Technology>& researched = {}
);
[[nodiscard]] std::size_t navigate_technology_tree(
    const TechnologyTreeLayout& layout,
    std::size_t current,
    TechnologyTreeDirection direction
);

}  // namespace aoe

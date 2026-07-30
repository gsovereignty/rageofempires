#include "aoe/technology_tree.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value) {
    if (!value) throw std::runtime_error("technology tree test failed");
}
}

int main() {
    const aoe::TechnologyTreeLayout britons =
        aoe::build_technology_tree(
            aoe::Civilization::britons,
            {aoe::Technology::fletching}
        );
    constexpr std::size_t expected =
        static_cast<std::size_t>(aoe::UnitKind::elite_woad_raider) + 1 +
        static_cast<std::size_t>(aoe::BuildingKind::wonder) + 1 +
        aoe::technology_count;
    require(britons.nodes.size() == expected);
    require(britons.dependencies.size() ==
            static_cast<std::size_t>(aoe::UnitKind::elite_woad_raider) + 1 +
            aoe::technology_count);
    require(britons.width > 1000);
    require(britons.height > 1000);
    bool upgraded{};
    bool disabled{};
    for (const aoe::TechnologyTreeNode& node : britons.nodes) {
        require(node.x >= 0 && node.y >= 0);
        upgraded = upgraded ||
            (node.kind == aoe::TechnologyTreeNodeKind::technology &&
             node.id == static_cast<std::size_t>(
                 aoe::Technology::fletching) &&
             node.state ==
                 aoe::TechnologyTreeNodeState::upgraded);
        disabled = disabled ||
            node.state == aoe::TechnologyTreeNodeState::disabled;
    }
    require(upgraded);
    require(disabled);
    const auto mayans =
        aoe::build_technology_tree(aoe::Civilization::mayans);
    require(mayans.nodes.size() == britons.nodes.size());
    require(aoe::navigate_technology_tree(
                britons, 0, aoe::TechnologyTreeDirection::previous
            ) == britons.nodes.size() - 1);
    require(aoe::navigate_technology_tree(
                britons, britons.nodes.size() - 1,
                aoe::TechnologyTreeDirection::next
            ) == 0);
    const std::size_t right = aoe::navigate_technology_tree(
        britons, 0, aoe::TechnologyTreeDirection::right
    );
    require(right < britons.nodes.size());
    require(britons.nodes[right].x > britons.nodes[0].x);
    const std::size_t down = aoe::navigate_technology_tree(
        britons, 0, aoe::TechnologyTreeDirection::down
    );
    require(down < britons.nodes.size());
    require(britons.nodes[down].y > britons.nodes[0].y);
    std::cout << "technology tree tests passed\n";
}

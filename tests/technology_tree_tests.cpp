#include "aoe/technology_tree.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool value) {
    if (!value) throw std::runtime_error("technology tree test failed");
}

bool forbidden_unit(std::size_t id) {
    using aoe::UnitKind;
    const auto unit = static_cast<UnitKind>(id);
    return unit == UnitKind::sheep || unit == UnitKind::deer ||
        unit == UnitKind::boar || unit == UnitKind::relic ||
        unit == UnitKind::king || unit == UnitKind::packed_trebuchet;
}

bool forbidden_technology(std::size_t id) {
    using aoe::Technology;
    const auto technology = static_cast<Technology>(id);
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
    case Technology::fish_trap_gate:
    case Technology::trade_cog_gate:
    case Technology::outpost_gate:
    case Technology::wonder_plans:
        return true;
    default:
        return false;
    }
}
}

int main() {
    std::size_t common_node_count{};
    for (int civ = 1; civ <= 18; ++civ) {
        const auto civilization = static_cast<aoe::Civilization>(civ);
        const aoe::TechnologyTreeLayout layout =
            aoe::build_technology_tree(civilization);
        require(!layout.nodes.empty());
        require(layout.width == 1290);
        require(layout.height > 1800);
        if (common_node_count == 0) common_node_count = layout.nodes.size();
        require(layout.nodes.size() == common_node_count);

        std::set<std::pair<aoe::TechnologyTreeNodeKind, std::size_t>> keys;
        std::set<std::pair<int, int>> coordinates;
        bool available{};
        bool disabled{};
        std::size_t unit_nodes{};
        std::size_t technology_nodes{};
        for (const auto& node : layout.nodes) {
            require(keys.emplace(node.kind, node.id).second);
            require(coordinates.emplace(node.x, node.y).second);
            require(node.x >= 0 && node.y >= 0);
            require(node.x < layout.width && node.y < layout.height);
            require(node.producer ==
                (node.kind == aoe::TechnologyTreeNodeKind::unit
                    ? aoe::rules_for(static_cast<aoe::UnitKind>(node.id)).trained_at
                    : node.kind == aoe::TechnologyTreeNodeKind::technology
                    ? aoe::rules_for(static_cast<aoe::Technology>(node.id)).researched_at
                    : static_cast<aoe::BuildingKind>(node.id)));
            require(node.kind != aoe::TechnologyTreeNodeKind::unit ||
                    !forbidden_unit(node.id));
            require(node.kind != aoe::TechnologyTreeNodeKind::technology ||
                    !forbidden_technology(node.id));
            available = available ||
                node.state == aoe::TechnologyTreeNodeState::available;
            disabled = disabled ||
                node.state == aoe::TechnologyTreeNodeState::disabled;
            unit_nodes += node.kind == aoe::TechnologyTreeNodeKind::unit;
            technology_nodes +=
                node.kind == aoe::TechnologyTreeNodeKind::technology;
        }
        require(unit_nodes == aoe::unit_kind_count - 6);
        require(technology_nodes == aoe::technology_count - 26);
        require(available && disabled);
        for (const auto& [from, to] : layout.dependencies) {
            require(from < layout.nodes.size() && to < layout.nodes.size());
            require(from != to);
            require(layout.nodes[from].producer == layout.nodes[to].producer);
        }
    }

    const auto britons = aoe::build_technology_tree(
        aoe::Civilization::britons, {aoe::Technology::fletching}
    );
    const auto fletching = std::ranges::find_if(
        britons.nodes, [](const auto& node) {
            return node.kind == aoe::TechnologyTreeNodeKind::technology &&
                node.id == static_cast<std::size_t>(aoe::Technology::fletching);
        }
    );
    require(fletching != britons.nodes.end());
    require(fletching->state == aoe::TechnologyTreeNodeState::upgraded);

    const auto edge_exists = [&](auto from_kind, std::size_t from_id,
                                 auto to_kind, std::size_t to_id) {
        for (const auto& [from, to] : britons.dependencies) {
            if (britons.nodes[from].kind == from_kind &&
                britons.nodes[from].id == from_id &&
                britons.nodes[to].kind == to_kind &&
                britons.nodes[to].id == to_id) return true;
        }
        return false;
    };
    require(edge_exists(
        aoe::TechnologyTreeNodeKind::unit,
        static_cast<std::size_t>(aoe::UnitKind::archer),
        aoe::TechnologyTreeNodeKind::unit,
        static_cast<std::size_t>(aoe::UnitKind::crossbowman)
    ));
    require(edge_exists(
        aoe::TechnologyTreeNodeKind::technology,
        static_cast<std::size_t>(aoe::Technology::fletching),
        aoe::TechnologyTreeNodeKind::technology,
        static_cast<std::size_t>(aoe::Technology::bodkin_arrow)
    ));
    require(aoe::navigate_technology_tree(
        britons, 0, aoe::TechnologyTreeDirection::previous
    ) == britons.nodes.size() - 1);
    require(aoe::navigate_technology_tree(
        britons, britons.nodes.size() - 1,
        aoe::TechnologyTreeDirection::next
    ) == 0);
    std::cout << "technology tree tests passed\n";
}

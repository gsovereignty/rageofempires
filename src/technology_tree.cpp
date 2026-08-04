#include "aoe/technology_tree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace aoe {
namespace {

constexpr int age_width = 238;
constexpr int lane_height = 118;
constexpr int node_width = 150;

std::string age_requirement(Age age) {
    return std::string{name(age)} + " Age";
}

bool player_facing_unit(UnitKind unit) {
    return unit != UnitKind::sheep && unit != UnitKind::deer &&
        unit != UnitKind::boar && unit != UnitKind::relic &&
        unit != UnitKind::king && unit != UnitKind::packed_trebuchet;
}

bool hidden_technology(Technology technology) {
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

// Orientation variants are one player-facing tree item. Upgraded defenses
// remain distinct nodes because their upgrade chains are meaningful.
bool player_facing_building(BuildingKind building) {
    switch (building) {
    case BuildingKind::palisade_gate_y:
    case BuildingKind::stone_gate_y:
    case BuildingKind::fortified_gate_y:
    case BuildingKind::fish_trap:
        return false;
    default:
        return true;
    }
}

using UnitEdge = std::pair<UnitKind, UnitKind>;
constexpr std::array unit_upgrades{
    UnitEdge{UnitKind::militia, UnitKind::man_at_arms},
    UnitEdge{UnitKind::man_at_arms, UnitKind::long_swordsman},
    UnitEdge{UnitKind::long_swordsman, UnitKind::two_handed_swordsman},
    UnitEdge{UnitKind::two_handed_swordsman, UnitKind::champion},
    UnitEdge{UnitKind::spearman, UnitKind::pikeman},
    UnitEdge{UnitKind::pikeman, UnitKind::halberdier},
    UnitEdge{UnitKind::archer, UnitKind::crossbowman},
    UnitEdge{UnitKind::crossbowman, UnitKind::arbalester},
    UnitEdge{UnitKind::skirmisher, UnitKind::elite_skirmisher},
    UnitEdge{UnitKind::scout_cavalry, UnitKind::light_cavalry},
    UnitEdge{UnitKind::light_cavalry, UnitKind::hussar},
    UnitEdge{UnitKind::knight, UnitKind::cavalier},
    UnitEdge{UnitKind::cavalier, UnitKind::paladin},
    UnitEdge{UnitKind::camel_rider, UnitKind::heavy_camel},
    UnitEdge{UnitKind::cavalry_archer, UnitKind::heavy_cavalry_archer},
    UnitEdge{UnitKind::battering_ram, UnitKind::capped_ram},
    UnitEdge{UnitKind::capped_ram, UnitKind::siege_ram},
    UnitEdge{UnitKind::mangonel, UnitKind::onager},
    UnitEdge{UnitKind::onager, UnitKind::siege_onager},
    UnitEdge{UnitKind::scorpion, UnitKind::heavy_scorpion},
    UnitEdge{UnitKind::galley, UnitKind::war_galley},
    UnitEdge{UnitKind::war_galley, UnitKind::galleon},
    UnitEdge{UnitKind::fire_ship, UnitKind::fast_fire_ship},
    UnitEdge{UnitKind::demolition_ship, UnitKind::heavy_demolition_ship},
    UnitEdge{UnitKind::cannon_galleon, UnitKind::elite_cannon_galleon},
    UnitEdge{UnitKind::longboat, UnitKind::elite_longboat},
    UnitEdge{UnitKind::turtle_ship, UnitKind::elite_turtle_ship},
    UnitEdge{UnitKind::longbowman, UnitKind::elite_longbowman},
    UnitEdge{UnitKind::throwing_axeman, UnitKind::elite_throwing_axeman},
    UnitEdge{UnitKind::huskarl, UnitKind::elite_huskarl},
    UnitEdge{UnitKind::teutonic_knight, UnitKind::elite_teutonic_knight},
    UnitEdge{UnitKind::samurai, UnitKind::elite_samurai},
    UnitEdge{UnitKind::chu_ko_nu, UnitKind::elite_chu_ko_nu},
    UnitEdge{UnitKind::cataphract, UnitKind::elite_cataphract},
    UnitEdge{UnitKind::war_elephant, UnitKind::elite_war_elephant},
    UnitEdge{UnitKind::mameluke, UnitKind::elite_mameluke},
    UnitEdge{UnitKind::janissary, UnitKind::elite_janissary},
    UnitEdge{UnitKind::berserk, UnitKind::elite_berserk},
    UnitEdge{UnitKind::mangudai, UnitKind::elite_mangudai},
    UnitEdge{UnitKind::jaguar_warrior, UnitKind::elite_jaguar_warrior},
    UnitEdge{UnitKind::plumed_archer, UnitKind::elite_plumed_archer},
    UnitEdge{UnitKind::conquistador, UnitKind::elite_conquistador},
    UnitEdge{UnitKind::tarkan, UnitKind::elite_tarkan},
    UnitEdge{UnitKind::eagle_warrior, UnitKind::elite_eagle_warrior},
    UnitEdge{UnitKind::woad_raider, UnitKind::elite_woad_raider},
};

using TechnologyEdge = std::pair<Technology, Technology>;
constexpr std::array technology_upgrades{
    TechnologyEdge{Technology::fletching, Technology::bodkin_arrow},
    TechnologyEdge{Technology::bodkin_arrow, Technology::bracer},
    TechnologyEdge{Technology::forging, Technology::iron_casting},
    TechnologyEdge{Technology::iron_casting, Technology::blast_furnace},
    TechnologyEdge{Technology::scale_mail_armor, Technology::chain_mail_armor},
    TechnologyEdge{Technology::chain_mail_armor, Technology::plate_mail_armor},
    TechnologyEdge{Technology::scale_barding_armor, Technology::chain_barding_armor},
    TechnologyEdge{Technology::chain_barding_armor, Technology::plate_barding_armor},
    TechnologyEdge{Technology::padded_archer_armor, Technology::leather_archer_armor},
    TechnologyEdge{Technology::leather_archer_armor, Technology::ring_archer_armor},
    TechnologyEdge{Technology::double_bit_axe, Technology::bow_saw},
    TechnologyEdge{Technology::bow_saw, Technology::two_man_saw},
    TechnologyEdge{Technology::horse_collar, Technology::heavy_plow},
    TechnologyEdge{Technology::heavy_plow, Technology::crop_rotation},
    TechnologyEdge{Technology::gold_mining, Technology::gold_shaft_mining},
    TechnologyEdge{Technology::stone_mining, Technology::stone_shaft_mining},
    TechnologyEdge{Technology::wheelbarrow, Technology::hand_cart},
    TechnologyEdge{Technology::town_watch, Technology::town_patrol},
    TechnologyEdge{Technology::careening, Technology::dry_dock},
    TechnologyEdge{Technology::coinage, Technology::banking},
};

using BuildingEdge = std::pair<BuildingKind, BuildingKind>;
constexpr std::array building_upgrades{
    BuildingEdge{BuildingKind::watch_tower, BuildingKind::guard_tower},
    BuildingEdge{BuildingKind::guard_tower, BuildingKind::keep},
    BuildingEdge{BuildingKind::stone_wall, BuildingKind::fortified_wall},
    BuildingEdge{BuildingKind::stone_gate_x, BuildingKind::fortified_gate_x},
};

template<typename Kind>
std::optional<std::size_t> find_node(
    const TechnologyTreeLayout& layout,
    TechnologyTreeNodeKind node_kind,
    Kind id
) {
    const auto found = std::ranges::find_if(
        layout.nodes,
        [=](const TechnologyTreeNode& node) {
            return node.kind == node_kind &&
                node.id == static_cast<std::size_t>(id);
        }
    );
    if (found == layout.nodes.end()) return std::nullopt;
    return static_cast<std::size_t>(found - layout.nodes.begin());
}

}  // namespace

TechnologyTreeLayout build_technology_tree(
    Civilization civilization,
    const std::vector<Technology>& researched
) {
    TechnologyTreeLayout layout;
    layout.civilization = civilization;

    // Manual and original screen organize rows by producer, then ages from
    // left to right. Keep every supported family even when civilization
    // disables its final tier: genuine exclusions stay visible and shaded.
    std::vector<BuildingKind> producers;
    for (std::size_t value = 0; value < building_kind_count; ++value) {
        const auto building = static_cast<BuildingKind>(value);
        if (!player_facing_building(building)) continue;
        bool used = false;
        for (std::size_t unit_value = 0; unit_value < unit_kind_count; ++unit_value) {
            const auto unit = static_cast<UnitKind>(unit_value);
            used = used || (player_facing_unit(unit) &&
                rules_for(unit).trained_at == building);
        }
        for (std::size_t tech_value = 0; tech_value < technology_count; ++tech_value) {
            const auto technology = static_cast<Technology>(tech_value);
            used = used || (!hidden_technology(technology) &&
                rules_for(technology).researched_at == building);
        }
        if (used || building == BuildingKind::house ||
            building == BuildingKind::farm ||
            building == BuildingKind::palisade_wall ||
            building == BuildingKind::stone_wall ||
            building == BuildingKind::palisade_gate_x ||
            building == BuildingKind::stone_gate_x ||
            building == BuildingKind::wonder) {
            producers.push_back(building);
        }
    }

    std::set<BuildingKind> producer_set(producers.begin(), producers.end());
    std::map<BuildingKind, int> lane_y;
    int next_lane_y = 66;
    for (const BuildingKind producer : producers) {
        std::array<int, 4> age_slots{};
        ++age_slots[static_cast<std::size_t>(rules_for(producer).minimum_age)];
        for (std::size_t value = 0; value < unit_kind_count; ++value) {
            const auto unit = static_cast<UnitKind>(value);
            if (player_facing_unit(unit) &&
                rules_for(unit).trained_at == producer) {
                ++age_slots[static_cast<std::size_t>(
                    rules_for(unit).minimum_age)];
            }
        }
        for (std::size_t value = 0; value < technology_count; ++value) {
            const auto technology = static_cast<Technology>(value);
            if (!hidden_technology(technology) &&
                rules_for(technology).researched_at == producer) {
                ++age_slots[static_cast<std::size_t>(
                    rules_for(technology).minimum_age)];
            }
        }
        lane_y[producer] = next_lane_y;
        next_lane_y += std::max(
            lane_height,
            *std::max_element(age_slots.begin(), age_slots.end()) * 48 + 58
        );
    }
    std::map<std::pair<BuildingKind, Age>, int> slots;
    auto position = [&](BuildingKind producer, Age age) {
        const int slot = slots[{producer, age}]++;
        return std::pair{
            188 + static_cast<int>(age) * age_width,
            lane_y[producer] + slot * 48
        };
    };

    for (const BuildingKind building : producers) {
        const BuildingRules& rules = rules_for(building);
        const auto [x, y] = position(building, rules.minimum_age);
        layout.nodes.push_back({
            TechnologyTreeNodeKind::building,
            static_cast<std::size_t>(building), std::string{name(building)},
            rules.minimum_age,
            civilization_has_building(civilization, building)
                ? TechnologyTreeNodeState::available
                : TechnologyTreeNodeState::disabled,
            x, y, rules.wood_cost, 0, rules.gold_cost, rules.stone_cost,
            age_requirement(rules.minimum_age), building,
        });
    }

    for (std::size_t value = 0; value < unit_kind_count; ++value) {
        const auto unit = static_cast<UnitKind>(value);
        if (!player_facing_unit(unit)) continue;
        const UnitRules& rules = rules_for(unit);
        if (!producer_set.contains(rules.trained_at)) continue;
        const auto [x, y] = position(rules.trained_at, rules.minimum_age);
        layout.nodes.push_back({
            TechnologyTreeNodeKind::unit, value, std::string{name(unit)},
            rules.minimum_age,
            civilization_has_unit(civilization, unit)
                ? TechnologyTreeNodeState::available
                : TechnologyTreeNodeState::disabled,
            x, y, rules.wood_cost, rules.food_cost, rules.gold_cost, 0,
            std::string{"Trained at "} + std::string{name(rules.trained_at)} +
                "; " + age_requirement(rules.minimum_age), rules.trained_at,
        });
    }

    for (std::size_t value = 0; value < technology_count; ++value) {
        const auto technology = static_cast<Technology>(value);
        if (hidden_technology(technology)) continue;
        const TechnologyRules& rules = rules_for(technology);
        if (!producer_set.contains(rules.researched_at)) continue;
        const auto [x, y] = position(rules.researched_at, rules.minimum_age);
        TechnologyTreeNodeState state =
            civilization_has_technology(civilization, technology)
                ? TechnologyTreeNodeState::available
                : TechnologyTreeNodeState::disabled;
        if (std::ranges::find(researched, technology) != researched.end()) {
            state = TechnologyTreeNodeState::upgraded;
        }
        layout.nodes.push_back({
            TechnologyTreeNodeKind::technology, value,
            std::string{name(technology)}, rules.minimum_age, state, x, y,
            rules.wood_cost, rules.food_cost, rules.gold_cost,
            rules.stone_cost,
            std::string{"Research at "} +
                std::string{name(rules.researched_at)} + "; " +
                age_requirement(rules.minimum_age), rules.researched_at,
        });
    }

    std::set<std::size_t> has_predecessor;
    const auto connect = [&](auto first, auto second,
                             TechnologyTreeNodeKind kind) {
        const auto from = find_node(layout, kind, first);
        const auto to = find_node(layout, kind, second);
        if (from && to) {
            layout.dependencies.emplace_back(*from, *to);
            has_predecessor.insert(*to);
        }
    };
    for (const auto& [from, to] : unit_upgrades) {
        connect(from, to, TechnologyTreeNodeKind::unit);
    }
    for (const auto& [from, to] : technology_upgrades) {
        connect(from, to, TechnologyTreeNodeKind::technology);
    }
    for (const auto& [from, to] : building_upgrades) {
        connect(from, to, TechnologyTreeNodeKind::building);
    }

    for (std::size_t index = 0; index < layout.nodes.size(); ++index) {
        const TechnologyTreeNode& node = layout.nodes[index];
        if (node.kind == TechnologyTreeNodeKind::building ||
            has_predecessor.contains(index)) continue;
        const auto producer = find_node(
            layout, TechnologyTreeNodeKind::building, node.producer
        );
        if (producer && *producer != index) {
            layout.dependencies.emplace_back(*producer, index);
        }
    }

    int maximum_y = 0;
    for (const TechnologyTreeNode& node : layout.nodes) {
        maximum_y = std::max(maximum_y, node.y);
    }
    layout.width = 188 + 4 * age_width + node_width;
    layout.height = maximum_y + 88;
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
        const bool horizontal = direction == TechnologyTreeDirection::left ||
            direction == TechnologyTreeDirection::right;
        const long long primary = horizontal ? std::abs(dx) : std::abs(dy);
        const long long secondary = horizontal ? std::abs(dy) : std::abs(dx);
        const long long score = primary * 100000LL + secondary;
        if (score < best_score) {
            best_score = score;
            best = index;
        }
    }
    return best;
}

}  // namespace aoe

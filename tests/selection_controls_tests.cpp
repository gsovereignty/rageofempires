#include "aoe/selection_controls.hpp"

#include <iostream>
#include <limits>

namespace {
int failures{};
void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    expect(!aoe::exact_health_fill_pixels(0.0, 10),
           "zero health bar hidden");
    expect(!aoe::exact_health_fill_pixels(1.0, 0),
           "nonpositive maximum health bar hidden");
    expect(!aoe::exact_health_fill_pixels(
               std::numeric_limits<double>::infinity(), 10
           ),
           "non-finite current health bar hidden");
    expect(aoe::exact_health_fill_pixels(1.9, 10) == 3,
           "current HP truncates before inclusive 24-pixel scale");
    expect(aoe::exact_health_fill_pixels(5.0, 10) == 13,
           "half health uses inclusive endpoint");
    expect(aoe::exact_health_fill_pixels(10.0, 10) == 25,
           "full health fills exact 25-pixel bar");
    expect(aoe::exact_health_fill_pixels(99.0, 10) == 25,
           "over-maximum health clamps to exact bar width");

    std::vector<aoe::EntityId> selected{1, 2};
    aoe::toggle_selected_id(selected, 2);
    aoe::toggle_selected_id(selected, 3);
    expect(selected == std::vector<aoe::EntityId>({1, 3}),
           "shift toggle semantics");

    std::vector<aoe::Unit> units(4);
    units[0].id = 1; units[0].owner = aoe::Player::blue;
    units[0].kind = aoe::UnitKind::villager;
    units[1].id = 2; units[1].owner = aoe::Player::blue;
    units[1].kind = aoe::UnitKind::villager;
    units[2].id = 3; units[2].owner = aoe::Player::red;
    units[2].kind = aoe::UnitKind::villager;
    units[3].id = 4; units[3].owner = aoe::Player::blue;
    units[3].kind = aoe::UnitKind::sheep;
    const std::vector<std::uint8_t> visible{1, 0, 1, 1};
    const auto same = aoe::same_visible_owned_type(
        units, units[0], aoe::Player::blue, visible
    );
    expect(same == std::vector<aoe::EntityId>{1},
           "double-click visibility/ownership filter");
    const std::vector<aoe::EntityId> box{1, 3, 4};
    expect(
        aoe::filter_drag_selection(units, aoe::Player::blue, box) ==
            std::vector<aoe::EntityId>{1},
        "drag ownership/commandable filter"
    );
    aoe::SelectionControlGroup group{{1, 99}, 7};
    std::vector<aoe::Building> buildings(1);
    buildings[0].id = 7;
    buildings[0].owner = aoe::Player::red;
    aoe::prune_control_group(
        group, units, buildings, aoe::Player::blue
    );
    expect(group.units == std::vector<aoe::EntityId>{1} &&
           !group.building, "dead/enemy group IDs pruned");
    if (failures == 0) std::cout << "selection controls tests passed\n";
    return failures == 0 ? 0 : 1;
}

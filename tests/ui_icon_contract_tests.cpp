#include "aoe/ui_icon_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <utility>

namespace {

void executable_sheet_dispatch_is_bounded() {
    assert(!aoe::ui_icons::technology(-1));
    assert(aoe::ui_icons::technology(0)->sheet == 50729);
    assert(aoe::ui_icons::technology(117)->frame == 117);
    assert(!aoe::ui_icons::technology(118));

    assert(!aoe::ui_icons::ordinary_unit(-1));
    assert(aoe::ui_icons::ordinary_unit(0)->sheet == 50730);
    assert(aoe::ui_icons::ordinary_unit(133)->frame == 133);
    assert(!aoe::ui_icons::ordinary_unit(134));
}

void reconstruction_training_units_use_exact_dat_frames() {
    using aoe::UnitKind;
    constexpr std::array expected{
        std::pair{UnitKind::villager, 15},
        std::pair{UnitKind::knight, 1},
        std::pair{UnitKind::archer, 17},
        std::pair{UnitKind::scout_cavalry, 64},
        std::pair{UnitKind::militia, 8},
        std::pair{UnitKind::spearman, 31},
        std::pair{UnitKind::battering_ram, 74},
        std::pair{UnitKind::skirmisher, 20},
        std::pair{UnitKind::mangonel, 27},
        std::pair{UnitKind::monk, 33},
        std::pair{UnitKind::trade_cart, 34},
        std::pair{UnitKind::fishing_ship, 24},
        std::pair{UnitKind::galley, 87},
        std::pair{UnitKind::war_galley, 25},
        std::pair{UnitKind::galleon, 60},
        std::pair{UnitKind::transport_ship, 95},
        std::pair{UnitKind::scorpion, 80},
        std::pair{UnitKind::camel_rider, 78},
    };
    for (const auto [kind, frame] : expected) {
        const auto binding = aoe::ui_icons::training_unit(kind);
        assert(binding);
        assert(binding->sheet == aoe::ui_icons::unit_sheet);
        assert(binding->frame == frame);
        assert(
            binding->evidence ==
            aoe::ui_icons::Evidence::exact_executable_dispatch
        );
    }
}

void reconstruction_buildings_use_exact_dat_frames() {
    for (std::size_t value = 0;
         value < aoe::building_kind_count;
         ++value) {
        const auto binding = aoe::ui_icons::building(
            static_cast<aoe::BuildingKind>(value));
        assert(binding);
        assert(binding->sheet == aoe::ui_icons::building_sheet);
        assert(binding->frame >= 0 && binding->frame < 52);
        assert(
            binding->evidence ==
            aoe::ui_icons::Evidence::exact_executable_dispatch
        );
    }
    assert(
        aoe::ui_icons::building(aoe::BuildingKind::town_center)->frame == 28
    );
    assert(
        aoe::ui_icons::building(aoe::BuildingKind::farm)->frame == 35
    );
    assert(aoe::ui_icons::building(
        aoe::BuildingKind::guard_tower)->frame == 25);
    assert(aoe::ui_icons::building(
        aoe::BuildingKind::keep)->frame == 26);
    assert(aoe::ui_icons::building(
        aoe::BuildingKind::fortified_wall)->frame == 31);
    assert(aoe::ui_icons::building(
        aoe::BuildingKind::fortified_gate_x)->frame == 36);
}

}  // namespace

int main() {
    executable_sheet_dispatch_is_bounded();
    reconstruction_training_units_use_exact_dat_frames();
    reconstruction_buildings_use_exact_dat_frames();
}

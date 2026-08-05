#include "aoe/minimap_contract.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("minimap contract assertion failed");
    }
}

}  // namespace

#undef assert
#define assert(condition) require(condition)

int main() {
    using namespace aoe;
    using namespace aoe::minimap;

    assert(positive_floor(0.5) == 0);
    assert(positive_floor(1.5) == 1);
    assert(positive_floor(2.999) == 2);

    const auto square = build_scaling_rows(4, 4, 7);
    assert(square.size() == 7);
    assert(square[0] == ScalingRow(0, 0, 1));
    assert(square[3] == ScalingRow(3, 3, 4));
    assert(square[6] == ScalingRow(6, 6, 1));

    const auto asymmetric = build_scaling_rows(5, 3, 4);
    assert(asymmetric[0] == ScalingRow(0, 0, 1));
    assert(asymmetric[1] == ScalingRow(1, 1, 2));
    assert(asymmetric[2] == ScalingRow(2, 3, 3));
    assert(asymmetric[3] == ScalingRow(3, 5, 2));

    constexpr std::array<std::uint8_t, 8> expected_palette{
        242, 36, 241, 243, 251, 252, 132, 84,
    };
    assert(player_marker_palette_indices == expected_palette);
    constexpr std::array<std::array<std::uint8_t, 3>, 8> expected_rgb{{
        {0, 0, 255}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
        {0, 255, 255}, {255, 0, 255}, {185, 185, 185}, {255, 130, 1},
    }};
    assert(player_marker_rgb == expected_rgb);
    assert(size_one_marker_rect(10, 20) == InclusiveRect(9, 19, 11, 21));
    assert(readable_marker_rect(10, 20) ==
           InclusiveRect(8, 18, 12, 22));
    assert(type_0x112_signal_outline(10, 20) ==
           InclusiveRect(6, 16, 14, 24));

    assert(advance_type_0x112_signal_phase(false, 332) ==
           SignalPhase(false, false));
    assert(advance_type_0x112_signal_phase(false, 333) ==
           SignalPhase(true, true));
    assert(type_0x112_signal_palette(false, 70, 71) == 70);
    assert(type_0x112_signal_palette(true, 70, 71) == 71);

    assert(frame_1024_rect(1024, 768) ==
           InclusiveRect(688, 599, 1013, 762));
    assert(proved_viewport_bounds(100, 80, 101, 60, 0.5, 0.25) ==
           ViewportBounds(75, 65, 126, 96));
    assert(!viewport_scanline_polygon_proved);
    assert(!map640_anchor_proved);

    assert(shows_unit(MinimapMode::normal, UnitKind::villager));
    assert(!shows_unit(MinimapMode::combat, UnitKind::villager));
    assert(shows_unit(MinimapMode::combat, UnitKind::archer));
    assert(shows_unit(MinimapMode::economic, UnitKind::trade_cart));
    assert(!shows_unit(MinimapMode::economic, UnitKind::knight));
    constexpr std::array economic_units{
        UnitKind::villager, UnitKind::trade_cart, UnitKind::trade_cog,
        UnitKind::fishing_ship, UnitKind::sheep, UnitKind::deer,
        UnitKind::boar, UnitKind::relic,
    };
    for (std::size_t index = 0; index < unit_kind_count; ++index) {
        const UnitKind kind = static_cast<UnitKind>(index);
        const bool economic = std::ranges::find(economic_units, kind) !=
            economic_units.end();
        assert(shows_unit(MinimapMode::normal, kind));
        assert(shows_unit(MinimapMode::economic, kind) == economic);
        assert(shows_unit(MinimapMode::combat, kind) == !economic);
    }
    assert(shows_building(MinimapMode::combat, BuildingKind::castle));
    assert(!shows_building(MinimapMode::combat, BuildingKind::farm));
    assert(shows_building(MinimapMode::economic, BuildingKind::market));
    constexpr std::array economic_buildings{
        BuildingKind::town_center, BuildingKind::mill,
        BuildingKind::lumber_camp, BuildingKind::mining_camp,
        BuildingKind::farm, BuildingKind::market, BuildingKind::dock,
        BuildingKind::fish_trap,
    };
    for (std::size_t index = 0; index < building_kind_count; ++index) {
        const BuildingKind kind = static_cast<BuildingKind>(index);
        const bool economic = std::ranges::find(economic_buildings, kind) !=
            economic_buildings.end();
        assert(shows_building(MinimapMode::normal, kind));
        assert(shows_building(MinimapMode::economic, kind) == economic);
        assert(shows_building(MinimapMode::combat, kind) == !economic);
    }
    assert(highlights_resource(MinimapMode::economic, Terrain::gold_mine));
    assert(!highlights_resource(MinimapMode::normal, Terrain::gold_mine));
    assert(next_mode(MinimapMode::normal) == MinimapMode::combat);
    assert(next_mode(MinimapMode::combat) == MinimapMode::economic);
    assert(next_mode(MinimapMode::economic) == MinimapMode::normal);
    assert(std::string{mode_name(MinimapMode::combat)} == "COMBAT");

    MatchStatistics statistics;
    statistics.current_score[0] = 321;
    statistics.players[0].units_killed = 7;
    statistics.players[0].units_lost = 2;
    statistics.players[0].gathered = {10, 20, 30, 40};
    assert(statistics_summary(MinimapMode::normal, statistics).values[0] ==
           "321");
    assert(statistics_summary(MinimapMode::combat, statistics).values[0] ==
           "7/2");
    assert(statistics_summary(MinimapMode::economic, statistics).values[0] ==
           "100");

    bool rejected = false;
    try {
        static_cast<void>(build_scaling_rows(0, 4, 4));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

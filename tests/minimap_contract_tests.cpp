#include "aoe/minimap_contract.hpp"

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

    bool rejected = false;
    try {
        static_cast<void>(build_scaling_rows(0, 4, 4));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

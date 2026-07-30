#include "aoe/selection_visual_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main() {
    using namespace aoe::selection_visual;

    assert(dispatch(1, 0, false).front == Shape::cube);
    assert(dispatch(0, 1, false).back == Shape::cube);
    assert(dispatch(0, 2, false).front == Shape::square);
    assert(dispatch(0, 3, false).back == Shape::square);
    assert(dispatch(0, 0, false).front == Shape::unknown);
    const auto hardware = dispatch(0, 3, true);
    assert(hardware.hardware_path);
    assert(hardware.radii == RadiusSource::collision);

    assert(selected(1));
    assert(selected_overlay(1));
    assert(!selected_overlay(9));
    assert(selected_health(1, 0));
    assert(!selected_health(1, 2));

    assert(palette_index(0, false, 0) == 255);
    assert(palette_index(0, true, 0) == 133);
    assert(palette_index(9, false, 0) == 243);
    assert(palette_index(2, false, 0) == 36);
    assert(palette_index(2, false, 0x100) == 241);
    assert(palette_index(4, false, 0x100) == 243);

    assert(!group_number_frame(0));
    assert(group_number_frame(1) == 0);
    assert(group_number_frame(9) == 8);
    assert(!group_number_frame(10));
    assert(draw_group_numbers(1, 0x02, 3, 3));
    assert(!draw_group_numbers(1, 0, 3, 3));
    assert(!draw_group_numbers(1, 0x02, 2, 3));
    assert(!draw_group_numbers(9, 0x02, 3, 3));

    assert(square_back_segments + square_front_segments == 4);
    assert(cube_back_segments == 6);
    assert(cube_front_segments == 18);
    assert(!dat_shape_dispatch_proved);
    assert(!hover_visibility_policy_proved);
}

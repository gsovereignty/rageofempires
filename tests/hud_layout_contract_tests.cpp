#include "aoe/hud_layout_contract.hpp"
#include "aoe/ui_icon_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <string>

int main() {
    using namespace aoe::hud_layout;

    const auto plain = vertical_layout(1280, 1024, 40, 180, false);
    assert((plain.main_child == Rect{0, 40, 1280, 805}));
    assert(plain.bottom == 844);

    const auto with_top = vertical_layout(1024, 768, 20, 160, true);
    assert(with_top.top_child);
    assert((*with_top.top_child == Rect{0, 20, 1024, 30}));
    assert((with_top.main_child == Rect{0, 50, 1024, 559}));
    assert(!plain.top_child);

    assert((command_button(844, 0) == Rect{37, 875, 40, 40}));
    assert((command_button(844, 4) == Rect{201, 875, 40, 40}));
    assert((command_button(844, 14) == Rect{201, 957, 40, 40}));
    assert((command_button(844, 15) == Rect{}));
    for (int index = 0; index < 15; ++index) {
        const Rect button = command_button(844, index);
        assert(command_button_at(844, button.x, button.y) == index);
        assert(command_button_at(
            844,
            button.x + button.width - 1,
            button.y + button.height - 1
        ) == index);
    }
    assert(command_button_at(844, 77, 875) == -1);

    assert((anchored_large_panel(1280, 1024) ==
           Rect{944, 855, 326, 164}));
    assert((anchored_large_panel(1024, 768) ==
           Rect{688, 599, 326, 164}));
    assert((top_status_strip() == Rect{2, 2, 420, 16}));
    assert((centered_top_control(1280) == Rect{485, 16, 310, 20}));
    assert((top_right_control(1280, 0) == Rect{1020, 3, 50, 19}));
    assert((top_right_control(1280, 4) == Rect{1220, 3, 50, 19}));
    assert((bottom_right_control(1280, 1024, 0) ==
           Rect{972, 870, 35, 35}));
    assert((bottom_right_control(1280, 1024, 1) ==
           Rect{971, 975, 35, 35}));
    assert((bottom_right_control(1280, 1024, 2) ==
           Rect{1184, 868, 25, 25}));
    assert((bottom_right_control(1280, 1024, 7) ==
           Rect{1178, 985, 25, 25}));

    assert((absolute_layout(1280, 1024)->main_child ==
           Rect{0, 0, 1280, 850}));
    assert(absolute_layout(1024, 768)->bottom == 593);
    assert(absolute_layout(640, 480)->bottom == 305);
    assert(!absolute_layout(640, 174));

    assert(civilization_file_name(aoe::Civilization::britons) ==
           "game_b1.slp");
    assert(civilization_file_name(aoe::Civilization::mayans) ==
           "game_b18.slp");
    assert(civilization_file_name(aoe::Civilization::generic) ==
           "game_b1.slp");
    for (int civilization = 1; civilization <= 18; ++civilization) {
        const auto value =
            static_cast<aoe::Civilization>(civilization);
        assert(civilization_file_index(value) == civilization);
        assert(civilization_file_name(value) ==
               "game_b" + std::to_string(civilization) + ".slp");
    }

    const std::array<FrameMetrics, 8> frames{{
        {32, 32, 0, 0},
        {325, 175, 0, 0},
        {34, 175, 0, 0},
        {34, 175, 0, 0},
        {391, 175, 0, 0},
        {239, 130, 0, 0},
        {392, 25, -2, -1},
        {165, 21, -2, -5},
    }};
    const Rect sibling =
        frame7_sibling_view(1280, frames[6], frames[7]);
    assert((sibling == Rect{624, 6, 165, 20}));
    const auto draws = background_composition(
        1280, 1024, sibling.x, sibling.width, frames
    );
    assert(draws.size() == 64);
    assert((draws.front() == BackgroundDraw{0, 0, 0}));
    assert((draws[39] == BackgroundDraw{0, 1248, 0}));
    assert((draws[40] == BackgroundDraw{6, 0, 0}));
    assert((draws[41] == BackgroundDraw{2, 291, 849}));
    assert(std::count_if(
        draws.begin(), draws.end(),
        [](const BackgroundDraw& draw) { return draw.frame == 3; }
    ) == 4);
    assert((draws[60] == BackgroundDraw{1, 0, 849}));
    assert((draws[61] == BackgroundDraw{4, 889, 849}));
    assert((draws[62] == BackgroundDraw{5, 488, 784}));
    assert((draws[63] == BackgroundDraw{7, 624, 0}));
    auto missing_frames = frames;
    missing_frames[0].width = 0;
    assert(background_composition(
        1280, 1024, sibling.x, sibling.width, missing_frames
    ).empty());
}

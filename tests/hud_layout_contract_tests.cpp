#include "aoe/hud_layout_contract.hpp"
#include "aoe/ui_icon_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

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

    assert((anchored_large_panel(1280, 1024) ==
           Rect{944, 855, 326, 164}));
    assert((anchored_large_panel(1024, 768) ==
           Rect{688, 599, 326, 164}));
    assert((top_status_strip() == Rect{2, 2, 420, 16}));
    assert((centered_top_control(1280) == Rect{485, 16, 310, 20}));
    assert((top_right_control(1280, 0) == Rect{1020, 3, 50, 19}));
    assert((top_right_control(1280, 4) == Rect{1220, 3, 50, 19}));

    assert(!absolute_layout(1280, 1024));
    assert(!absolute_layout(1024, 768));
    assert(!absolute_layout(640, 480));

    const auto normal = aoe::ui_icons::button_visual(false, 73);
    assert(normal.chrome_frame == 36);
    assert(normal.icon_frame == 73);
    assert(normal.icon_offset_x == 0 && normal.icon_offset_y == 0);
    const auto pressed = aoe::ui_icons::button_visual(true, 73);
    assert(pressed.chrome_frame == 37);
    assert(pressed.icon_frame == 73);
    assert(pressed.icon_offset_x == 1 && pressed.icon_offset_y == 1);
}

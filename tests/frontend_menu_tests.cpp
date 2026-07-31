#include "aoe/frontend_menu.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>

int main() {
    using namespace aoe;

    assert(main_menu_items().size() == 8);
    assert(single_player_menu_items().size() == 7);
    assert(main_menu_items()[1].bounds ==
        (FrontendMenuRect{311, 12, 114, 38}));
    assert(single_player_menu_items()[0].bounds ==
        (FrontendMenuRect{476, 89, 260, 39}));

    const auto exact = frontend_logical_transform(800, 600);
    assert(exact.scale == 1.0F);
    assert(exact.offset_x == 0.0F && exact.offset_y == 0.0F);

    const auto wide = frontend_logical_transform(1920, 1080);
    assert(std::abs(wide.scale - 1.8F) < 0.001F);
    assert(std::abs(wide.offset_x - 240.0F) < 0.001F);
    const auto logical = wide.window_to_logical(
        240.0F + 311.0F * 1.8F,
        12.0F * 1.8F
    );
    assert(logical && std::abs((*logical)[0] - 311.0F) < 0.001F);
    assert(!wide.window_to_logical(10.0F, 10.0F));

    const auto ultrawide = frontend_logical_transform(3440, 1440);
    assert(std::abs(ultrawide.scale - 2.4F) < 0.001F);
    assert(std::abs(ultrawide.offset_x - 760.0F) < 0.001F);

    assert(frontend_menu_hit_test(
        FrontendMenuScreen::main_menu, 330, 30
    ) == 1);
    assert(frontend_menu_hit_test(
        FrontendMenuScreen::single_player_menu, 500, 100
    ) == 0);
    assert(!frontend_menu_hit_test(
        FrontendMenuScreen::single_player_menu, 400, 100
    ));

    assert(move_frontend_menu_focus(
        FrontendMenuScreen::main_menu, 0, -1
    ) == 7);
    assert(move_frontend_menu_focus(
        FrontendMenuScreen::main_menu, 5, 1
    ) == 6);
    assert(move_frontend_menu_focus(
        FrontendMenuScreen::single_player_menu, 6, 1
    ) == 0);

    const auto single = activate_frontend_menu_item(
        FrontendMenuScreen::main_menu, 1
    );
    assert(single.activate);
    assert(single.screen == FrontendMenuScreen::single_player_menu);
    assert(single.command == FrontendMenuCommand::open_single_player);

    const auto regicide = activate_frontend_menu_item(
        FrontendMenuScreen::single_player_menu, 2
    );
    assert(!regicide.activate);

    const auto death_match = activate_frontend_menu_item(
        FrontendMenuScreen::single_player_menu, 3
    );
    assert(!death_match.activate);

    const auto disabled = activate_frontend_menu_item(
        FrontendMenuScreen::main_menu, 0
    );
    assert(!disabled.activate);

    const auto closed = close_frontend_menu(
        FrontendMenuScreen::single_player_menu
    );
    assert(closed.activate);
    assert(closed.screen == FrontendMenuScreen::main_menu);
}

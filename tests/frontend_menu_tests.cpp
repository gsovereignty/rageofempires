#include "aoe/frontend_menu.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>

int main() {
    using namespace aoe;

    assert(main_menu_items().size() == 9);
    const auto native = native_main_menu_controls();
    assert(native.size() == 7);
    assert(native[0].bounds == (FrontendMenuRect{532, 9, 192, 258}));
    assert(native[0].label_bounds == (FrontendMenuRect{542, 20, 178, 38}));
    assert(native[0].first_frame == 10);
    assert(native[0].label_string_id == 9500);
    assert(native[0].help_string_id == 31000);
    assert(native[1].bounds == (FrontendMenuRect{495, 265, 161, 188}));
    assert(native[1].label_bounds == (FrontendMenuRect{502, 284, 145, 23}));
    assert(native[1].first_frame == 14);
    assert(native[1].label_string_id == 9501);
    assert(native[2].first_frame == 22);
    assert(native[2].label_bounds == (FrontendMenuRect{150, 13, 188, 40}));
    assert(native[2].label_string_id == 9503);
    assert(native[3].first_frame == 26);
    assert(native[3].label_bounds == (FrontendMenuRect{420, 355, 107, 18}));
    assert(native[3].label_string_id == 9504);
    assert(native[4].first_frame == 30);
    assert(native[4].label_bounds == (FrontendMenuRect{304, 213, 128, 32}));
    assert(native[4].label_string_id == 9505);
    assert(native[5].first_frame == 34);
    assert(native[5].label_bounds == (FrontendMenuRect{304, 450, 117, 24}));
    assert(native[5].label_string_id == 9506);
    assert(native[6].bounds == (FrontendMenuRect{174, 631, 230, 137}));
    assert(native[6].label_bounds == (FrontendMenuRect{200, 704, 160, 26}));
    assert(native[6].first_frame == 46);
    assert(native[6].label_string_id == 9509);
    const auto& font = native_frontend_font();
    assert(font.family == "Lucida Blackletter");
    assert(font.height == 14);
    assert(font.raster_height == 19);
    assert(font.weight == 400 && !font.italic);
    assert(font.normal_color == (std::array<std::uint8_t, 3>{217, 208, 176}));
    assert(font.shadow_color == (std::array<std::uint8_t, 3>{0, 0, 0}));
    assert(font.focus_color == (std::array<std::uint8_t, 3>{202, 207, 1}));
    assert(font.disabled_color == (std::array<std::uint8_t, 3>{255, 255, 255}));
    assert(font.horizontal_alignment == 2);
    assert(font.shadow_offset_x == 1 && font.shadow_offset_y == 1);

    std::vector<FrontendHitMask> masks;
    for (const auto& control : native) {
        FrontendHitMask mask;
        mask.width = static_cast<int>(control.bounds.width);
        mask.height = static_cast<int>(control.bounds.height);
        mask.opaque.assign(
            static_cast<std::size_t>(mask.width * mask.height), 0
        );
        masks.push_back(std::move(mask));
    }
    masks[0].opaque[10 * masks[0].width + 20] = 1;
    assert(native_frontend_hit_test(native, masks, 552, 19) == 0);
    assert(!native_frontend_hit_test(native, masks, 533, 10));
    assert(!native_frontend_hit_test(
        native, std::span<const FrontendHitMask>{masks}.first(6), 552, 19
    ));
    assert(single_player_menu_items().size() == 7);
    assert(ai_arabia_size_menu_items().size() == 6);
    assert(main_menu_items()[1].bounds ==
        (FrontendMenuRect{42, 154, 250, 42}));
    for (std::size_t index = 0; index < main_menu_items().size(); ++index) {
        const auto& bounds = main_menu_items()[index].bounds;
        assert(bounds.x == 42);
        assert(bounds.width == 250);
        assert(bounds.height == 42);
        if (index > 0) {
            assert(bounds.y == main_menu_items()[index - 1].bounds.y + 50);
        }
    }
    assert(single_player_menu_items()[0].bounds ==
        (FrontendMenuRect{476, 89, 260, 39}));
    assert(ai_arabia_size_menu_items()[0].command ==
        FrontendMenuCommand::launch_ai_arabia_tiny);
    assert(ai_arabia_size_menu_items()[3].command ==
        FrontendMenuCommand::launch_ai_arabia_normal);
    assert(ai_arabia_size_menu_items()[4].command ==
        FrontendMenuCommand::launch_ai_arabia_large);
    assert(ai_arabia_size_menu_items()[5].command ==
        FrontendMenuCommand::launch_ai_arabia_giant);

    const auto exact = frontend_logical_transform(800, 600);
    assert(exact.scale == 1.0F);
    assert(exact.offset_x == 0.0F && exact.offset_y == 0.0F);

    const auto wide = frontend_logical_transform(1920, 1080);
    assert(std::abs(wide.scale - 2.4F) < 0.001F);
    assert(std::abs(wide.offset_y + 180.0F) < 0.001F);
    const auto logical = wide.window_to_logical(
        42.0F * 2.4F,
        -180.0F + 154.0F * 2.4F
    );
    assert(logical && std::abs((*logical)[0] - 42.0F) < 0.001F);
    const auto covered_corner = wide.window_to_logical(10.0F, 10.0F);
    assert(covered_corner);
    assert((*covered_corner)[0] >= 0.0F && (*covered_corner)[1] >= 0.0F);

    const auto ultrawide = frontend_logical_transform(3440, 1440);
    assert(std::abs(ultrawide.scale - 4.3F) < 0.001F);
    assert(std::abs(ultrawide.offset_y + 570.0F) < 0.001F);

    const auto native_exact = native_frontend_logical_transform(1366, 768);
    assert(native_exact.scale == 1.0F);
    assert(native_exact.offset_x == 0.0F && native_exact.offset_y == 0.0F);
    assert(native_exact.logical_width == 1366);
    assert(native_exact.logical_height == 768);
    const auto native_four_three =
        native_frontend_logical_transform(1024, 768);
    assert(native_four_three.scale > 0.749F &&
           native_four_three.scale < 0.750F);
    assert(std::abs(native_four_three.offset_y - 96.0F) < 0.2F);
    assert(!native_four_three.window_to_logical(512, 40));
    assert(native_four_three.window_to_logical(512, 384));

    assert(frontend_menu_hit_test(
        FrontendMenuScreen::main_menu, 100, 170
    ) == 1);
    assert(frontend_menu_hit_test(
        FrontendMenuScreen::single_player_menu, 500, 100
    ) == 0);
    assert(!frontend_menu_hit_test(
        FrontendMenuScreen::single_player_menu, 400, 100
    ));

    assert(move_frontend_menu_focus(
        FrontendMenuScreen::main_menu, 0, -1
    ) == 8);
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

    const auto ai_arabia = activate_frontend_menu_item(
        FrontendMenuScreen::main_menu, 2
    );
    assert(ai_arabia.activate);
    assert(ai_arabia.screen == FrontendMenuScreen::ai_arabia_size_menu);
    assert(ai_arabia.command == FrontendMenuCommand::open_ai_arabia);

    const auto tiny = activate_frontend_menu_item(
        FrontendMenuScreen::ai_arabia_size_menu, 0
    );
    assert(tiny.activate);
    assert(tiny.command == FrontendMenuCommand::launch_ai_arabia_tiny);

    const auto regicide = activate_frontend_menu_item(
        FrontendMenuScreen::single_player_menu, 2
    );
    assert(regicide.activate);
    assert(regicide.screen == FrontendMenuScreen::random_map_setup);

    const auto death_match = activate_frontend_menu_item(
        FrontendMenuScreen::single_player_menu, 3
    );
    assert(death_match.activate);
    assert(death_match.screen == FrontendMenuScreen::random_map_setup);

    const auto learn = activate_frontend_menu_item(
        FrontendMenuScreen::main_menu, 0
    );
    assert(learn.activate);
    assert(learn.command == FrontendMenuCommand::learn_to_play);

    const auto closed = close_frontend_menu(
        FrontendMenuScreen::single_player_menu
    );
    assert(closed.activate);
    assert(closed.screen == FrontendMenuScreen::main_menu);
    assert(close_frontend_menu(
        FrontendMenuScreen::ai_arabia_size_menu
    ).screen == FrontendMenuScreen::main_menu);
}

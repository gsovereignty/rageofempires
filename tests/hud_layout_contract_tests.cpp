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
    for (const int width : {640, 800, 1024, 1280, 1920}) {
        for (const bool icons : {false, true}) {
            const ResourceStatusLayout layout =
                resource_status_layout(width, icons);
            assert(layout.row.x == 10);
            assert(layout.row.width <= 780);
            assert(layout.row.x >= 0);
            assert(layout.row.x + layout.row.width <= width);
            assert(layout.left_safe_margin >= 10);
            assert(layout.right_safe_margin >= 10);
            assert(layout.text_baseline == 8);
            for (std::size_t index = 0;
                 index < layout.fields.size();
                 ++index) {
                const ResourceFieldLayout& field =
                    layout.fields[index];
                assert(field.bounds.width >= 0);
                assert(field.bounds.height >= 0);
                assert(field.bounds.x >= layout.row.x);
                assert(field.bounds.x + field.bounds.width <=
                       layout.row.x + layout.row.width);
                assert(field.text.x >= field.bounds.x);
                assert(field.text.x + field.text.width <=
                       field.bounds.x + field.bounds.width);
                if (index > 0) {
                    const Rect& previous =
                        layout.fields[index - 1].bounds;
                    assert(previous.x + previous.width +
                               layout.gap ==
                           field.bounds.x);
                    assert(previous.x + previous.width <
                           field.bounds.x);
                }
                if (field.icon) {
                    assert(index < 4);
                    assert(field.icon->width == 16);
                    assert(field.icon->height == 16);
                    assert(field.icon->x >= field.bounds.x);
                    assert(field.icon->x + field.icon->width <=
                           field.bounds.x + field.bounds.width);
                    assert(field.text.x ==
                           field.icon->x + field.icon->width + 4);
                } else {
                    assert(field.text.x == field.bounds.x);
                }
                for (const std::string source : {
                         "WOOD 200",
                         "FOOD 999999999",
                         "RESSOURCEENBOIS 999999999",
                     }) {
                    const std::string text =
                        truncate_debug_text(
                            source, field.text.width
                        );
                    assert(
                        static_cast<int>(text.size()) *
                            debug_glyph_width <=
                        field.text.width
                    );
                }
            }
            const Rect& stone = layout.fields[3].bounds;
            const Rect& population = layout.fields[4].bounds;
            assert(stone.x + stone.width < population.x);
            assert(population.x + population.width <= width - 10);
            if (width >= 1024) {
                assert(layout.row.width == 780);
            }
        }
    }
    const ResourceStatusLayout remainder =
        resource_status_layout(641, true);
    assert(remainder.fields.front().bounds.width ==
           remainder.fields[2].bounds.width + 1);
    assert(remainder.fields.back().bounds.x +
               remainder.fields.back().bounds.width ==
           remainder.row.x + remainder.row.width);
    assert((inset(Rect{0, 0, 100, 40}, 6) == Rect{6, 6, 88, 28}));
    assert((inset(Rect{0, 0, 8, 8}, 6) == Rect{6, 6, 0, 0}));
    assert(information_content_x >= 255 + 23 + debug_glyph_width);
    assert(truncate_debug_text("VILLAGER", 64) == "VILLAGER");
    assert(truncate_debug_text("VILLAGER", 56) == "VILL...");
    assert(truncate_debug_text("VILLAGER", 16).empty());
    assert(population_status_text(3, 5, 1, 0, false, 160) ==
           "POP 3/5 IDLE 1/0");
    assert(population_status_text(3, 5, 1, 0, true, 112) ==
           "POP 3/5 PAUSED");
    assert(population_status_text(
               999999, 999999, 9999, 9999, true, 120
           ).size() * debug_glyph_width <= 120);
    constexpr std::array screen_widths{640, 1024, 1280};
    for (const int screen_width : screen_widths) {
        constexpr int left_margin = 10;
        constexpr int right_margin = 10;
        const std::string bounded = truncate_debug_text(
            std::string(300, 'X'),
            screen_width - left_margin - right_margin
        );
        assert(left_margin >= 8);
        assert(
            left_margin + static_cast<int>(bounded.size()) * 8 <=
            screen_width - right_margin
        );
    }
    assert((contain(40, 80, {10.0F, 20.0F, 60.0F, 60.0F}) ==
            FloatRect{25.0F, 20.0F, 30.0F, 60.0F}));
    assert((contain(0, 80, {10.0F, 20.0F, 60.0F, 60.0F}) ==
            FloatRect{}));
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

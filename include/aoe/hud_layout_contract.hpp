#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace aoe::hud_layout {

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};

    auto operator<=>(const Rect&) const = default;
};

struct VerticalLayout {
    std::optional<Rect> top_child{};
    Rect main_child{};
    int bottom{};

    auto operator<=>(const VerticalLayout&) const = default;
};

// FUN_005f37c0 operands. Inputs are stored screen fields; their producers and
// semantic resolution names are not recovered.
[[nodiscard]] constexpr VerticalLayout vertical_layout(
    int screen_width,
    int screen_height,
    int stored_top,
    int stored_bottom_height,
    bool top_child_visible
) {
    const int top_after_child = stored_top + (top_child_visible ? 30 : 0);
    const int bottom = screen_height - stored_bottom_height;
    return {
        top_child_visible
            ? std::optional<Rect>{{0, stored_top, screen_width, 30}}
            : std::nullopt,
        {0, top_after_child, screen_width, bottom - top_after_child + 1},
        bottom,
    };
}

[[nodiscard]] constexpr Rect command_button(int bottom, int index) {
    if (index < 0 || index >= 15) return {};
    return {
        37 + 41 * (index % 5),
        bottom + 31 + 41 * (index / 5),
        40,
        40,
    };
}

[[nodiscard]] constexpr Rect anchored_large_panel(
    int screen_width,
    int screen_height
) {
    return {screen_width - 336, screen_height - 169, 326, 164};
}

[[nodiscard]] constexpr Rect top_status_strip() {
    return {2, 2, 420, 16};
}

[[nodiscard]] constexpr Rect centered_top_control(int screen_width) {
    return {screen_width / 2 - 155, 16, 310, 20};
}

[[nodiscard]] constexpr Rect top_right_control(
    int screen_width,
    int index
) {
    if (index < 0 || index >= 5) return {};
    return {screen_width - 260 + 50 * index, 3, 50, 19};
}

// Absolute split, panel roles, and width-to-layout-class mapping stay unknown.
[[nodiscard]] constexpr std::optional<VerticalLayout> absolute_layout(
    int,
    int
) {
    return std::nullopt;
}

inline constexpr bool loose_game_background_metrics_available = false;
inline constexpr bool resolution_class_mapping_proved = false;
inline constexpr bool panel_roles_proved = false;

}  // namespace aoe::hud_layout

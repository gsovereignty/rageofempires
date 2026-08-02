#pragma once

#include <compare>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aoe/types.hpp"

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

struct FrameMetrics {
    int width{};
    int height{};
    int hotspot_x{};
    int hotspot_y{};

    auto operator<=>(const FrameMetrics&) const = default;
};

struct FloatRect {
    float x{};
    float y{};
    float width{};
    float height{};

    auto operator<=>(const FloatRect&) const = default;
};

struct BackgroundDraw {
    int frame{};
    int anchor_x{};
    int anchor_y{};

    auto operator<=>(const BackgroundDraw&) const = default;
};

struct ResourceFieldLayout {
    Rect bounds{};
    std::optional<Rect> icon{};
    Rect text{};

    auto operator<=>(const ResourceFieldLayout&) const = default;
};

struct ResourceStatusLayout {
    Rect row{};
    std::array<ResourceFieldLayout, 5> fields{};
    int gap{};
    int left_safe_margin{};
    int right_safe_margin{};
    int text_baseline{};

    auto operator<=>(const ResourceStatusLayout&) const = default;
};

inline constexpr int game_background_frame_count = 8;
inline constexpr int debug_glyph_width = 8;
// Original HUD skin's left divider extends past nominal information-panel
// edge. Keep text and portraits beyond divider plus one debug-glyph margin.
inline constexpr int information_content_x = 286;

[[nodiscard]] constexpr int civilization_file_index(
    Civilization civilization
) {
    const int index = static_cast<int>(civilization);
    return index >= 1 && index <= 18 ? index : 1;
}

[[nodiscard]] constexpr std::string_view civilization_file_name(
    Civilization civilization
) {
    constexpr std::array<std::string_view, 19> names{
        "game_b1.slp",
        "game_b1.slp", "game_b2.slp", "game_b3.slp",
        "game_b4.slp", "game_b5.slp", "game_b6.slp",
        "game_b7.slp", "game_b8.slp", "game_b9.slp",
        "game_b10.slp", "game_b11.slp", "game_b12.slp",
        "game_b13.slp", "game_b14.slp", "game_b15.slp",
        "game_b16.slp", "game_b17.slp", "game_b18.slp",
    };
    return names[static_cast<std::size_t>(
        civilization_file_index(civilization)
    )];
}

// Exact FUN_005e7cb0 call order and operands. Coordinates are SLP draw
// anchors; callers apply each frame hotspot when producing its destination.
[[nodiscard]] inline std::vector<BackgroundDraw> background_composition(
    int screen_width,
    int screen_height,
    int sibling_x,
    int sibling_width,
    const std::array<FrameMetrics, game_background_frame_count>& frames
) {
    std::vector<BackgroundDraw> draws;
    if (screen_width <= 0 || screen_height <= 0 ||
        frames[0].width <= 0 || frames[1].width <= 0 ||
        frames[2].width <= 0 || frames[3].width <= 0 ||
        frames[4].width <= 0) {
        return draws;
    }
    for (int x = 0; x < screen_width; x += frames[0].width) {
        draws.push_back({0, x, 0});
    }
    draws.push_back({6, 0, 0});

    const int bottom = screen_height - frames[1].height;
    const int right_cap_x = screen_width - frames[4].width;
    int x = frames[1].width - frames[2].width;
    unsigned index = 0;
    while (x < frames[2].width + right_cap_x) {
        const int frame = index % 4 == 3 ? 3 : 2;
        draws.push_back({frame, x, bottom});
        x += frames[static_cast<std::size_t>(frame)].width;
        ++index;
    }
    draws.push_back({1, 0, bottom});
    draws.push_back({4, right_cap_x, bottom});
    draws.push_back({
        5,
        ((right_cap_x - frames[1].width) / 2 -
         frames[5].width / 2) + frames[1].width,
        bottom - frames[5].height / 2,
    });
    draws.push_back({
        7,
        sibling_x - (frames[7].width - sibling_width) / 2,
        0,
    });
    return draws;
}

// FUN_005f37c0 positions the sibling used by FUN_005e7cb0 frame 7 between
// frame 6's right edge and the 260-pixel top-right control zone.
[[nodiscard]] constexpr Rect frame7_sibling_view(
    int screen_width,
    const FrameMetrics& frame6,
    const FrameMetrics& frame7
) {
    return {
        ((screen_width - frame6.width - 260) / 2 -
         frame7.width / 2) + frame6.width,
        6,
        frame7.width,
        20,
    };
}

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
        34 + 42 * (index % 5),
        bottom + 31 + 42 * (index / 5),
        38,
        38,
    };
}

[[nodiscard]] constexpr int command_button_at(
    int bottom,
    int x,
    int y
) {
    for (int index = 0; index < 15; ++index) {
        const Rect button = command_button(bottom, index);
        if (x >= button.x && x < button.x + button.width &&
            y >= button.y && y < button.y + button.height) {
            return index;
        }
    }
    return -1;
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

// Reconstruction-native responsive resource row. Exact commercial field
// positions are unproved. Width is capped so wide screens retain one compact
// status group; integer remainder pixels are assigned from left to right.
[[nodiscard]] constexpr ResourceStatusLayout resource_status_layout(
    int screen_width,
    bool icons_available
) {
    constexpr int margin = 10;
    constexpr int gap = 6;
    constexpr int maximum_row_width = 780;
    constexpr int row_y = 3;
    constexpr int row_height = 18;
    constexpr int icon_size = 16;
    constexpr int icon_text_gap = 4;
    ResourceStatusLayout result{};
    const int row_width = std::min(
        maximum_row_width,
        std::max(0, screen_width - margin * 2)
    );
    result.row = {margin, row_y, row_width, row_height};
    result.gap = gap;
    result.left_safe_margin = margin;
    result.right_safe_margin =
        std::max(margin, screen_width - margin - row_width);
    result.text_baseline = 8;
    const int available = std::max(0, row_width - gap * 4);
    const int field_width = available / 5;
    const int remainder = available % 5;
    int x = margin;
    for (std::size_t index = 0; index < result.fields.size(); ++index) {
        ResourceFieldLayout& field = result.fields[index];
        const int width =
            field_width + (static_cast<int>(index) < remainder ? 1 : 0);
        field.bounds = {x, row_y, width, row_height};
        const bool has_icon =
            icons_available && index < 4 && width >= icon_size;
        if (has_icon) {
            field.icon = Rect{x, row_y, icon_size, icon_size};
        }
        const int text_x =
            x + (has_icon ? icon_size + icon_text_gap : 0);
        field.text = {
            text_x,
            result.text_baseline,
            std::max(0, x + width - text_x),
            debug_glyph_width,
        };
        x += width + gap;
    }
    return result;
}

[[nodiscard]] constexpr Rect inset(Rect rect, int margin) {
    const int safe_margin = std::max(0, margin);
    return {
        rect.x + safe_margin,
        rect.y + safe_margin,
        std::max(0, rect.width - safe_margin * 2),
        std::max(0, rect.height - safe_margin * 2),
    };
}

[[nodiscard]] inline std::string truncate_debug_text(
    std::string_view text,
    int pixel_width
) {
    const std::size_t capacity = pixel_width > 0
        ? static_cast<std::size_t>(pixel_width / debug_glyph_width)
        : 0U;
    if (text.size() <= capacity) return std::string{text};
    if (capacity < 4U) return {};
    return std::string{text.substr(0, capacity - 3U)} + "...";
}

[[nodiscard]] inline std::string population_status_text(
    int population,
    int capacity,
    int idle_villagers,
    int idle_military,
    bool paused,
    int pixel_width
) {
    const std::string primary =
        "POP " + std::to_string(population) + "/" +
        std::to_string(capacity);
    if (static_cast<int>(primary.size()) * debug_glyph_width >
        pixel_width) {
        return truncate_debug_text(primary, pixel_width);
    }
    std::string result = primary;
    const auto append_if_fits = [&result, pixel_width](
        const std::string& optional
    ) {
        if (static_cast<int>(result.size() + optional.size()) *
                debug_glyph_width <=
            pixel_width) {
            result += optional;
        }
    };
    if (paused) append_if_fits(" PAUSED");
    append_if_fits(
        " IDLE " + std::to_string(idle_villagers) + "/" +
        std::to_string(idle_military)
    );
    return result;
}

[[nodiscard]] constexpr FloatRect contain(
    int source_width,
    int source_height,
    FloatRect bounds
) {
    if (source_width <= 0 || source_height <= 0 ||
        bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return {};
    }
    const float scale = std::min(
        bounds.width / static_cast<float>(source_width),
        bounds.height / static_cast<float>(source_height)
    );
    const float width = static_cast<float>(source_width) * scale;
    const float height = static_cast<float>(source_height) * scale;
    return {
        bounds.x + (bounds.width - width) * 0.5F,
        bounds.y + (bounds.height - height) * 0.5F,
        width,
        height,
    };
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

[[nodiscard]] constexpr Rect bottom_right_control(
    int screen_width,
    int screen_height,
    int index
) {
    constexpr std::array<Rect, 8> offsets{{
        {-308, -154, 35, 35},
        {-309, -49, 35, 35},
        {-96, -156, 25, 25},
        {-69, -162, 25, 25},
        {-60, -137, 25, 25},
        {-61, -59, 25, 25},
        {-74, -35, 25, 25},
        {-102, -39, 25, 25},
    }};
    if (index < 0 || index >= static_cast<int>(offsets.size())) {
        return {};
    }
    const Rect value = offsets[static_cast<std::size_t>(index)];
    return {
        screen_width + value.x,
        screen_height + value.y,
        value.width,
        value.height,
    };
}

[[nodiscard]] constexpr int background_bottom(
    int screen_height,
    const FrameMetrics& left_cap
) {
    return screen_height - left_cap.height;
}

// Every recovered game_b file has a 175-pixel frame-1 left cap. This proves
// the compositor's absolute bottom-band start for any current screen height.
[[nodiscard]] constexpr std::optional<VerticalLayout> absolute_layout(
    int screen_width,
    int screen_height
) {
    if (screen_width <= 0 || screen_height < 175) return std::nullopt;
    return vertical_layout(screen_width, screen_height, 0, 175, false);
}

inline constexpr bool loose_game_background_metrics_available = true;
inline constexpr bool resolution_class_mapping_proved = false;
inline constexpr bool panel_roles_proved = false;

}  // namespace aoe::hud_layout

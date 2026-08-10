#include "aoe/window_mode.hpp"

#include <algorithm>

namespace aoe {

EdgeScrollDirections edge_scroll_directions(
    float pointer_x,
    float pointer_y,
    int window_width,
    int window_height,
    float edge_margin
) {
    if (window_width <= 0 || window_height <= 0 || edge_margin < 0.0F ||
        pointer_x < 0.0F || pointer_y < 0.0F ||
        pointer_x > static_cast<float>(window_width) ||
        pointer_y > static_cast<float>(window_height)) {
        return {};
    }
    return {
        pointer_x <= edge_margin,
        pointer_x >= static_cast<float>(window_width) - edge_margin,
        pointer_y <= edge_margin,
        pointer_y >= static_cast<float>(window_height) - edge_margin,
    };
}

std::optional<RenderExtent> render_extent_for_window(
    int window_width,
    int window_height,
    int drawable_width,
    int drawable_height,
    int hud_height
) {
    if (window_width <= 0 || window_height <= hud_height ||
        drawable_width <= 0 || drawable_height <= 0 || hud_height < 0) {
        return std::nullopt;
    }
    return RenderExtent{
        window_width,
        window_height,
        window_height - hud_height,
    };
}

std::vector<DisplayMode> supported_display_modes(
    std::span<const DisplayMode> reported
) {
    std::vector<DisplayMode> result;
    for (const auto mode : reported) {
        if (mode.width < 800 || mode.height < 600) continue;
        if (std::ranges::find(result, mode) == result.end()) {
            result.push_back(mode);
        }
    }
    std::ranges::sort(result, {}, [](const DisplayMode& mode) {
        return std::pair{mode.width, mode.height};
    });
    return result;
}

std::optional<RenderExtent> fixed_canvas_extent(
    int canvas_width,
    int canvas_height,
    int drawable_width,
    int drawable_height,
    int hud_height
) {
    if (canvas_width < 800 || canvas_height <= hud_height ||
        drawable_width <= 0 || drawable_height <= 0) return std::nullopt;
    return RenderExtent{canvas_width, canvas_height, canvas_height - hud_height};
}

WindowModeState window_mode_result(
    const WindowModeState& before,
    bool requested_fullscreen,
    bool transition_succeeded,
    bool observed_fullscreen,
    std::optional<WindowGeometry> geometry_before_transition
) {
    WindowModeState result = before;
    if (!transition_succeeded ||
        observed_fullscreen != requested_fullscreen) {
        result.active_fullscreen = before.live_fullscreen;
        result.draft_fullscreen = before.live_fullscreen;
        return result;
    }
    if (requested_fullscreen && !before.live_fullscreen &&
        geometry_before_transition) {
        result.windowed_geometry = *geometry_before_transition;
    }
    result.live_fullscreen = observed_fullscreen;
    result.active_fullscreen = observed_fullscreen;
    result.draft_fullscreen = observed_fullscreen;
    return result;
}

}  // namespace aoe

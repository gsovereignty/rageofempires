#include "aoe/window_mode.hpp"

namespace aoe {

std::optional<RenderExtent> render_extent_for_drawable(
    int width,
    int height,
    int hud_height
) {
    if (width <= 0 || height <= hud_height || hud_height < 0) {
        return std::nullopt;
    }
    return RenderExtent{width, height, height - hud_height};
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

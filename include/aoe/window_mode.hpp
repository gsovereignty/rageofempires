#pragma once

#include <optional>
#include <span>
#include <vector>

namespace aoe {

struct RenderExtent {
    int width{};
    int screen_height{};
    int world_height{};

    bool operator==(const RenderExtent&) const = default;
};

std::optional<RenderExtent> render_extent_for_window(
    int window_width,
    int window_height,
    int drawable_width,
    int drawable_height,
    int hud_height
);

struct DisplayMode {
    int width{};
    int height{};
    bool operator==(const DisplayMode&) const = default;
};

std::vector<DisplayMode> supported_display_modes(
    std::span<const DisplayMode> reported
);

std::optional<RenderExtent> fixed_canvas_extent(
    int canvas_width,
    int canvas_height,
    int drawable_width,
    int drawable_height,
    int hud_height
);

struct WindowGeometry {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct WindowModeState {
    bool live_fullscreen{};
    bool active_fullscreen{};
    bool draft_fullscreen{};
    WindowGeometry windowed_geometry{};
};

WindowModeState window_mode_result(
    const WindowModeState& before,
    bool requested_fullscreen,
    bool transition_succeeded,
    bool observed_fullscreen,
    std::optional<WindowGeometry> geometry_before_transition = std::nullopt
);

}  // namespace aoe

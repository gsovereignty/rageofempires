#include "aoe/window_mode.hpp"

#include <array>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition) {
    if (!condition) throw std::runtime_error("window-mode check failed");
}

}  // namespace

int main() {
    using namespace aoe;
    require((render_extent_for_window(1280, 720, 1280, 720, 175) ==
             RenderExtent{1280, 720, 545}));
    require((render_extent_for_window(640, 360, 640, 360, 175) ==
             RenderExtent{640, 360, 185}));
    require(!render_extent_for_window(0, 720, 1280, 720, 175));
    require(!render_extent_for_window(640, 175, 640, 175, 175));
    require(!render_extent_for_window(640, 360, 0, 0, 175));
    require(
        render_extent_for_window(1920, 720, 3840, 1440, 175)->width ==
        1920
    );
    require(
        render_extent_for_window(640, 1080, 1280, 2160, 175)
            ->world_height == 905
    );
    require((render_extent_for_window(1280, 720, 2560, 1440, 175) ==
             RenderExtent{1280, 720, 545}));
    require((fixed_canvas_extent(1024, 768, 2048, 1536, 175) ==
             RenderExtent{1024, 768, 593}));
    require((fixed_canvas_extent(1024, 768, 2560, 1440, 175) ==
             RenderExtent{1024, 768, 593}));
    const std::array reported{
        DisplayMode{640, 480}, DisplayMode{1920, 1080},
        DisplayMode{1024, 768}, DisplayMode{1920, 1080}
    };
    require((supported_display_modes(reported) ==
             std::vector{DisplayMode{1024, 768}, DisplayMode{1920, 1080}}));

    const WindowGeometry geometry{20, 30, 800, 600};
    const WindowModeState windowed{false, false, false, geometry};
    const WindowGeometry captured{40, 50, 1024, 768};
    const auto entered = window_mode_result(
        windowed, true, true, true, captured
    );
    require(entered.live_fullscreen);
    require(entered.active_fullscreen && entered.draft_fullscreen);
    require(entered.windowed_geometry.width == 1024);

    const auto failed = window_mode_result(
        windowed, true, false, false, captured
    );
    require(!failed.live_fullscreen);
    require(!failed.active_fullscreen && !failed.draft_fullscreen);
    require(failed.windowed_geometry.width == 800);

    const auto left = window_mode_result(
        entered, false, true, false
    );
    require(!left.live_fullscreen);
    require(!left.active_fullscreen && !left.draft_fullscreen);
    require(left.windowed_geometry.x == 40);
    require(left.windowed_geometry.height == 768);
}

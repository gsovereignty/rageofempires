#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "aoe/game_map.hpp"

namespace aoe {

struct WorldTilePickerView {
    float camera_x{};
    float camera_y{};
    float zoom{1.0F};
    int origin_x{};
    int origin_y{};
    int half_tile_width{};
    int half_tile_height{};
    int elevation_pixel_step{};
};

[[nodiscard]] inline TilePosition pick_world_tile(
    const GameMap& map,
    float mouse_x,
    float mouse_y,
    const WorldTilePickerView& view
) {
    if (view.zoom <= 0.0F ||
        view.half_tile_width <= 0 ||
        view.half_tile_height <= 0) {
        return {};
    }
    const float projected_x =
        (mouse_x / view.zoom + view.camera_x -
         static_cast<float>(view.origin_x)) /
        static_cast<float>(view.half_tile_width);
    const float projected_y =
        (mouse_y / view.zoom + view.camera_y -
         static_cast<float>(view.origin_y)) /
        static_cast<float>(view.half_tile_height);
    const TilePosition flat{
        static_cast<int>(std::floor((projected_y + projected_x) / 2.0F)),
        static_cast<int>(std::floor((projected_y - projected_x) / 2.0F)),
    };

    // Elevation is bounded to 0..7 by GameMap. Its vertical displacement can
    // move inverse-projected x/y by at most four tiles. Include click-diamond
    // width and one rounding tile, while keeping lookup independent of map
    // area.
    constexpr int maximum_elevation = 7;
    const int elevation_tiles = static_cast<int>(std::ceil(
        static_cast<float>(
            maximum_elevation * view.elevation_pixel_step
        ) / static_cast<float>(view.half_tile_height)
    ));
    const int radius = elevation_tiles + 2;
    const int minimum_x = std::max(0, flat.x - radius);
    const int maximum_x = std::min(map.width() - 1, flat.x + radius);
    const int minimum_y = std::max(0, flat.y - radius);
    const int maximum_y = std::min(map.height() - 1, flat.y + radius);

    std::optional<TilePosition> elevated;
    float frontmost = -std::numeric_limits<float>::infinity();
    for (int y = minimum_y; y <= maximum_y; ++y) {
        for (int x = minimum_x; x <= maximum_x; ++x) {
            const TilePosition candidate{x, y};
            const float top_x = (
                static_cast<float>(
                    view.origin_x +
                    (candidate.x - candidate.y) * view.half_tile_width
                ) - view.camera_x
            ) * view.zoom;
            const float top_y = (
                static_cast<float>(
                    view.origin_y +
                    (candidate.x + candidate.y) * view.half_tile_height -
                    map.elevation_at(candidate) *
                        view.elevation_pixel_step
                ) - view.camera_y
            ) * view.zoom;
            const float center_y =
                top_y + static_cast<float>(view.half_tile_height) *
                    view.zoom;
            const float normalized =
                std::abs(mouse_x - top_x) /
                    (static_cast<float>(view.half_tile_width) * view.zoom) +
                std::abs(mouse_y - center_y) /
                    (static_cast<float>(view.half_tile_height) * view.zoom);
            if (normalized <= 1.0F && center_y >= frontmost) {
                elevated = candidate;
                frontmost = center_y;
            }
        }
    }
    return elevated.value_or(flat);
}

}  // namespace aoe

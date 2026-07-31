#include "aoe/world_tile_picker.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <limits>
#include <optional>

namespace {

constexpr aoe::WorldTilePickerView view{
    120.0F, 80.0F, 1.25F, 7680, 32, 48, 24, 24
};

std::pair<float, float> tile_center(
    const aoe::GameMap& map,
    aoe::TilePosition tile
) {
    const float x = (
        static_cast<float>(
            view.origin_x + (tile.x - tile.y) * view.half_tile_width
        ) - view.camera_x
    ) * view.zoom;
    const float y = (
        static_cast<float>(
            view.origin_y + (tile.x + tile.y) * view.half_tile_height -
            map.elevation_at(tile) * view.elevation_pixel_step
        ) - view.camera_y
    ) * view.zoom +
        static_cast<float>(view.half_tile_height) * view.zoom;
    return {x, y};
}

aoe::TilePosition reference_pick(
    const aoe::GameMap& map,
    float mouse_x,
    float mouse_y
) {
    const float projected_x =
        (mouse_x / view.zoom + view.camera_x - view.origin_x) /
        view.half_tile_width;
    const float projected_y =
        (mouse_y / view.zoom + view.camera_y - view.origin_y) /
        view.half_tile_height;
    const aoe::TilePosition flat{
        static_cast<int>(std::floor((projected_y + projected_x) / 2.0F)),
        static_cast<int>(std::floor((projected_y - projected_x) / 2.0F)),
    };
    std::optional<aoe::TilePosition> elevated;
    float frontmost = -std::numeric_limits<float>::infinity();
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const aoe::TilePosition candidate{x, y};
            const auto [top_x, center_y] = tile_center(map, candidate);
            const float normalized =
                std::abs(mouse_x - top_x) /
                    (view.half_tile_width * view.zoom) +
                std::abs(mouse_y - center_y) /
                    (view.half_tile_height * view.zoom);
            if (normalized <= 1.0F && center_y >= frontmost) {
                elevated = candidate;
                frontmost = center_y;
            }
        }
    }
    return elevated.value_or(flat);
}

}  // namespace

int main() {
    using namespace aoe;

    GameMap map{64, 64};
    for (int elevation = 0; elevation <= 7; ++elevation) {
        const TilePosition tile{20 + elevation, 30};
        map.set_elevation(tile, elevation);
        const auto [x, y] = tile_center(map, tile);
        assert(
            pick_world_tile(map, x, y, view) ==
            reference_pick(map, x, y)
        );
    }

    const TilePosition front{32, 31};
    const TilePosition behind{31, 30};
    map.set_elevation(front, 0);
    map.set_elevation(behind, 2);
    const auto [front_x, front_y] = tile_center(map, front);
    const auto [behind_x, behind_y] = tile_center(map, behind);
    assert(
        pick_world_tile(map, front_x, front_y, view) ==
        reference_pick(map, front_x, front_y)
    );
    assert(
        pick_world_tile(map, behind_x, behind_y, view) ==
        reference_pick(map, behind_x, behind_y)
    );

    GameMap maximum_map{480, 480};
    const auto [x, y] = tile_center(maximum_map, {240, 240});
    const auto started = std::chrono::steady_clock::now();
    TilePosition picked;
    for (int iteration = 0; iteration < 10000; ++iteration) {
        picked = pick_world_tile(maximum_map, x, y, view);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    assert(picked == reference_pick(maximum_map, x, y));
    assert(elapsed < std::chrono::seconds{2});
}

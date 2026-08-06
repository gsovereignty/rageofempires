#include "aoe/browser_telemetry.hpp"

#include <emscripten/emscripten.h>

namespace aoe {

EM_JS(void, publish_browser_telemetry_js,
    (std::uint64_t tick,
     std::uint64_t selected_unit,
     std::uint64_t selected_building,
     int wood,
     int food,
     int gold,
     int stone,
     int outcome,
     int logical_width,
     int logical_height,
     float camera_x,
     float camera_y,
     float camera_zoom,
     std::size_t unit_count,
     std::size_t building_count,
     std::size_t fallback_count), {
      Module.browserTelemetry = {
        tick: Number(tick),
        selectedUnit: Number(selected_unit),
        selectedBuilding: Number(selected_building),
        resources: {wood, food, gold, stone},
        outcome,
        logicalWidth: logical_width,
        logicalHeight: logical_height,
        camera: {x: camera_x, y: camera_y, zoom: camera_zoom},
        unitCount: Number(unit_count),
        buildingCount: Number(building_count),
        fallbackCount: Number(fallback_count),
        wasmHeapBytes: HEAPU8.length
      };
    });

void publish_browser_telemetry(const BrowserTelemetry& telemetry) {
    publish_browser_telemetry_js(
        telemetry.tick,
        telemetry.selected_unit,
        telemetry.selected_building,
        telemetry.wood,
        telemetry.food,
        telemetry.gold,
        telemetry.stone,
        telemetry.outcome,
        telemetry.logical_width,
        telemetry.logical_height,
        telemetry.camera_x,
        telemetry.camera_y,
        telemetry.camera_zoom,
        telemetry.unit_count,
        telemetry.building_count,
        telemetry.fallback_count
    );
}

}  // namespace aoe

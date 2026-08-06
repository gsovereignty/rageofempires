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
     std::size_t blue_military_count,
     bool man_at_arms_researched,
     int enemy_building_hit_points,
     std::size_t fallback_count,
     float villager_x,
     float villager_y,
     float resource_x,
     float resource_y,
     float military_x,
     float military_y,
     float barracks_x,
     float barracks_y,
     float enemy_building_x,
     float enemy_building_y), {
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
        blueMilitaryCount: Number(blue_military_count),
        manAtArmsResearched: Boolean(man_at_arms_researched),
        enemyBuildingHitPoints: enemy_building_hit_points,
        fallbackCount: Number(fallback_count),
        targets: {
          villager: {x: villager_x, y: villager_y},
          resource: {x: resource_x, y: resource_y},
          military: {x: military_x, y: military_y},
          barracks: {x: barracks_x, y: barracks_y},
          enemyBuilding: {x: enemy_building_x, y: enemy_building_y}
        },
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
        telemetry.blue_military_count,
        telemetry.man_at_arms_researched,
        telemetry.enemy_building_hit_points,
        telemetry.fallback_count,
        telemetry.targets.villager_x,
        telemetry.targets.villager_y,
        telemetry.targets.resource_x,
        telemetry.targets.resource_y,
        telemetry.targets.military_x,
        telemetry.targets.military_y,
        telemetry.targets.barracks_x,
        telemetry.targets.barracks_y,
        telemetry.targets.enemy_building_x,
        telemetry.targets.enemy_building_y
    );
}

}  // namespace aoe

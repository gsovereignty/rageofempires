#pragma once

#include <cstddef>
#include <cstdint>

namespace aoe {

struct BrowserTargetTelemetry {
    float villager_x{-1.0F};
    float villager_y{-1.0F};
    float resource_x{-1.0F};
    float resource_y{-1.0F};
    float military_x{-1.0F};
    float military_y{-1.0F};
    float barracks_x{-1.0F};
    float barracks_y{-1.0F};
    float enemy_building_x{-1.0F};
    float enemy_building_y{-1.0F};
};

struct BrowserTelemetry {
    std::uint64_t tick{};
    std::uint64_t selected_unit{};
    std::uint64_t selected_building{};
    int wood{};
    int food{};
    int gold{};
    int stone{};
    int outcome{};
    int logical_width{};
    int logical_height{};
    float camera_x{};
    float camera_y{};
    float camera_zoom{1.0F};
    std::size_t unit_count{};
    std::size_t building_count{};
    std::size_t blue_military_count{};
    bool man_at_arms_researched{};
    int enemy_building_hit_points{-1};
    std::size_t fallback_count{};
    BrowserTargetTelemetry targets;
};

void publish_browser_telemetry(const BrowserTelemetry& telemetry);

}  // namespace aoe

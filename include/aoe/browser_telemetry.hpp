#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aoe {

struct BrowserTargetTelemetry {
    float villager_x{-1.0F};
    float villager_y{-1.0F};
    float resource_x{-1.0F};
    float resource_y{-1.0F};
    float food_x{-1.0F};
    float food_y{-1.0F};
    float military_x{-1.0F};
    float military_y{-1.0F};
    float barracks_x{-1.0F};
    float barracks_y{-1.0F};
    float town_center_x{-1.0F};
    float town_center_y{-1.0F};
    float enemy_building_x{-1.0F};
    float enemy_building_y{-1.0F};
    float enemy_town_center_x{-1.0F};
    float enemy_town_center_y{-1.0F};
};

struct BrowserTelemetry {
    std::uint64_t tick{};
    std::uint64_t selected_unit{};
    std::size_t selected_unit_count{};
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
    bool pending_building{};
    int enemy_building_hit_points{-1};
    int enemy_town_center_hit_points{-1};
    std::size_t fallback_count{};
    BrowserTargetTelemetry targets;
};

void publish_browser_telemetry(const BrowserTelemetry& telemetry);
[[nodiscard]] bool browser_render_telemetry_enabled();
void publish_browser_render_telemetry(std::string_view json);

}  // namespace aoe

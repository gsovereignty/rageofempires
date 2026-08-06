#pragma once

#include <cstddef>
#include <cstdint>

namespace aoe {

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
    std::size_t fallback_count{};
};

void publish_browser_telemetry(const BrowserTelemetry& telemetry);

}  // namespace aoe

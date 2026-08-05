#include "aoe/minimap_contract.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace aoe::minimap {

int positive_floor(double value) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument("positive_floor requires a finite nonnegative value");
    }
    return static_cast<int>(value);
}

std::vector<ScalingRow> build_scaling_rows(
    int map_width,
    int map_height,
    int output_row_count
) {
    if (map_width <= 0 || map_height <= 0 || output_row_count <= 0) {
        throw std::invalid_argument("minimap dimensions must be positive");
    }

    const int source_row_count = map_width + map_height - 1;
    const double source_step =
        static_cast<double>(source_row_count) /
        static_cast<double>(output_row_count);
    std::vector<ScalingRow> rows;
    rows.reserve(static_cast<std::size_t>(output_row_count));

    double source_position = 0.0;
    for (int output_row = 0; output_row < output_row_count; ++output_row) {
        const int source_diagonal =
            std::min(positive_floor(source_position), source_row_count - 1);
        const int source_span = std::min({
            source_diagonal + 1,
            map_width,
            map_height,
            source_row_count - source_diagonal,
        });
        rows.push_back({output_row, source_diagonal, source_span});
        source_position += source_step;
    }
    return rows;
}

InclusiveRect size_one_marker_rect(int center_x, int center_y) {
    return {center_x - 1, center_y - 1, center_x + 1, center_y + 1};
}

InclusiveRect readable_marker_rect(int center_x, int center_y) {
    return {center_x - 2, center_y - 2, center_x + 2, center_y + 2};
}

InclusiveRect type_0x112_signal_outline(int center_x, int center_y) {
    return {center_x - 4, center_y - 4, center_x + 4, center_y + 4};
}

SignalPhase advance_type_0x112_signal_phase(
    bool alternate,
    std::uint32_t elapsed_ms
) {
    const bool toggled = elapsed_ms > 332;
    return {toggled ? !alternate : alternate, toggled};
}

std::uint8_t type_0x112_signal_palette(
    bool alternate,
    std::uint8_t primary_palette_index,
    std::uint8_t alternate_palette_index
) {
    return alternate ? alternate_palette_index : primary_palette_index;
}

InclusiveRect frame_1024_rect(int screen_width, int screen_height) {
    return {
        screen_width - 336,
        screen_height - 169,
        screen_width - 11,
        screen_height - 6,
    };
}

ViewportBounds proved_viewport_bounds(
    int transformed_x,
    int transformed_y,
    int viewport_width,
    int viewport_height,
    double scale_x,
    double scale_y
) {
    if (viewport_width < 0 || viewport_height < 0) {
        throw std::invalid_argument("viewport dimensions must be nonnegative");
    }
    return {
        transformed_x -
            positive_floor(static_cast<double>(viewport_width / 2) * scale_x),
        transformed_y -
            positive_floor(static_cast<double>(viewport_height) * scale_y),
        transformed_x +
            positive_floor(
                static_cast<double>(viewport_width / 2 + 2) * scale_x
            ),
        transformed_y +
            positive_floor(static_cast<double>(viewport_height + 4) * scale_y),
    };
}

bool shows_unit(MinimapMode mode, UnitKind kind) {
    if (mode == MinimapMode::normal) return true;
    const bool economic = kind == UnitKind::villager ||
        kind == UnitKind::trade_cart || kind == UnitKind::trade_cog ||
        kind == UnitKind::fishing_ship || kind == UnitKind::sheep ||
        kind == UnitKind::deer || kind == UnitKind::boar ||
        kind == UnitKind::relic;
    return mode == MinimapMode::economic ? economic : !economic;
}

bool shows_building(MinimapMode mode, BuildingKind kind) {
    if (mode == MinimapMode::normal) return true;
    const bool economic = kind == BuildingKind::town_center ||
        kind == BuildingKind::mill || kind == BuildingKind::lumber_camp ||
        kind == BuildingKind::mining_camp || kind == BuildingKind::farm ||
        kind == BuildingKind::market || kind == BuildingKind::dock ||
        kind == BuildingKind::fish_trap;
    return mode == MinimapMode::economic ? economic : !economic;
}

bool highlights_resource(MinimapMode mode, Terrain terrain) {
    if (mode != MinimapMode::economic) return false;
    return terrain == Terrain::forest || terrain == Terrain::pine_forest ||
        terrain == Terrain::oak_forest || terrain == Terrain::bamboo_forest ||
        terrain == Terrain::palm_forest ||
        terrain == Terrain::jungle_forest ||
        terrain == Terrain::berry_bush || terrain == Terrain::gold_mine ||
        terrain == Terrain::stone_mine || terrain == Terrain::fish ||
        terrain == Terrain::fish_shore || terrain == Terrain::fish_deep;
}

const char* mode_name(MinimapMode mode) {
    switch (mode) {
        case MinimapMode::normal: return "NORMAL";
        case MinimapMode::combat: return "COMBAT";
        case MinimapMode::economic: return "ECONOMIC";
    }
    return "NORMAL";
}

const char* mode_help(MinimapMode mode) {
    switch (mode) {
        case MinimapMode::normal:
            return "Normal map: terrain, resources, units, buildings; Statistics shows score.";
        case MinimapMode::combat:
            return "Combat map: military units and buildings; Statistics shows combat totals.";
        case MinimapMode::economic:
            return "Economic map: resources, workers, trade and economy buildings; Statistics shows resources.";
    }
    return "";
}

MinimapMode next_mode(MinimapMode mode) {
    switch (mode) {
        case MinimapMode::normal: return MinimapMode::combat;
        case MinimapMode::combat: return MinimapMode::economic;
        case MinimapMode::economic: return MinimapMode::normal;
    }
    return MinimapMode::normal;
}

StatisticsSummary statistics_summary(
    MinimapMode mode,
    const MatchStatistics& statistics
) {
    StatisticsSummary result;
    result.heading = mode == MinimapMode::normal ? "SCORE" :
        mode == MinimapMode::combat ? "KILLS/LOSSES" : "RESOURCES";
    for (std::size_t index = 0; index < result.values.size(); ++index) {
        const PlayerStatistics& player = statistics.players[index];
        if (mode == MinimapMode::normal) {
            result.values[index] = std::to_string(statistics.current_score[index]);
        } else if (mode == MinimapMode::combat) {
            result.values[index] = std::to_string(player.units_killed) + "/" +
                std::to_string(player.units_lost);
        } else {
            result.values[index] = std::to_string(
                player.gathered.wood + player.gathered.food +
                player.gathered.gold + player.gathered.stone
            );
        }
    }
    return result;
}

}  // namespace aoe::minimap

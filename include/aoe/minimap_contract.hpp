#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace aoe::minimap {

struct InclusiveRect {
    int left{};
    int top{};
    int right{};
    int bottom{};

    auto operator<=>(const InclusiveRect&) const = default;
};

struct ScalingRow {
    int output_row{};
    int source_diagonal{};
    int source_span{};

    auto operator<=>(const ScalingRow&) const = default;
};

struct SignalPhase {
    bool alternate{};
    bool toggled{};

    auto operator<=>(const SignalPhase&) const = default;
};

struct ViewportBounds {
    int left{};
    int top{};
    int right{};
    int bottom{};

    auto operator<=>(const ViewportBounds&) const = default;
};

inline constexpr std::array<std::uint8_t, 8> player_marker_palette_indices{
    242, 36, 241, 243, 251, 252, 132, 84,
};
inline constexpr std::array<std::array<std::uint8_t, 3>, 8> player_marker_rgb{{
    {0, 0, 255}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
    {0, 255, 255}, {255, 0, 255}, {185, 185, 185}, {255, 130, 1},
}};

[[nodiscard]] int positive_floor(double value);
[[nodiscard]] std::vector<ScalingRow> build_scaling_rows(
    int map_width,
    int map_height,
    int output_row_count
);
[[nodiscard]] InclusiveRect size_one_marker_rect(int center_x, int center_y);
[[nodiscard]] InclusiveRect type_0x112_signal_outline(
    int center_x,
    int center_y
);
[[nodiscard]] SignalPhase advance_type_0x112_signal_phase(
    bool alternate,
    std::uint32_t elapsed_ms
);
[[nodiscard]] std::uint8_t type_0x112_signal_palette(
    bool alternate,
    std::uint8_t primary_palette_index,
    std::uint8_t alternate_palette_index
);
[[nodiscard]] InclusiveRect frame_1024_rect(int screen_width, int screen_height);
[[nodiscard]] ViewportBounds proved_viewport_bounds(
    int transformed_x,
    int transformed_y,
    int viewport_width,
    int viewport_height,
    double scale_x,
    double scale_y
);

inline constexpr bool viewport_scanline_polygon_proved = false;
inline constexpr bool map640_anchor_proved = false;

}  // namespace aoe::minimap

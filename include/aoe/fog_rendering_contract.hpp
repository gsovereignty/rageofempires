#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace aoe::fog {

inline constexpr int shape_count = 17;
inline constexpr int edge_class_count = 47;

enum class WorldState : std::uint8_t {
    hidden,
    explored,
    visible,
};

enum class Neighbor : std::uint8_t {
    north_west = 0,
    south_west = 1,
    south_east = 2,
    north_east = 3,
    west = 4,
    south = 5,
    east = 6,
    north = 7,
};

struct AssetSelection {
    std::uint8_t tile_edge_class{};
    std::uint8_t black_edge_class{};
    bool apply_black_edge{};

    auto operator<=>(const AssetSelection&) const = default;
};

struct ScanlineSpan {
    std::uint8_t row{};
    std::uint8_t left{};
    std::uint8_t right{};

    auto operator<=>(const ScanlineSpan&) const = default;
};

enum class EdgeLayer : std::uint8_t { tile_left, tile_right, black };

// Original 8-bit renderer uses ordered stipple words in FUN_0054fb20.
inline constexpr std::uint8_t explored_dither_pattern = 0x56;
inline constexpr std::uint8_t hidden_dither_pattern = 0x28;
inline constexpr std::uint8_t hidden_rgb = 0;
inline constexpr std::uint8_t explored_brightness_percent = 50;

[[nodiscard]] std::uint8_t neighbor_mask(
    const std::array<bool, 8>& selected_neighbors
);
[[nodiscard]] const std::array<std::uint8_t, 256>& canonical_classes();
[[nodiscard]] std::uint8_t canonical_class(std::uint8_t neighbor_mask);
[[nodiscard]] AssetSelection select_assets(
    WorldState state,
    std::uint8_t visible_neighbor_mask,
    std::uint8_t explored_neighbor_mask
);
[[nodiscard]] bool valid_shape(std::uint8_t tile_shape);
[[nodiscard]] std::span<const std::uint8_t> encoded_spans(
    std::uint8_t tile_shape,
    std::uint8_t edge_class,
    EdgeLayer layer
);
[[nodiscard]] std::size_t span_count(
    std::uint8_t tile_shape,
    std::uint8_t edge_class,
    EdgeLayer layer
);

// Edge DAT payloads are geometry span lists, not pixel, palette, alpha, or
// dither payloads. Each record is {row, left, right}; 0xff in row terminates.
inline constexpr int payload_record_bytes = 3;
inline constexpr std::uint8_t payload_terminator = 0xff;
inline constexpr bool edge_payload_has_palette = false;
inline constexpr bool edge_payload_has_alpha = false;

}  // namespace aoe::fog

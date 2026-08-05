#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "aoe/legacy_assets.hpp"

namespace aoe::elevation_render {

struct Topology {
    std::uint8_t slope_id{};
    int base_level{};
    int maximum_level{};
};

// Exact FUN_004fd590 neighborhood classifier. Input is NW, N, NE, W, center,
// E, SW, S, SE. IDs 1..16 select FilterMaps records 1..16.
[[nodiscard]] Topology classify(
    std::array<int, 9> neighborhood
);

struct FilterSample {
    std::uint16_t source_offset{};
    std::uint16_t weight{};
};

struct FilterPixel {
    std::uint16_t lighting_offset{};
    std::vector<FilterSample> samples;
};

struct FilterRow {
    std::vector<FilterPixel> pixels;
};

struct FilterMap {
    std::vector<FilterRow> rows;
};

[[nodiscard]] std::array<FilterMap, 17> decode_filter_maps(
    std::span<const std::byte> bytes
);

[[nodiscard]] std::vector<std::array<std::uint8_t, 4>> decode_4096_tables(
    std::span<const std::byte> bytes,
    std::size_t expected_count
);

[[nodiscard]] int lighting_orientation(std::uint8_t slope_id);

struct Lighting {
    std::vector<std::vector<std::uint8_t>> pattern_masks;
    std::vector<std::vector<std::uint8_t>> light_maps;
    std::vector<std::uint8_t> inverse_color_maps;
    LegacyPalette palette;
};

[[nodiscard]] std::vector<std::vector<std::uint8_t>> decode_tables(
    std::span<const std::byte> bytes,
    std::size_t expected_count
);

[[nodiscard]] Lighting decode_lighting(
    std::span<const std::byte> pattern_masks,
    std::span<const std::byte> light_maps,
    std::span<const std::byte> inverse_color_maps,
    LegacyPalette palette
);

// Exact classic FilterMaps weighted scanline composition. Output keeps the
// source RGBA colors; palette lighting is applied by renderer's asset chain.
[[nodiscard]] RgbaFrame compose(
    const RgbaFrame& flat,
    const FilterMap& filter,
    const Lighting* lighting = nullptr,
    int orientation = 0
);

}  // namespace aoe::elevation_render

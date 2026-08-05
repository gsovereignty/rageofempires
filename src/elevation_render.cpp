#include "aoe/elevation_render.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace aoe::elevation_render {
namespace {

[[nodiscard]] std::uint8_t byte(std::byte value) {
    return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] std::uint32_t u32(
    std::span<const std::byte> bytes,
    std::size_t offset
) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw LegacyAssetError{"elevation asset is truncated"};
    }
    return static_cast<std::uint32_t>(byte(bytes[offset])) |
        static_cast<std::uint32_t>(byte(bytes[offset + 1])) << 8U |
        static_cast<std::uint32_t>(byte(bytes[offset + 2])) << 16U |
        static_cast<std::uint32_t>(byte(bytes[offset + 3])) << 24U;
}

[[nodiscard]] std::uint16_t u16(
    std::span<const std::byte> bytes,
    std::size_t offset
) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw LegacyAssetError{"elevation filter is truncated"};
    }
    return static_cast<std::uint16_t>(byte(bytes[offset])) |
        static_cast<std::uint16_t>(byte(bytes[offset + 1])) << 8U;
}

[[nodiscard]] std::uint32_t u24(
    std::span<const std::byte> bytes,
    std::size_t offset
) {
    if (offset > bytes.size() || bytes.size() - offset < 3) {
        throw LegacyAssetError{"elevation filter sample is truncated"};
    }
    return static_cast<std::uint32_t>(byte(bytes[offset])) |
        static_cast<std::uint32_t>(byte(bytes[offset + 1])) << 8U |
        static_cast<std::uint32_t>(byte(bytes[offset + 2])) << 16U;
}

}  // namespace

Topology classify(std::array<int, 9> h) {
    for (const int level : h) {
        if (level < 0 || level > 7) {
            throw std::invalid_argument{"elevation corner outside 0..7"};
        }
    }
    const int center = h[4];
    const int up = center + 1;
    const int down = center - 1;
    std::uint8_t id{};
    if (h[1] == up && h[5] == up) id = 14;
    else if (h[3] == up && h[7] == up) id = 13;
    else if (h[1] == up && h[3] == up) id = 16;
    else if (h[5] == up && h[7] == up) id = 15;
    else if (h[1] == up) id = 6;
    else if (h[5] == up) id = 8;
    else if (h[3] == up) id = 5;
    else if (h[7] == up) id = 7;
    else if (h[2] == up) id = (h[3] == down && h[7] == down) ? 2 : 10;
    else if (h[6] == up) id = (h[5] == down && h[7] == down) ? 3 : 11;
    else if (h[8] == up) id = (h[1] == down && h[3] == down) ? 4 : 12;
    else if (h[0] == up) id = (h[1] == down && h[5] == down) ? 1 : 9;
    const auto maximum = std::max_element(h.begin(), h.end());
    return {id, center, *maximum};
}

std::array<FilterMap, 17> decode_filter_maps(
    std::span<const std::byte> bytes
) {
    std::array<FilterMap, 17> result;
    std::size_t file_offset{};
    for (FilterMap& map : result) {
        const std::uint32_t block_size = u32(bytes, file_offset);
        if (block_size < 4 || file_offset + 4 > bytes.size() ||
            block_size > bytes.size() - file_offset - 4) {
            throw LegacyAssetError{"invalid elevation filter block size"};
        }
        const auto block = bytes.subspan(file_offset + 4, block_size);
        const std::uint32_t row_count = u32(block, 0);
        if (row_count == 0 || row_count > 97) {
            throw LegacyAssetError{"invalid elevation filter row count"};
        }
        std::size_t offset = 4;
        map.rows.reserve(row_count);
        for (std::uint32_t row = 0; row < row_count; ++row) {
            if (offset >= block.size()) {
                throw LegacyAssetError{"elevation filter row is truncated"};
            }
            const std::size_t pixel_count =
                byte(block[offset++]);
            if (pixel_count == 0 || pixel_count > 97) {
                throw LegacyAssetError{"invalid elevation filter row width"};
            }
            FilterRow decoded_row;
            decoded_row.pixels.reserve(pixel_count);
            while (decoded_row.pixels.size() < pixel_count) {
                const std::uint16_t header = u16(block, offset);
                offset += 2;
                const std::size_t sample_count = header & 0x0fU;
                if (sample_count == 0) {
                    throw LegacyAssetError{"empty elevation filter pixel"};
                }
                FilterPixel pixel;
                pixel.lighting_offset = header >> 4U;
                pixel.samples.reserve(sample_count);
                for (std::size_t sample = 0; sample < sample_count; ++sample) {
                    const std::uint32_t packed = u24(block, offset);
                    offset += 3;
                    pixel.samples.push_back({
                        static_cast<std::uint16_t>((packed >> 9U) & 0x7fffU),
                        static_cast<std::uint16_t>(packed & 0x1ffU),
                    });
                }
                decoded_row.pixels.push_back(std::move(pixel));
            }
            map.rows.push_back(std::move(decoded_row));
        }
        if (offset != block.size()) {
            throw LegacyAssetError{"elevation filter has trailing bytes"};
        }
        file_offset += 4 + block_size;
    }
    if (file_offset != bytes.size()) {
        throw LegacyAssetError{"FilterMaps.dat has trailing blocks"};
    }
    return result;
}

std::vector<std::array<std::uint8_t, 4>> decode_4096_tables(
    std::span<const std::byte> bytes,
    std::size_t expected_count
) {
    std::vector<std::array<std::uint8_t, 4>> result;
    result.reserve(expected_count * 1024);
    std::size_t offset{};
    for (std::size_t table = 0; table < expected_count; ++table) {
        if (u32(bytes, offset) != 4096) {
            throw LegacyAssetError{"invalid 4096-byte elevation table"};
        }
        offset += 4;
        if (offset > bytes.size() || bytes.size() - offset < 4096) {
            throw LegacyAssetError{"elevation table is truncated"};
        }
        for (std::size_t index = 0; index < 4096; index += 4) {
            result.push_back({
                byte(bytes[offset + index]),
                byte(bytes[offset + index + 1]),
                byte(bytes[offset + index + 2]),
                byte(bytes[offset + index + 3]),
            });
        }
        offset += 4096;
    }
    if (offset != bytes.size()) {
        throw LegacyAssetError{"elevation table file has trailing bytes"};
    }
    return result;
}

std::vector<std::vector<std::uint8_t>> decode_tables(
    std::span<const std::byte> bytes,
    std::size_t expected_count
) {
    std::vector<std::vector<std::uint8_t>> result;
    result.reserve(expected_count);
    std::size_t offset{};
    for (std::size_t table = 0; table < expected_count; ++table) {
        if (u32(bytes, offset) != 4096) {
            throw LegacyAssetError{"invalid 4096-byte elevation table"};
        }
        offset += 4;
        if (offset > bytes.size() || bytes.size() - offset < 4096) {
            throw LegacyAssetError{"elevation table is truncated"};
        }
        std::vector<std::uint8_t> decoded(4096);
        std::transform(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4096),
            decoded.begin(), byte
        );
        result.push_back(std::move(decoded));
        offset += 4096;
    }
    if (offset != bytes.size()) {
        throw LegacyAssetError{"elevation table file has trailing bytes"};
    }
    return result;
}

Lighting decode_lighting(
    std::span<const std::byte> patterns,
    std::span<const std::byte> lights,
    std::span<const std::byte> inverse,
    LegacyPalette palette
) {
    if (inverse.size() != 10U * 32768U || palette.colors.size() != 256) {
        throw LegacyAssetError{"invalid elevation ICM or palette"};
    }
    Lighting result{
        decode_tables(patterns, 40), decode_tables(lights, 18), {},
        std::move(palette),
    };
    result.inverse_color_maps.reserve(inverse.size());
    std::transform(inverse.begin(), inverse.end(),
                   std::back_inserter(result.inverse_color_maps), byte);
    return result;
}

int lighting_orientation(std::uint8_t slope_id) {
    switch (slope_id) {
        case 1: case 2: case 9: case 10: return 2;
        case 3: case 5: case 6: case 11: case 16: return 0;
        case 4: case 7: case 8: case 12: case 15: return 1;
        case 13: case 14: return 3;
        default: return 0;
    }
}

RgbaFrame compose(
    const RgbaFrame& flat,
    const FilterMap& filter,
    const Lighting* lighting,
    int orientation
) {
    if (flat.width != 97 || flat.height != 49 ||
        flat.rgba.size() != 97U * 49U * 4U || filter.rows.empty()) {
        throw LegacyAssetError{"elevation composition requires 97x49 terrain"};
    }
    std::vector<std::array<std::uint8_t, 4>> source(2468);
    std::size_t encoded = 0;
    for (int y = 0; y < flat.height; ++y) {
        std::vector<std::array<std::uint8_t, 4>> row;
        for (int x = 0; x < flat.width; ++x) {
            const std::size_t at = static_cast<std::size_t>(y * flat.width + x) * 4;
            if (flat.rgba[at + 3] == 0) continue;
            row.push_back({flat.rgba[at], flat.rgba[at + 1],
                           flat.rgba[at + 2], flat.rgba[at + 3]});
        }
        encoded += row.size() < 64 ? 1 : 2;
        for (const auto& color : row) {
            if (encoded >= source.size()) {
                throw LegacyAssetError{"terrain command stream exceeds classic size"};
            }
            source[encoded++] = color;
        }
        ++encoded;
    }
    RgbaFrame output{
        97, static_cast<int>(filter.rows.size()), 48,
        static_cast<int>(filter.rows.size()) / 2,
        std::vector<std::uint8_t>(97U * filter.rows.size() * 4U, 0),
    };
    for (std::size_t y = 0; y < filter.rows.size(); ++y) {
        const auto& pixels = filter.rows[y].pixels;
        const std::size_t left = (97U - pixels.size()) / 2U;
        for (std::size_t x = 0; x < pixels.size(); ++x) {
            std::array<std::uint64_t, 4> sum{};
            std::uint64_t weights{};
            for (const FilterSample sample : pixels[x].samples) {
                if (sample.source_offset >= source.size()) {
                    throw LegacyAssetError{"elevation source offset is invalid"};
                }
                weights += sample.weight;
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    sum[channel] += source[sample.source_offset][channel] *
                        static_cast<std::uint64_t>(sample.weight);
                }
            }
            if (weights == 0) throw LegacyAssetError{"zero elevation filter weight"};
            const std::size_t at = (y * 97U + left + x) * 4U;
            std::array<std::uint8_t, 4> color{};
            for (std::size_t channel = 0; channel < 4; ++channel) {
                color[channel] = static_cast<std::uint8_t>(
                    std::min<std::uint64_t>(255, (sum[channel] + weights / 2) / weights)
                );
            }
            if (lighting != nullptr) {
                if (orientation < 0 || orientation >= 4 ||
                    pixels[x].lighting_offset >= 4096) {
                    throw LegacyAssetError{"invalid elevation lighting offset"};
                }
                const std::uint8_t pattern = lighting->pattern_masks[
                    static_cast<std::size_t>(orientation)
                ][pixels[x].lighting_offset] >> 2U;
                if (pattern >= lighting->light_maps.size()) {
                    throw LegacyAssetError{"invalid elevation lighting pattern"};
                }
                const std::uint8_t icm = lighting->light_maps[pattern][
                    pixels[x].lighting_offset
                ];
                if (icm >= 10) {
                    throw LegacyAssetError{"invalid elevation ICM index"};
                }
                const std::size_t rgb555 =
                    static_cast<std::size_t>(color[0] >> 3U) * 1024U +
                    static_cast<std::size_t>(color[1] >> 3U) * 32U +
                    static_cast<std::size_t>(color[2] >> 3U);
                const std::uint8_t palette_index =
                    lighting->inverse_color_maps[icm * 32768U + rgb555];
                const auto& mapped = lighting->palette.colors[palette_index];
                color = {mapped[0], mapped[1], mapped[2], color[3]};
            }
            std::copy(color.begin(), color.end(), output.rgba.begin() + at);
        }
    }
    return output;
}

}  // namespace aoe::elevation_render

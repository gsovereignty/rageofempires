#include "aoe/terrain_transition.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace aoe {
namespace {

constexpr std::array<std::array<int, 8>, 8> blend_mask_lookup{{
    {{2, 3, 2, 1, 1, 6, 5, 4}},
    {{3, 3, 3, 1, 1, 6, 5, 4}},
    {{2, 3, 2, 1, 1, 6, 1, 4}},
    {{1, 1, 1, 0, 7, 6, 5, 4}},
    {{1, 1, 1, 7, 7, 6, 5, 4}},
    {{6, 6, 6, 6, 6, 6, 5, 4}},
    {{5, 5, 1, 5, 5, 5, 5, 4}},
    {{4, 3, 4, 4, 4, 4, 4, 4}},
}};

std::uint32_t read_u32(
    std::span<const std::byte> bytes,
    std::size_t& offset
) {
    if (offset + 4 > bytes.size()) {
        throw LegacyAssetError{"truncated blendomatic.dat"};
    }
    std::uint32_t result{};
    for (int shift = 0; shift < 32; shift += 8) {
        result |=
            std::to_integer<std::uint32_t>(bytes[offset++]) << shift;
    }
    return result;
}

std::vector<std::uint8_t> expand_mask(
    std::span<const std::byte> packed
) {
    constexpr int width = 97;
    constexpr int height = 49;
    std::vector<std::uint8_t> result(width * height);
    std::size_t source{};
    for (int y = 0; y < height; ++y) {
        const int distance = std::abs(24 - y);
        const int row_width = width - distance * 4;
        const int first = distance * 2;
        for (int x = 0; x < row_width; ++x) {
            if (source >= packed.size()) {
                throw LegacyAssetError{
                    "truncated blendomatic alpha mask"
                };
            }
            result[
                static_cast<std::size_t>(y * width + first + x)
            ] = std::to_integer<std::uint8_t>(packed[source++]);
        }
    }
    if (source != packed.size()) {
        throw LegacyAssetError{"invalid blendomatic tile size"};
    }
    return result;
}

void append_mask_ids(
    TerrainMaskSelection& result,
    std::uint8_t bits,
    std::optional<TilePosition> position
) {
    const std::array<std::pair<std::uint8_t, int>, 11> fixed{{
        {0b01000100, 20}, {0b00010001, 21},
        {0b01000001, 22}, {0b00000101, 23},
        {0b01010000, 24}, {0b00010100, 25},
        {0b01010100, 26}, {0b01010001, 27},
        {0b01000101, 28}, {0b00010101, 29},
        {0b01010101, 30},
    }};
    for (const auto [pattern, id] : fixed) {
        if ((bits & 0x55U) == pattern) {
            result.fixed_mask_ids.push_back(id);
            break;
        }
    }
    const std::array<std::pair<std::uint8_t, int>, 4> diagonal{{
        {0b00001000, 16}, {0b00100000, 17},
        {0b00000010, 18}, {0b10000000, 19},
    }};
    for (const auto [bit, id] : diagonal) {
        if ((bits & bit) != 0) result.fixed_mask_ids.push_back(id);
    }
    const std::array<std::pair<std::uint8_t, int>, 4> cardinal{{
        {0b00010000, 0}, {0b00000100, 4},
        {0b01000000, 8}, {0b00000001, 12},
    }};
    if ((bits & 0x55U) != 0 &&
        result.fixed_mask_ids.empty()) {
        for (const auto [bit, first] : cardinal) {
            if ((bits & 0x55U) == bit) {
                if (position) {
                    const std::uint32_t variant =
                        (static_cast<std::uint32_t>(position->x) +
                         static_cast<std::uint32_t>(position->y)) &
                        3U;
                    result.fixed_mask_ids.push_back(
                        first + static_cast<int>(variant)
                    );
                } else {
                    result.unresolved_cardinal_family =
                        std::array<int, 4>{
                            first, first + 1, first + 2, first + 3
                        };
                }
                break;
            }
        }
    }
}

}  // namespace

std::array<TerrainTransitionColor, 7>
procedural_transition_band(
    TerrainTransitionColor center,
    TerrainTransitionColor neighbor
) {
    std::array<TerrainTransitionColor, 7> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        // Both sides produce the same 1/2 boundary color; neighbor influence
        // then falls monotonically to zero toward the tile center.
        const unsigned neighbor_weight =
            static_cast<unsigned>(result.size() - 1 - index);
        constexpr unsigned denominator = 12;
        const unsigned center_weight = denominator - neighbor_weight;
        const auto mix = [=](std::uint8_t center_channel,
                             std::uint8_t neighbor_channel) {
            return static_cast<std::uint8_t>(
                (static_cast<unsigned>(center_channel) * center_weight +
                 static_cast<unsigned>(neighbor_channel) * neighbor_weight +
                 denominator / 2) /
                denominator
            );
        };
        result[index] = {
            mix(center.red, neighbor.red),
            mix(center.green, neighbor.green),
            mix(center.blue, neighbor.blue),
        };
    }
    return result;
}

std::optional<TerrainBlendEvidence>
terrain_blend_evidence(Terrain terrain) {
    if (terrain == Terrain::fish) terrain = Terrain::water;
    if (terrain == Terrain::forest ||
        terrain == Terrain::berry_bush ||
        terrain == Terrain::gold_mine ||
        terrain == Terrain::stone_mine) {
        terrain = Terrain::grass;
    }
    switch (terrain) {
        case Terrain::grass: return TerrainBlendEvidence{
            terrain, 102, 0, 15001};
        case Terrain::water: return TerrainBlendEvidence{
            terrain, 139, 3, 15002};
        case Terrain::beach: return TerrainBlendEvidence{
            terrain, 110, 2, 15017};
        case Terrain::shallows: return TerrainBlendEvidence{
            terrain, 60, 4, 15014};
        default: return std::nullopt;
    }
}

std::vector<TerrainMaskSelection>
select_terrain_transition_masks(
    Terrain center,
    const std::array<std::optional<Terrain>, 8>& neighbors,
    std::optional<TilePosition> position
) {
    const auto base = terrain_blend_evidence(center);
    if (!base) return {};
    std::map<int, TerrainMaskSelection> influence;
    std::array<bool, 8> dominant{};
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
        if (!neighbors[index]) continue;
        const auto overlay = terrain_blend_evidence(*neighbors[index]);
        dominant[index] =
            overlay && overlay->terrain != base->terrain &&
            overlay->priority > base->priority;
    }
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
        if (!dominant[index]) continue;
        if (index % 2 == 1 &&
            (dominant[(index + 7) % 8] ||
             dominant[(index + 1) % 8])) {
            continue;
        }
        const auto overlay = *terrain_blend_evidence(*neighbors[index]);
        auto& selected = influence[overlay.priority];
        selected.overlay = overlay.terrain;
        selected.priority = overlay.priority;
        selected.blend_mode =
            blend_mask_lookup[base->stored_mode][overlay.stored_mode];
        selected.influence_bits |=
            static_cast<std::uint8_t>(1U << index);
    }
    std::vector<TerrainMaskSelection> result;
    for (auto& [priority, selected] : influence) {
        (void)priority;
        append_mask_ids(
            selected, selected.influence_bits, position
        );
        result.push_back(std::move(selected));
    }
    return result;
}

BlendomaticData decode_blendomatic(
    std::span<const std::byte> bytes
) {
    std::size_t offset{};
    const std::uint32_t mode_count = read_u32(bytes, offset);
    const std::uint32_t mask_count = read_u32(bytes, offset);
    if (mode_count != 9 || mask_count != 31) {
        throw LegacyAssetError{
            "unsupported classic blendomatic dimensions"
        };
    }
    BlendomaticData result;
    result.modes.resize(mode_count);
    for (std::uint32_t mode = 0; mode < mode_count; ++mode) {
        const std::uint32_t tile_size = read_u32(bytes, offset);
        if (tile_size != 2353) {
            throw LegacyAssetError{
                "unsupported classic blendomatic tile size"
            };
        }
        const std::size_t flags = mask_count;
        // Pinned openage converter reads one packed 32-mask block whose
        // byte length is exactly four times the pixel count.
        const std::size_t bitmaps = tile_size * 4U;
        if (offset + flags + bitmaps > bytes.size()) {
            throw LegacyAssetError{"truncated blendomatic mode"};
        }
        offset += flags + bitmaps;
        result.modes[mode].reserve(mask_count);
        for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
            if (offset + tile_size > bytes.size()) {
                throw LegacyAssetError{
                    "truncated blendomatic alpha maps"
                };
            }
            BlendomaticMask decoded;
            decoded.alpha = expand_mask(
                bytes.subspan(offset, tile_size)
            );
            offset += tile_size;
            result.modes[mode].push_back(std::move(decoded));
        }
    }
    if (offset != bytes.size()) {
        throw LegacyAssetError{"trailing blendomatic.dat bytes"};
    }
    return result;
}

BlendomaticData load_blendomatic(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw LegacyAssetError{"cannot open blendomatic.dat"};
    }
    const std::vector<char> raw{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
    std::vector<std::byte> bytes(raw.size());
    std::transform(raw.begin(), raw.end(), bytes.begin(), [](char value) {
        return static_cast<std::byte>(
            static_cast<unsigned char>(value)
        );
    });
    return decode_blendomatic(bytes);
}

RgbaFrame compose_terrain_transition(
    const RgbaFrame& base,
    const RgbaFrame& overlay,
    const BlendomaticMask& mask
) {
    if (base.width != overlay.width ||
        base.height != overlay.height ||
        base.width != mask.width ||
        base.height != mask.height ||
        base.rgba.size() != overlay.rgba.size() ||
        mask.alpha.size() !=
            static_cast<std::size_t>(mask.width * mask.height)) {
        throw LegacyAssetError{
            "terrain transition dimensions do not match"
        };
    }
    RgbaFrame result = base;
    for (std::size_t pixel = 0; pixel < mask.alpha.size(); ++pixel) {
        const unsigned alpha = std::min<unsigned>(
            mask.alpha[pixel], 128U
        );
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t index = pixel * 4 + channel;
            result.rgba[index] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(base.rgba[index]) *
                     (128U - alpha) +
                 static_cast<unsigned>(overlay.rgba[index]) *
                     alpha +
                 64U) /
                128U
            );
        }
    }
    return result;
}

}  // namespace aoe

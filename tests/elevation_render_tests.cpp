#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "aoe/elevation_render.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "open elevation asset");
    const std::vector<char> chars{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t index = 0; index < chars.size(); ++index) {
        bytes[index] = static_cast<std::byte>(
            static_cast<unsigned char>(chars[index])
        );
    }
    return bytes;
}

std::uint64_t hash(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value = 1469598103934665603ULL;
    for (const std::uint8_t byte : bytes) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value;
}

}  // namespace

int main() {
    using namespace aoe::elevation_render;
    require(classify({2,2,2,2,2,2,2,2,2}).slope_id == 0, "flat topology");
    require(classify({3,2,2,2,2,2,2,2,2}).slope_id == 9, "NW rise");
    require(classify({3,1,2,2,2,1,2,2,2}).slope_id == 1, "NW peak");
    require(classify({2,3,2,3,2,2,2,2,2}).slope_id == 16, "northwest sides");
    require(classify({2,2,2,2,2,2,2,2,2}).base_level == 2, "base level");

    const auto filters = decode_filter_maps(read_bytes(
        std::filesystem::path{AOE_TEST_DATA_ROOT} / "FilterMaps.dat"
    ));
    const std::array<std::size_t, 17> heights{
        49, 25, 73, 49, 49, 25, 73, 25, 73,
        25, 73, 49, 49, 25, 73, 49, 49,
    };
    const std::array<std::size_t, 17> pixels{
        2353, 1225, 3529, 2377, 2377, 1225, 3529, 1225, 3529,
        1225, 3529, 2377, 2377, 1225, 3529, 2377, 2377,
    };
    for (std::size_t index = 0; index < filters.size(); ++index) {
        require(filters[index].rows.size() == heights[index], "filter height");
        std::size_t count{};
        for (const auto& row : filters[index].rows) count += row.pixels.size();
        require(count == pixels[index], "filter pixel count");
    }
    require(lighting_orientation(1) == 2, "slope 1 lighting");
    require(lighting_orientation(13) == 3, "slope 13 lighting");
    require(lighting_orientation(16) == 0, "slope 16 lighting");

    const aoe::DrsArchive interface{
        std::filesystem::path{AOE_TEST_DATA_ROOT} / "interfac.drs"
    };
    const aoe::DrsArchive terrain{
        std::filesystem::path{AOE_TEST_DATA_ROOT} / "terrain.drs"
    };
    const auto lighting = decode_lighting(
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "PatternMasks.dat"),
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "lightMaps.dat"),
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "view_icm.dat"),
        aoe::LegacyPalette::from_jasc(interface.read("bina", 50500))
    );
    require(lighting.pattern_masks.size() == 40, "pattern mask count");
    require(lighting.light_maps.size() == 18, "light map count");
    require(lighting.inverse_color_maps.size() == 327680, "ICM size");

    const auto grass_slp = terrain.read("slp", 15001);
    const aoe::IndexedSlpFrame flat =
        aoe::decode_indexed_slp_frame(grass_slp, 0);
    require(flat.source_bytes.size() == 2468, "exact indexed source span");
    require(flat.row_command_offsets.size() == 49, "indexed row offsets");
    require(flat.row_command_offsets.front() == 0, "indexed source base");
    const std::array<std::pair<std::int32_t, std::uint64_t>, 4>
        indexed_fixtures{{
            {15001, 3079128777978920411ULL},
            {15002, 9177422860481952568ULL},
            {15014, 3970707742325976802ULL},
            {15017, 16448584681211254875ULL},
        }};
    for (const auto [resource, expected_hash] : indexed_fixtures) {
        const auto indexed = aoe::decode_indexed_slp_frame(
            terrain.read("slp", resource), 0
        );
        require(indexed.source_bytes.size() == 2468,
                "terrain indexed span size");
        require(hash(indexed.source_bytes) == expected_hash,
                "terrain indexed span hash");
    }
    const aoe::RgbaFrame slope = compose(flat, filters[1], lighting, 2);
    require(slope.width == 97 && slope.height == 25, "composed dimensions");
    require(
        hash(slope.rgba) == 15247931737235412466ULL,
        "indexed scanline hash"
    );
    const aoe::RgbaFrame& lit = slope;
    for (std::size_t y = 0; y < filters[1].rows.size(); ++y) {
        const auto& row = filters[1].rows[y];
        const std::size_t left = (97U - row.pixels.size()) / 2U;
        for (std::size_t x = 0; x < row.pixels.size(); ++x) {
            require(
                lit.rgba[(y * 97U + left + x) * 4U + 3U] == 255,
                "filtered slope pixels remain opaque"
            );
        }
    }
    for (std::uint8_t slope_id = 1; slope_id <= 16; ++slope_id) {
        const auto rendered = compose(
            flat, filters[slope_id], lighting,
            lighting_orientation(slope_id)
        );
        require(!rendered.rgba.empty(), "all slope IDs compose");
    }
    std::vector<std::vector<std::uint8_t>> transitions(
        1, std::vector<std::uint8_t>(4096, 0)
    );
    const auto transitioned = compose(flat, filters[1], lighting, 2, transitions);
    require(hash(transitioned.rgba) != hash(slope.rgba),
            "indexed transition pattern affects slope lighting");

    bool bad_offset_rejected{};
    try {
        FilterMap invalid = filters[1];
        invalid.rows[0].pixels[0].samples[0].source_offset =
            static_cast<std::uint16_t>(flat.source_bytes.size());
        (void)compose(flat, invalid, lighting, 2);
    } catch (const aoe::LegacyAssetError&) {
        bad_offset_rejected = true;
    }
    require(bad_offset_rejected, "out-of-span source offset rejected");

    bool rejected{};
    try {
        auto truncated = read_bytes(
            std::filesystem::path{AOE_TEST_DATA_ROOT} / "FilterMaps.dat"
        );
        truncated.pop_back();
        (void)decode_filter_maps(truncated);
    } catch (const aoe::LegacyAssetError&) {
        rejected = true;
    }
    require(rejected, "truncated FilterMaps rejected");
    return 0;
}

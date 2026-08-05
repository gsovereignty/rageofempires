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
    const auto lighting = decode_lighting(
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "PatternMasks.dat"),
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "lightMaps.dat"),
        read_bytes(std::filesystem::path{AOE_TEST_DATA_ROOT} / "view_icm.dat"),
        aoe::LegacyPalette::from_jasc(interface.read("bina", 50500))
    );
    require(lighting.pattern_masks.size() == 40, "pattern mask count");
    require(lighting.light_maps.size() == 18, "light map count");
    require(lighting.inverse_color_maps.size() == 327680, "ICM size");

    aoe::RgbaFrame flat{
        97, 49, 48, 24, std::vector<std::uint8_t>(97U * 49U * 4U),
    };
    for (int y = 0; y < 49; ++y) {
        const int span = y <= 24 ? 1 + y * 4 : 1 + (48 - y) * 4;
        const int left = (97 - span) / 2;
        for (int x = left; x < left + span; ++x) {
            const std::size_t at = static_cast<std::size_t>(y * 97 + x) * 4;
            flat.rgba[at] = static_cast<std::uint8_t>(x * 2);
            flat.rgba[at + 1] = static_cast<std::uint8_t>(y * 4);
            flat.rgba[at + 2] = static_cast<std::uint8_t>((x + y) * 2);
            flat.rgba[at + 3] = 255;
        }
    }
    const aoe::RgbaFrame slope = compose(flat, filters[1]);
    require(slope.width == 97 && slope.height == 25, "composed dimensions");
    require(
        hash(slope.rgba) == 6863862511191404407ULL,
        "composed scanline hash"
    );
    const aoe::RgbaFrame lit = compose(flat, filters[1], &lighting, 2);
    require(
        hash(lit.rgba) == 704675108723462888ULL,
        "lit scanline hash"
    );

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

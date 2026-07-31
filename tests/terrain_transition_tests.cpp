#include "aoe/terrain_transition.hpp"

#include <iostream>

namespace {
int failures{};
void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void append_u32(
    std::vector<std::byte>& bytes,
    std::uint32_t value
) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>(
            (value >> shift) & 0xffU
        ));
    }
}
}

int main() {
    using aoe::Terrain;
    const auto band = aoe::procedural_transition_band(
        {140, 100, 60}, {70, 170, 200}
    );
    expect(
        band.front() == aoe::TerrainTransitionColor{105, 135, 130} &&
            band.back() == aoe::TerrainTransitionColor{140, 100, 60},
        "procedural band blends boundary and converges to center"
    );
    for (std::size_t index = 1; index < band.size(); ++index) {
        expect(
            band[index].red >= band[index - 1].red &&
                band[index].green <= band[index - 1].green &&
                band[index].blue <= band[index - 1].blue,
            "procedural band channels converge monotonically"
        );
    }
    std::array<std::optional<Terrain>, 8> neighbors;
    expect(
        aoe::select_terrain_transition_masks(
            Terrain::grass, neighbors, aoe::TilePosition{5, 6}
        ).empty(),
        "unknown fogged neighbors cannot influence transition"
    );
    neighbors[2] = Terrain::grass;
    neighbors[6] = Terrain::grass;
    auto selected = aoe::select_terrain_transition_masks(
        Terrain::shallows, neighbors
    );
    expect(selected.size() == 1 &&
               selected[0].fixed_mask_ids ==
                   std::vector<int>{20} &&
               !selected[0].unresolved_cardinal_family,
           "fixed adjacent mask 20");

    neighbors = {};
    neighbors[3] = Terrain::water;
    selected = aoe::select_terrain_transition_masks(
        Terrain::grass, neighbors
    );
    expect(selected.size() == 1 &&
               selected[0].fixed_mask_ids ==
                   std::vector<int>{16},
           "fixed diagonal mask 16");

    neighbors[2] = Terrain::water;
    selected = aoe::select_terrain_transition_masks(
        Terrain::grass, neighbors
    );
    expect(selected.size() == 1 &&
               selected[0].fixed_mask_ids.empty() &&
               selected[0].unresolved_cardinal_family ==
                   std::optional<std::array<int, 4>>{
                       std::array<int, 4>{4, 5, 6, 7}},
           "adjacent influence suppresses diagonal and stays unresolved");

    const std::array<std::pair<std::size_t, int>, 4> cardinals{{
        {0, 15}, {2, 7}, {4, 3}, {6, 11},
    }};
    for (const auto [neighbor, expected_mask] : cardinals) {
        neighbors = {};
        neighbors[neighbor] = Terrain::water;
        selected = aoe::select_terrain_transition_masks(
            Terrain::grass, neighbors, aoe::TilePosition{5, 6}
        );
        expect(
            selected.size() == 1 &&
                selected[0].fixed_mask_ids ==
                    std::vector<int>{expected_mask} &&
                !selected[0].unresolved_cardinal_family,
            "cardinal orientation and x+y variant"
        );
    }
    for (int variant = 0; variant < 4; ++variant) {
        neighbors = {};
        neighbors[0] = Terrain::water;
        selected = aoe::select_terrain_transition_masks(
            Terrain::grass,
            neighbors,
            aoe::TilePosition{variant, 0}
        );
        expect(
            selected.size() == 1 &&
                selected[0].fixed_mask_ids ==
                    std::vector<int>{12 + variant},
            "cardinal x+y cycles all four variants"
        );
    }

    const std::array<std::pair<std::uint8_t, int>, 11>
        cardinal_combinations{{
            {0x44, 20}, {0x11, 21}, {0x41, 22},
            {0x05, 23}, {0x50, 24}, {0x14, 25},
            {0x54, 26}, {0x51, 27}, {0x45, 28},
            {0x15, 29}, {0x55, 30},
        }};
    for (const auto [bits, expected_mask] :
         cardinal_combinations) {
        neighbors = {};
        for (std::size_t index = 0; index < neighbors.size();
             index += 2) {
            if ((bits & (1U << index)) != 0) {
                neighbors[index] = Terrain::water;
            }
        }
        selected = aoe::select_terrain_transition_masks(
            Terrain::grass, neighbors
        );
        expect(
            selected.size() == 1 &&
                selected[0].fixed_mask_ids ==
                    std::vector<int>{expected_mask},
            "fixed cardinal combination orientation"
        );
    }

    neighbors = {};
    neighbors[1] = Terrain::grass;
    neighbors[3] = Terrain::water;
    selected = aoe::select_terrain_transition_masks(
        Terrain::shallows, neighbors
    );
    expect(selected.size() == 2 &&
               selected[0].priority == 102 &&
               selected[1].priority == 139 &&
               selected[0].blend_mode == 1 &&
               selected[1].blend_mode == 7,
           "priority order and pinned lookup modes");

    aoe::RgbaFrame base{1, 1, 0, 0, {0, 20, 40, 255}};
    aoe::RgbaFrame overlay{1, 1, 0, 0, {128, 100, 40, 255}};
    aoe::BlendomaticMask half{1, 1, {64}};
    const aoe::RgbaFrame composed =
        aoe::compose_terrain_transition(base, overlay, half);
    expect(composed.rgba ==
               std::vector<std::uint8_t>{64, 60, 40, 255},
           "128-based alpha composition");
    aoe::BlendomaticMask zero{1, 1, {0}};
    aoe::BlendomaticMask full{1, 1, {128}};
    expect(
        aoe::compose_terrain_transition(base, overlay, zero).rgba ==
            base.rgba &&
        aoe::compose_terrain_transition(base, overlay, full).rgba ==
            overlay.rgba,
        "alpha endpoints"
    );

    std::vector<std::byte> fixture;
    append_u32(fixture, 9);
    append_u32(fixture, 31);
    for (int mode = 0; mode < 9; ++mode) {
        append_u32(fixture, 2353);
        fixture.insert(fixture.end(), 31, std::byte{});
        fixture.insert(fixture.end(), 2353 * 4, std::byte{});
        for (int mask = 0; mask < 31; ++mask) {
            fixture.insert(
                fixture.end(), 2353,
                static_cast<std::byte>(mode + mask)
            );
        }
    }
    const aoe::BlendomaticData decoded =
        aoe::decode_blendomatic(fixture);
    expect(decoded.modes.size() == 9 &&
               decoded.modes[0].size() == 31 &&
               decoded.modes[3][16].alpha.size() == 97U * 49U &&
               decoded.modes[3][16].alpha[48] == 19 &&
               decoded.modes[3][16].alpha[0] == 0,
           "bounded classic blendomatic fixture decoding");

    bool rejected{};
    try {
        fixture.pop_back();
        (void)aoe::decode_blendomatic(fixture);
    } catch (const aoe::LegacyAssetError&) {
        rejected = true;
    }
    expect(rejected, "truncated fixture rejected");

    if (failures == 0) std::cout << "terrain transition tests passed\n";
    return failures == 0 ? 0 : 1;
}

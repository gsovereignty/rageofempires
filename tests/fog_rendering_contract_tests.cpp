#include "aoe/fog_rendering_contract.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <set>

namespace {

using aoe::fog::Neighbor;

void compass_bits_match_original_callsite() {
    std::array<bool, 8> neighbors{};
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
        neighbors.fill(false);
        neighbors[bit] = true;
        assert(aoe::fog::neighbor_mask(neighbors) == (1U << bit));
    }

    assert(static_cast<int>(Neighbor::north_west) == 0);
    assert(static_cast<int>(Neighbor::south_west) == 1);
    assert(static_cast<int>(Neighbor::south_east) == 2);
    assert(static_cast<int>(Neighbor::north_east) == 3);
    assert(static_cast<int>(Neighbor::west) == 4);
    assert(static_cast<int>(Neighbor::south) == 5);
    assert(static_cast<int>(Neighbor::east) == 6);
    assert(static_cast<int>(Neighbor::north) == 7);
}

void normalization_is_total_and_has_exact_class_count() {
    const auto& classes = aoe::fog::canonical_classes();
    std::set<std::uint8_t> unique(classes.begin(), classes.end());
    assert(unique.size() == aoe::fog::edge_class_count);
    assert(*unique.begin() == 0);
    assert(*unique.rbegin() == 46);

    // A missing cardinal makes its two diagonal absence bits irrelevant.
    assert(
        aoe::fog::canonical_class(0x89) ==
        aoe::fog::canonical_class(0x80)
    );
    assert(
        aoe::fog::canonical_class(0x82) !=
        aoe::fog::canonical_class(0x80)
    );
}

void state_selects_original_edge_tables() {
    using aoe::fog::WorldState;
    const auto hidden = aoe::fog::select_assets(WorldState::hidden, 0xff, 0xff);
    assert(hidden.tile_edge_class == 0);
    assert(hidden.black_edge_class == 0);
    assert(!hidden.apply_black_edge);

    const auto explored =
        aoe::fog::select_assets(WorldState::explored, 0xff, 0x83);
    assert(explored.tile_edge_class == 0);
    assert(
        explored.black_edge_class == aoe::fog::canonical_class(0x83)
    );
    assert(explored.apply_black_edge);

    const auto visible =
        aoe::fog::select_assets(WorldState::visible, 0x83, 0xc7);
    assert(visible.tile_edge_class == aoe::fog::canonical_class(0x83));
    assert(visible.black_edge_class == aoe::fog::canonical_class(0xc7));
    assert(visible.apply_black_edge);
}

void tile_shape_selection_is_fail_closed() {
    for (std::uint8_t shape = 0; shape < 17; ++shape) {
        assert(aoe::fog::valid_shape(shape));
    }
    assert(!aoe::fog::valid_shape(17));
    assert(!aoe::fog::valid_shape(255));
}

void embedded_archive_geometry_is_exact_and_total() {
    using aoe::fog::EdgeLayer;
    std::size_t tile_records = 0;
    std::size_t black_records = 0;
    for (std::uint8_t shape = 0; shape < 17; ++shape) {
        for (std::uint8_t edge = 0; edge < 47; ++edge) {
            for (const auto layer : {EdgeLayer::tile_left, EdgeLayer::tile_right}) {
                const auto bytes = aoe::fog::encoded_spans(shape, edge, layer);
                assert(bytes.size() % 3 == 0);
                tile_records += bytes.size() / 3;
            }
            const auto black = aoe::fog::encoded_spans(shape, edge, EdgeLayer::black);
            assert(black.size() % 3 == 0);
            black_records += black.size() / 3;
        }
    }
    assert(tile_records == 66011);
    assert(black_records == 26270);
    assert(aoe::fog::span_count(0, 0, EdgeLayer::tile_left) == 49);
    assert(aoe::fog::span_count(0, 0, EdgeLayer::tile_right) == 0);
    assert(aoe::fog::span_count(0, 0, EdgeLayer::black) == 0);
    assert(aoe::fog::explored_dither_pattern == 0x56);
    assert(aoe::fog::hidden_dither_pattern == 0x28);
}

void every_neighbor_pattern_selects_geometry_for_every_shape() {
    using aoe::fog::EdgeLayer;
    using aoe::fog::WorldState;
    for (int visible_mask = 0; visible_mask < 256; ++visible_mask) {
        for (int explored_mask = 0; explored_mask < 256; ++explored_mask) {
            for (const WorldState state : {
                     WorldState::visible, WorldState::explored}) {
                const auto selection = aoe::fog::select_assets(
                    state,
                    static_cast<std::uint8_t>(visible_mask),
                    static_cast<std::uint8_t>(explored_mask)
                );
                for (std::uint8_t shape = 0; shape < 17; ++shape) {
                    assert(!aoe::fog::encoded_spans(
                        shape, selection.tile_edge_class,
                        EdgeLayer::tile_left).empty());
                    (void)aoe::fog::encoded_spans(
                        shape, selection.tile_edge_class,
                        EdgeLayer::tile_right);
                    if (selection.apply_black_edge) {
                        assert(!aoe::fog::encoded_spans(
                            shape, selection.black_edge_class,
                            EdgeLayer::black).empty());
                    }
                }
            }
        }
    }
}

}  // namespace

int main() {
    compass_bits_match_original_callsite();
    normalization_is_total_and_has_exact_class_count();
    state_selects_original_edge_tables();
    tile_shape_selection_is_fail_closed();
    embedded_archive_geometry_is_exact_and_total();
    every_neighbor_pattern_selects_geometry_for_every_shape();
}

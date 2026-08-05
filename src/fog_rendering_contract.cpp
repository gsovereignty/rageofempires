#include "aoe/fog_rendering_contract.hpp"

#include "fog_edge_geometry.hpp"

#include <stdexcept>

namespace aoe::fog {
namespace {

std::array<std::uint8_t, 256> build_canonical_classes() {
    std::array<int, 256> temporary{};
    temporary.fill(-1);

    int next_class = 0;
    for (int value = 0; value < 256; ++value) {
        const auto mask = static_cast<std::uint8_t>(value);
        const bool north_west_clear = (mask & 0x01) == 0;
        const bool west_set = (mask & 0x02) != 0;
        const bool south_west_set = (mask & 0x04) != 0;
        const bool south_clear = (mask & 0x08) == 0;

        bool accepted =
            (mask & 0x80) == 0 || (north_west_clear && south_clear);
        if ((mask & 0x40) != 0 && (south_west_set || !south_clear)) {
            accepted = false;
        }
        if ((mask & 0x20) != 0 && (west_set || south_west_set)) {
            accepted = false;
        }
        if (
            ((mask & 0x10) == 0 || (north_west_clear && !west_set)) &&
            accepted
        ) {
            temporary[static_cast<std::size_t>(value)] = next_class++;
        }
    }

    for (int value = 0; value < 256; ++value) {
        if (temporary[static_cast<std::size_t>(value)] >= 0) {
            continue;
        }
        auto normalized = static_cast<std::uint8_t>(value);
        if ((normalized & 0x80) != 0) {
            normalized &= static_cast<std::uint8_t>(~0x09);
        }
        if ((normalized & 0x40) != 0) {
            normalized &= static_cast<std::uint8_t>(~0x0c);
        }
        if ((normalized & 0x20) != 0) {
            normalized &= static_cast<std::uint8_t>(~0x06);
        }
        if ((normalized & 0x10) != 0) {
            normalized &= static_cast<std::uint8_t>(~0x03);
        }
        temporary[static_cast<std::size_t>(value)] =
            temporary[static_cast<std::size_t>(normalized)];
    }

    if (next_class != edge_class_count) {
        throw std::logic_error("original fog normalization must have 47 classes");
    }
    std::array<std::uint8_t, 256> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(temporary[index]);
    }
    return result;
}

}  // namespace

std::uint8_t neighbor_mask(
    const std::array<bool, 8>& selected_neighbors
) {
    std::uint8_t result = 0;
    for (std::size_t index = 0; index < selected_neighbors.size(); ++index) {
        if (selected_neighbors[index]) {
            result |= static_cast<std::uint8_t>(1U << index);
        }
    }
    return result;
}

const std::array<std::uint8_t, 256>& canonical_classes() {
    static const auto classes = build_canonical_classes();
    return classes;
}

std::uint8_t canonical_class(std::uint8_t mask) {
    return canonical_classes()[mask];
}

AssetSelection select_assets(
    WorldState state,
    std::uint8_t visible_neighbor_mask,
    std::uint8_t explored_neighbor_mask
) {
    switch (state) {
    case WorldState::hidden:
        return {canonical_class(0), canonical_class(0), false};
    case WorldState::explored:
        return {
            canonical_class(0),
            canonical_class(explored_neighbor_mask),
            canonical_class(explored_neighbor_mask) != 0,
        };
    case WorldState::visible:
        return {
            canonical_class(visible_neighbor_mask),
            canonical_class(explored_neighbor_mask),
            canonical_class(explored_neighbor_mask) != 0,
        };
    }
    throw std::invalid_argument("invalid fog world state");
}

bool valid_shape(std::uint8_t tile_shape) {
    return tile_shape < shape_count;
}

std::span<const std::uint8_t> encoded_spans(
    std::uint8_t tile_shape,
    std::uint8_t edge_class,
    EdgeLayer layer
) {
    if (!valid_shape(tile_shape) || edge_class >= edge_class_count) {
        throw std::out_of_range("fog edge geometry index outside 17x47 table");
    }
    const std::size_t entry =
        static_cast<std::size_t>(tile_shape) * edge_class_count + edge_class;
    std::uint32_t offset{};
    switch (layer) {
    case EdgeLayer::tile_left:
        offset = generated::tile_offsets[entry * 2];
        break;
    case EdgeLayer::tile_right:
        offset = generated::tile_offsets[entry * 2 + 1];
        break;
    case EdgeLayer::black:
        offset = generated::black_offsets[entry];
        break;
    }
    const auto begin = generated::span_bytes.begin() + offset;
    const auto end = std::find(begin, generated::span_bytes.end(), payload_terminator);
    if (end == generated::span_bytes.end()) {
        throw std::logic_error("generated fog span lacks terminator");
    }
    return {begin, static_cast<std::size_t>(end - begin)};
}

std::size_t span_count(
    std::uint8_t tile_shape,
    std::uint8_t edge_class,
    EdgeLayer layer
) {
    return encoded_spans(tile_shape, edge_class, layer).size() /
        payload_record_bytes;
}

}  // namespace aoe::fog

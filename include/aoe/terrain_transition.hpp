#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "aoe/legacy_assets.hpp"
#include "aoe/types.hpp"

namespace aoe {

struct TerrainBlendEvidence {
    Terrain terrain{Terrain::grass};
    int priority{};
    int stored_mode{};
    std::int32_t slp_id{};
};

struct TerrainMaskSelection {
    Terrain overlay{Terrain::grass};
    int priority{};
    int blend_mode{};
    std::uint8_t influence_bits{};
    std::vector<int> fixed_mask_ids;
    std::optional<std::array<int, 4>> unresolved_cardinal_family;
};

struct BlendomaticMask {
    int width{97};
    int height{49};
    std::vector<std::uint8_t> alpha;
};

struct BlendomaticData {
    std::vector<std::vector<BlendomaticMask>> modes;
};

[[nodiscard]] std::optional<TerrainBlendEvidence>
terrain_blend_evidence(Terrain terrain);

// Neighbor order is the classic Blendomatic order:
// 0 north, 1 north-east, 2 east, 3 south-east,
// 4 south, 5 south-west, 6 west, 7 north-west.
[[nodiscard]] std::vector<TerrainMaskSelection>
select_terrain_transition_masks(
    Terrain center,
    const std::array<std::optional<Terrain>, 8>& neighbors,
    std::optional<TilePosition> position = std::nullopt
);

[[nodiscard]] BlendomaticData decode_blendomatic(
    std::span<const std::byte> bytes
);
[[nodiscard]] BlendomaticData load_blendomatic(
    const std::filesystem::path& path
);

[[nodiscard]] RgbaFrame compose_terrain_transition(
    const RgbaFrame& base,
    const RgbaFrame& overlay,
    const BlendomaticMask& mask
);

}  // namespace aoe

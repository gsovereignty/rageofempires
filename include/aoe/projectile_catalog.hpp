#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "aoe/legacy_dat.hpp"
#include "aoe/types.hpp"

namespace aoe {

enum class ProjectileAssetKind {
    fire_stream,
    cannonball,
    gunpowder_shot,
    onager_primary,
    onager_volley,
    trebuchet_stone,
    throwing_axe,
    arrow,
    scorpion_bolt,
};

struct ProjectileAssetBinding {
    ProjectileAssetKind kind{};
    std::int16_t root_graphic{};
    std::int32_t slp_id{};
    std::int16_t frame_count{};
    std::int16_t angle_count{};
    std::uint8_t mirroring_mode{};
    std::optional<std::int16_t> shadow_graphic;
    std::optional<std::int32_t> shadow_slp_id;
    std::optional<std::int16_t> impact_graphic;
    std::optional<std::int32_t> impact_slp_id;
    std::optional<std::int16_t> impact_frame_count;
    bool direction_mapping_proved{};
};

struct ProjectileFrameSelection {
    std::size_t frame_index{};
    bool flip_horizontal{};
};

[[nodiscard]] std::span<const ProjectileAssetBinding>
canonical_projectile_asset_bindings();
[[nodiscard]] std::string_view projectile_asset_kind_name(
    ProjectileAssetKind kind
);
[[nodiscard]] std::optional<ProjectileAssetKind>
projectile_asset_kind_for(const Projectile& projectile);
[[nodiscard]] std::optional<ProjectileAssetKind>
impact_asset_kind_for(const ImpactEffect& impact);

// Uses Genie/openage's front direction (-1,+1), clockwise degrees, nearest
// logical angle, and horizontal mirroring above 180 degrees. Rejects archives
// whose physical frame count does not match the half-plus-center layout.
[[nodiscard]] std::optional<ProjectileFrameSelection>
select_projectile_frame(
    TilePosition origin,
    TilePosition destination,
    std::int16_t frames_per_angle,
    std::int16_t angle_count,
    std::size_t physical_frame_count,
    std::uint64_t animation_tick
);

// Resolves only exact, neutral DAT records with expected animation metadata
// and direct layer-10 shadow linkage. Any drift returns no binding.
[[nodiscard]] std::optional<ProjectileAssetBinding>
find_projectile_asset_binding(
    std::span<const LegacyGraphic> graphics,
    ProjectileAssetKind kind
);

[[nodiscard]] std::optional<ProjectileAssetBinding>
find_projectile_asset_binding(
    const LegacyDatFile& dat,
    ProjectileAssetKind kind
);

}  // namespace aoe

#include "aoe/projectile_catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace aoe {
namespace {

constexpr std::array projectile_bindings{
    ProjectileAssetBinding{ProjectileAssetKind::fire_stream, 3822, 4193,
        1, 1, 0, {}, {}, 5463, 4370, 20, true},
    ProjectileAssetBinding{ProjectileAssetKind::cannonball, 3382, 3803,
        1, 1, 0, 3383, 3804, 1744, 416, 10, true},
    ProjectileAssetBinding{ProjectileAssetKind::gunpowder_shot, 3396, 4500,
        1, 1, 0, 3397, 3818, 5463, 4370, 20, true},
    ProjectileAssetBinding{ProjectileAssetKind::onager_primary, 3396, 4500,
        1, 1, 0, 3397, 3818, 1744, 416, 10, true},
    ProjectileAssetBinding{ProjectileAssetKind::onager_volley, 3385, 3986,
        10, 1, 0, 3386, 3807, 1744, 416, 10, true},
    ProjectileAssetBinding{ProjectileAssetKind::trebuchet_stone, 3394, 3815,
        10, 1, 0, 3395, 3816, 4203, 4370, 20, true},
    ProjectileAssetBinding{ProjectileAssetKind::throwing_axe, 3380, 3801,
        10, 8, 6, 3381, 3802, {}, {}, {}, true},
    ProjectileAssetBinding{ProjectileAssetKind::arrow, 638, 50,
        1, 72, 54, {}, {}, {}, {}, {}, true},
    ProjectileAssetBinding{ProjectileAssetKind::scorpion_bolt, 3391, 3812,
        1, 18, 13, 3392, 3813, 1744, 416, 10, true},
};

const LegacyGraphic* graphic_at(
    std::span<const LegacyGraphic> graphics,
    std::int16_t id
) {
    if (id < 0 || static_cast<std::size_t>(id) >= graphics.size()) {
        return nullptr;
    }
    const LegacyGraphic& graphic =
        graphics[static_cast<std::size_t>(id)];
    return graphic.graphic_id == id ? &graphic : nullptr;
}

bool matches(
    const LegacyGraphic& graphic,
    std::int32_t slp,
    std::int16_t frames,
    std::int16_t angles,
    std::uint8_t mirror,
    std::uint8_t layer
) {
    return graphic.slp_id == slp &&
        graphic.frame_count == frames &&
        graphic.angle_count == angles &&
        graphic.mirroring_mode == mirror &&
        graphic.layer == layer &&
        graphic.player_color == -1;
}

}  // namespace

std::span<const ProjectileAssetBinding>
canonical_projectile_asset_bindings() {
    return projectile_bindings;
}

std::string_view projectile_asset_kind_name(ProjectileAssetKind kind) {
    switch (kind) {
        case ProjectileAssetKind::fire_stream: return "fire_stream";
        case ProjectileAssetKind::cannonball: return "cannonball";
        case ProjectileAssetKind::gunpowder_shot:
            return "gunpowder_shot";
        case ProjectileAssetKind::onager_primary:
            return "onager_primary";
        case ProjectileAssetKind::onager_volley:
            return "onager_volley";
        case ProjectileAssetKind::trebuchet_stone:
            return "trebuchet_stone";
        case ProjectileAssetKind::throwing_axe:
            return "throwing_axe";
        case ProjectileAssetKind::arrow: return "arrow";
        case ProjectileAssetKind::scorpion_bolt:
            return "scorpion_bolt";
    }
    return "unknown";
}

std::optional<ProjectileAssetKind> projectile_asset_kind_for(
    const Projectile& projectile
) {
    if (projectile.source_kind == UnitKind::scorpion ||
        projectile.source_kind == UnitKind::heavy_scorpion) {
        return ProjectileAssetKind::scorpion_bolt;
    }
    if (projectile.source_kind == UnitKind::mangonel ||
        projectile.source_kind == UnitKind::onager ||
        projectile.source_kind == UnitKind::siege_onager) {
        return projectile.visual_lane == 0
            ? ProjectileAssetKind::onager_primary
            : ProjectileAssetKind::onager_volley;
    }
    if (projectile.source_kind == UnitKind::trebuchet) {
        return ProjectileAssetKind::trebuchet_stone;
    }
    const bool gunshot =
        projectile.source_kind == UnitKind::hand_cannoneer ||
        (projectile.splash_radius == 0 &&
         projectile.damage_class == DamageClass::pierce &&
         projectile.damage >= 16);
    if (gunshot) return ProjectileAssetKind::gunpowder_shot;
    const bool throwing_axe =
        projectile.splash_radius == 0 &&
        projectile.damage_class == DamageClass::melee &&
        projectile.damage > 0 &&
        projectile.damage < 35;
    if (throwing_axe) return ProjectileAssetKind::throwing_axe;
    const bool cannonball =
        projectile.source_kind == UnitKind::bombard_cannon ||
        (projectile.source_is_building &&
         projectile.source_building_kind ==
             BuildingKind::bombard_tower) ||
        (projectile.splash_radius == 0 &&
         projectile.damage_class == DamageClass::melee &&
         projectile.damage >= 35);
    if (cannonball) return ProjectileAssetKind::cannonball;
    const bool fire_stream =
        projectile.splash_radius == 0 &&
        projectile.damage_class == DamageClass::pierce &&
        projectile.damage <= 3 &&
        projectile.visual_lane == 0;
    if (fire_stream) return ProjectileAssetKind::fire_stream;
    if (projectile.splash_radius == 0) {
        return ProjectileAssetKind::arrow;
    }
    return std::nullopt;
}

std::optional<ProjectileAssetKind> impact_asset_kind_for(
    const ImpactEffect& impact
) {
    const bool gunpowder =
        impact.source_kind == UnitKind::trebuchet ||
        impact.source_kind == UnitKind::hand_cannoneer ||
        impact.source_kind == UnitKind::janissary ||
        impact.source_kind == UnitKind::elite_janissary ||
        impact.source_kind == UnitKind::conquistador ||
        impact.source_kind == UnitKind::elite_conquistador ||
        impact.source_kind == UnitKind::fire_ship ||
        impact.source_kind == UnitKind::fast_fire_ship;
    if (gunpowder) return ProjectileAssetKind::gunpowder_shot;
    const bool siege =
        impact.source_kind == UnitKind::scorpion ||
        impact.source_kind == UnitKind::heavy_scorpion ||
        impact.source_kind == UnitKind::mangonel ||
        impact.source_kind == UnitKind::onager ||
        impact.source_kind == UnitKind::siege_onager ||
        impact.source_kind == UnitKind::bombard_cannon ||
        impact.source_kind == UnitKind::cannon_galleon ||
        impact.source_kind == UnitKind::elite_cannon_galleon ||
        impact.source_kind == UnitKind::turtle_ship ||
        impact.source_kind == UnitKind::elite_turtle_ship ||
        (impact.source_is_building &&
         impact.source_building_kind == BuildingKind::bombard_tower);
    return siege
        ? std::optional{ProjectileAssetKind::cannonball}
        : std::nullopt;
}

bool projectile_impact_is_visible(const ImpactEffect& impact) {
    return impact.splash || impact_asset_kind_for(impact).has_value();
}

std::optional<ProjectileFrameSelection>
select_projectile_frame(
    TilePosition origin,
    TilePosition destination,
    std::int16_t frames_per_angle,
    std::int16_t angle_count,
    std::size_t physical_frame_count,
    std::uint64_t animation_tick
) {
    if (frames_per_angle <= 0 || angle_count <= 0 ||
        origin == destination) {
        return std::nullopt;
    }
    const std::size_t stored_angles =
        static_cast<std::size_t>(angle_count / 2 + 1);
    if (physical_frame_count !=
        stored_angles *
            static_cast<std::size_t>(frames_per_angle)) {
        return std::nullopt;
    }
    const double dx =
        static_cast<double>(destination.x - origin.x);
    const double dy =
        static_cast<double>(destination.y - origin.y);
    double degrees = std::atan2(
        -(dx + dy), dy - dx
    ) * 180.0 / std::numbers::pi;
    if (degrees < 0.0) degrees += 360.0;
    const double step = 360.0 / angle_count;
    int logical = static_cast<int>(
        std::floor((degrees + step / 2.0) / step)
    ) % angle_count;
    const bool flip = logical > angle_count / 2;
    if (flip) logical = angle_count - logical;
    const std::size_t action =
        static_cast<std::size_t>(
            animation_tick %
            static_cast<std::uint64_t>(frames_per_angle)
        );
    return ProjectileFrameSelection{
        static_cast<std::size_t>(logical) *
            static_cast<std::size_t>(frames_per_angle) +
            action,
        flip,
    };
}

std::optional<ProjectileAssetBinding>
find_projectile_asset_binding(
    std::span<const LegacyGraphic> graphics,
    ProjectileAssetKind kind
) {
    const auto found = std::ranges::find(
        projectile_bindings, kind, &ProjectileAssetBinding::kind
    );
    if (found == projectile_bindings.end()) return std::nullopt;
    const ProjectileAssetBinding binding = *found;
    const LegacyGraphic* root =
        graphic_at(graphics, binding.root_graphic);
    if (!root || !matches(
            *root, binding.slp_id, binding.frame_count,
            binding.angle_count, binding.mirroring_mode, 30
        )) {
        return std::nullopt;
    }
    if (binding.shadow_graphic) {
        const LegacyGraphic* shadow =
            graphic_at(graphics, *binding.shadow_graphic);
        bool linked{};
        for (const LegacyGraphicDelta& delta : root->deltas) {
            linked = linked || delta.graphic_id == *binding.shadow_graphic;
        }
        if (!shadow || !linked ||
            shadow->slp_id != *binding.shadow_slp_id ||
            shadow->layer != 10 || shadow->player_color != -1 ||
            shadow->frame_count <= 0 || shadow->angle_count <= 0) {
            return std::nullopt;
        }
    }
    if (binding.impact_graphic) {
        const LegacyGraphic* impact =
            graphic_at(graphics, *binding.impact_graphic);
        if (!impact || impact->slp_id != *binding.impact_slp_id ||
            impact->frame_count != *binding.impact_frame_count ||
            impact->angle_count != 1 || impact->layer != 30 ||
            impact->player_color != -1) {
            return std::nullopt;
        }
    }
    return binding;
}

std::optional<ProjectileAssetBinding>
find_projectile_asset_binding(
    const LegacyDatFile& dat,
    ProjectileAssetKind kind
) {
    return find_projectile_asset_binding(dat.graphics(), kind);
}

}  // namespace aoe

#include "aoe/projectile_catalog.hpp"

#include <iostream>
#include <vector>

namespace {
int failures{};
void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

aoe::LegacyGraphic graphic(
    int id, int slp, int layer, int frames, int angles,
    int mirror = 0
) {
    aoe::LegacyGraphic result;
    result.graphic_id = static_cast<std::int16_t>(id);
    result.slp_id = slp;
    result.layer = static_cast<std::uint8_t>(layer);
    result.player_color = -1;
    result.frame_count = static_cast<std::int16_t>(frames);
    result.angle_count = static_cast<std::int16_t>(angles);
    result.mirroring_mode = static_cast<std::uint8_t>(mirror);
    return result;
}
}

int main() {
    std::vector<aoe::LegacyGraphic> graphics(4300);
    graphics[3382] = graphic(3382, 3803, 30, 1, 1);
    graphics[3383] = graphic(3383, 3804, 10, 1, 1);
    graphics[1744] = graphic(1744, 416, 30, 10, 1);
    graphics[3382].deltas.push_back({3383, 0, 0, -1});
    const auto cannonball = aoe::find_projectile_asset_binding(
        graphics, aoe::ProjectileAssetKind::cannonball
    );
    expect(
        cannonball && cannonball->slp_id == 3803 &&
            cannonball->shadow_slp_id == 3804 &&
            cannonball->impact_slp_id == 416 &&
            cannonball->direction_mapping_proved,
        "exact cannonball chain"
    );

    graphics[3383].layer = 20;
    expect(
        !aoe::find_projectile_asset_binding(
            graphics, aoe::ProjectileAssetKind::cannonball
        ),
        "wrong shadow layer rejected"
    );
    graphics[3383].layer = 10;
    graphics[1744].frame_count = 9;
    expect(
        !aoe::find_projectile_asset_binding(
            graphics, aoe::ProjectileAssetKind::cannonball
        ),
        "impact cadence drift rejected"
    );

    graphics[638] = graphic(638, 50, 30, 1, 72, 54);
    const auto arrow = aoe::find_projectile_asset_binding(
        graphics, aoe::ProjectileAssetKind::arrow
    );
    expect(
        arrow && arrow->direction_mapping_proved &&
            arrow->root_graphic == 638 && arrow->slp_id == 50,
        "static 72-direction arrow binding remains exact"
    );
    expect(
        aoe::select_projectile_frame(
            {0, 0}, {1, 0}, 1, 72, 37, 0
        ).has_value(),
        "static Arrow SLP half-plus-center layout accepted"
    );
    expect(
        !aoe::select_projectile_frame(
            {0, 0}, {1, 0}, 11, 32, 176, 0
        ),
        "short Arrow SLP layout rejected"
    );

    aoe::ImpactEffect crossbow_arrow;
    crossbow_arrow.source_kind = aoe::UnitKind::crossbowman;
    expect(
        !aoe::impact_asset_kind_for(crossbow_arrow) &&
            !aoe::projectile_impact_is_visible(crossbow_arrow),
        "ordinary unit arrow has no borrowed siege impact"
    );
    aoe::ImpactEffect tower_arrow;
    tower_arrow.source_is_building = true;
    tower_arrow.source_building_kind = aoe::BuildingKind::watch_tower;
    expect(
        !aoe::impact_asset_kind_for(tower_arrow) &&
            !aoe::projectile_impact_is_visible(tower_arrow),
        "ordinary building arrow has no borrowed siege impact"
    );
    aoe::ImpactEffect onager_impact;
    onager_impact.source_kind = aoe::UnitKind::onager;
    expect(
        aoe::impact_asset_kind_for(onager_impact) ==
                aoe::ProjectileAssetKind::cannonball &&
            aoe::projectile_impact_is_visible(onager_impact),
        "proved siege impact remains visible"
    );
    aoe::ImpactEffect petard_impact;
    petard_impact.source_kind = aoe::UnitKind::petard;
    petard_impact.splash = true;
    expect(
        !aoe::impact_asset_kind_for(petard_impact) &&
            aoe::projectile_impact_is_visible(petard_impact),
        "procedural splash impact remains visible"
    );

    const auto front = aoe::select_projectile_frame(
        {0, 0}, {-1, 1}, 1, 18, 10, 0
    );
    const auto back = aoe::select_projectile_frame(
        {0, 0}, {1, -1}, 1, 18, 10, 0
    );
    const auto mirrored = aoe::select_projectile_frame(
        {0, 0}, {1, 1}, 1, 18, 10, 0
    );
    expect(
        front && front->frame_index == 0 &&
            !front->flip_horizontal &&
            back && back->frame_index == 9 &&
            !back->flip_horizontal &&
            mirrored && mirrored->frame_index == 4 &&
            mirrored->flip_horizontal,
        "18-direction nearest-angle and mirror transform"
    );

    if (failures == 0) {
        std::cout << "projectile catalog tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

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

    graphics.resize(4301);
    graphics[3378] = graphic(3378, 3799, 30, 11, 32, 24);
    graphics[3379] = graphic(3379, 3800, 10, 1, 72, 54);
    graphics[3378].deltas.push_back({3379, 0, 0, -1});
    const auto arrow = aoe::find_projectile_asset_binding(
        graphics, aoe::ProjectileAssetKind::arrow
    );
    expect(
        arrow && !arrow->direction_mapping_proved,
        "unproved 32-direction mapping remains fail closed"
    );
    expect(
        !aoe::select_projectile_frame(
            {0, 0}, {1, 0}, 11, 32, 176, 0
        ),
        "short Arrow SLP layout rejected"
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

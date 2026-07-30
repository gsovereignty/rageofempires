#include "aoe/shadow_catalog.hpp"

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
    int player = -1
) {
    aoe::LegacyGraphic result;
    result.graphic_id = static_cast<std::int16_t>(id);
    result.slp_id = slp;
    result.layer = static_cast<std::uint8_t>(layer);
    result.frame_count = static_cast<std::int16_t>(frames);
    result.angle_count = static_cast<std::int16_t>(angles);
    result.player_color = static_cast<std::int16_t>(player);
    return result;
}
}

int main() {
    std::vector<aoe::LegacyGraphic> graphics{
        graphic(0, 100, 20, 10, 8),
        graphic(1, 200, 10, 10, 8),
    };
    graphics[0].deltas.push_back({1, 3, -2, -1});
    const auto exact = aoe::find_exact_shadow_binding(
        graphics, 100
    );
    expect(exact && exact->shadow_slp == 200 &&
               exact->offset_x == 3 && exact->offset_y == -2 &&
               exact->shadow_frame_count == 10,
           "exact layer-10 binding");

    graphics[1].frame_count = 1;
    expect(aoe::find_exact_shadow_binding(graphics, 100).has_value(),
           "static shadow synchronizes with animated root");

    graphics[1].player_color = 1;
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "player-colored child rejected");
    graphics[1].player_color = -1;
    graphics[1].frame_count = 4;
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "unmatched frame cadence rejected");

    graphics[1].frame_count = 10;
    graphics.push_back(graphic(2, 201, 10, 10, 8));
    graphics[0].deltas.push_back({2, 0, 0, -1});
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "multiple shadow children rejected");

    graphics.pop_back();
    graphics[0].deltas.pop_back();
    graphics.push_back(graphic(2, 100, 20, 10, 8));
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "shared root without shadow rejected");

    graphics[2].deltas.push_back({1, 4, -2, -1});
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "shared roots with conflicting offsets rejected");
    graphics[2].deltas[0].offset_x = 3;
    expect(aoe::find_exact_shadow_binding(graphics, 100).has_value(),
           "shared roots with identical binding accepted");
    graphics[2].deltas[0].display_angle = 5;
    expect(!aoe::find_exact_shadow_binding(graphics, 100),
           "shared roots with conflicting display angle rejected");

    if (failures == 0) std::cout << "shadow catalog tests passed\n";
    return failures == 0 ? 0 : 1;
}

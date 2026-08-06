#include "aoe/shadow_catalog.hpp"

namespace aoe {
namespace {

bool same_binding(
    const ExactShadowBinding& left,
    const ExactShadowBinding& right
) {
    return left.root_slp == right.root_slp &&
        left.shadow_slp == right.shadow_slp &&
        left.shadow_graphic == right.shadow_graphic &&
        left.offset_x == right.offset_x &&
        left.offset_y == right.offset_y &&
        left.display_angle == right.display_angle &&
        left.root_frame_count == right.root_frame_count &&
        left.root_angle_count == right.root_angle_count &&
        left.root_mirroring_mode == right.root_mirroring_mode &&
        left.shadow_frame_count == right.shadow_frame_count &&
        left.shadow_angle_count == right.shadow_angle_count &&
        left.shadow_mirroring_mode == right.shadow_mirroring_mode;
}

}  // namespace

std::optional<ExactShadowBinding>
find_exact_shadow_binding(
    std::span<const LegacyGraphic> graphics,
    std::int32_t root_slp
) {
    std::optional<ExactShadowBinding> result;
    for (const LegacyGraphic& root : graphics) {
        if (root.graphic_id < 0 || root.slp_id != root_slp) continue;
        std::optional<ExactShadowBinding> candidate;
        for (const LegacyGraphicDelta& delta : root.deltas) {
            if (delta.graphic_id < 0 ||
                static_cast<std::size_t>(delta.graphic_id) >=
                    graphics.size()) {
                return std::nullopt;
            }
            const LegacyGraphic& child =
                graphics[static_cast<std::size_t>(delta.graphic_id)];
            if (child.graphic_id < 0 || child.layer != 10) continue;
            if (candidate || child.slp_id < 0 ||
                child.player_color != -1 ||
                child.frame_count <= 0 ||
                child.angle_count <= 0 ||
                root.frame_count <= 0 ||
                root.angle_count <= 0 ||
                (child.frame_count != 1 &&
                 child.frame_count != root.frame_count)) {
                return std::nullopt;
            }
            candidate = ExactShadowBinding{
                root_slp,
                root.graphic_id,
                child.slp_id,
                child.graphic_id,
                delta.offset_x,
                delta.offset_y,
                delta.display_angle,
                root.frame_count,
                root.angle_count,
                root.mirroring_mode,
                child.frame_count,
                child.angle_count,
                child.mirroring_mode,
            };
        }
        if (!candidate) return std::nullopt;
        if (result && !same_binding(*result, *candidate)) {
            return std::nullopt;
        }
        result = candidate;
    }
    return result;
}

std::optional<ExactShadowBinding>
find_exact_shadow_binding(
    const LegacyDatFile& dat,
    std::int32_t root_slp
) {
    return find_exact_shadow_binding(dat.graphics(), root_slp);
}

}  // namespace aoe

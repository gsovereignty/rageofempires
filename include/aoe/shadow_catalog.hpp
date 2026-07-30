#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "aoe/legacy_dat.hpp"

namespace aoe {

struct ExactShadowBinding {
    std::int32_t root_slp{};
    std::int16_t root_graphic{};
    std::int32_t shadow_slp{};
    std::int16_t shadow_graphic{};
    std::int16_t offset_x{};
    std::int16_t offset_y{};
    std::int16_t display_angle{-1};
    std::int16_t root_frame_count{};
    std::int16_t root_angle_count{};
    std::int16_t shadow_frame_count{};
    std::int16_t shadow_angle_count{};
};

// Returns a binding only when every DAT graphic using root_slp agrees on one
// direct, non-player-colored layer-10 shadow child and its synchronization
// metadata. Ambiguity is a fallback, never a best-effort choice.
[[nodiscard]] std::optional<ExactShadowBinding>
find_exact_shadow_binding(
    std::span<const LegacyGraphic> graphics,
    std::int32_t root_slp
);

[[nodiscard]] std::optional<ExactShadowBinding>
find_exact_shadow_binding(
    const LegacyDatFile& dat,
    std::int32_t root_slp
);

}  // namespace aoe

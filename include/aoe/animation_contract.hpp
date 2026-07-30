#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aoe/game_rules.hpp"

namespace aoe::animation {

enum class State : std::uint8_t {
    idle,
    move,
    attack,
    gather_hunt,
    build,
    repair,
    building_work,
    building_attack,
};

enum class Layout : std::uint8_t {
    full,
    mirrored,
    ambiguous,
};

struct Binding {
    std::int32_t graphic_id{};
    std::int32_t slp_id{};
    std::int16_t frames_per_angle{};
    std::int16_t angle_count{};
    std::int32_t physical_frames{};
    double seconds_per_frame{};
    std::int32_t mirror_mode{};
    Layout layout{Layout::ambiguous};

    auto operator<=>(const Binding&) const = default;
};

[[nodiscard]] constexpr Layout classify_layout(
    std::int32_t frames_per_angle,
    std::int32_t angle_count,
    std::int32_t mirror_mode,
    std::int32_t physical_frames
) {
    if (frames_per_angle <= 0 || angle_count <= 0 || physical_frames <= 0) {
        return Layout::ambiguous;
    }
    if (physical_frames == frames_per_angle * angle_count) {
        return Layout::full;
    }
    if (
        mirror_mode != 0 &&
        physical_frames ==
            frames_per_angle * (angle_count / 2 + 1)
    ) {
        return Layout::mirrored;
    }
    return Layout::ambiguous;
}

[[nodiscard]] std::optional<Binding> binding(UnitKind kind, State state);

// Exact DAT duration/layout fields do not prove the HD/classic scheduler clock
// or logical-direction selector. Runtime fixed-tick modulo remains explicitly
// procedural until those paths are proved.
inline constexpr bool cadence_selector_proved = false;
inline constexpr bool direction_selector_proved = false;

}  // namespace aoe::animation

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

// Exact graphic roles recovered from the VER 5.7 DAT. Most are direct
// master-object fields. Villager work roles are task-graphic fields from the
// original Builder/Repairer object records. Roles stay distinct from runtime
// actions: choosing between walking and running, for example, requires
// original selector evidence rather than an art guess.
enum class Role : std::uint8_t {
    standing,
    alternate_standing,
    walking,
    running,
    attack,
    dying,
    construction,
    repair,
};

enum class ObjectCategory : std::uint8_t {
    unit,
    building,
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
    double replay_delay_seconds{};
    std::int32_t mirror_mode{};
    std::uint8_t sequence_type{};
    std::int32_t layer{};
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
[[nodiscard]] std::optional<Binding> binding(UnitKind kind, Role role);
[[nodiscard]] std::optional<Binding> binding(BuildingKind kind, Role role);
[[nodiscard]] std::optional<Binding> binding_for_dat_id(
    ObjectCategory category,
    std::uint16_t dat_id,
    Role role
);

struct ExactRoleBinding {
    ObjectCategory category{ObjectCategory::unit};
    std::uint16_t dat_id{};
    Role role{Role::standing};
    Binding art;
};

[[nodiscard]] std::span<const ExactRoleBinding> exact_role_bindings();

// Number of five-Hz world updates needed for the active attack graphic to
// reach the DAT attack-frame delay. A zero delay releases immediately.
[[nodiscard]] int attack_release_delay_ticks(UnitKind kind) noexcept;
[[nodiscard]] int attack_release_delay_ticks_for_dat_id(
    std::uint16_t object_id
) noexcept;

// The decompiled animated-object update proves the cadence scheduler. The
// logical-direction selector remains a separate unresolved contract.
inline constexpr bool cadence_selector_proved = true;
inline constexpr bool direction_selector_proved = false;

}  // namespace aoe::animation

#pragma once

#include "aoe/types.hpp"
#include <utility>

namespace aoe {

// Returns a live-DAT-backed command acknowledgement sound, or -1 when this
// represented unit must not borrow another line's acknowledgement.
[[nodiscard]] int accepted_command_sound(UnitKind kind) noexcept;
[[nodiscard]] int selected_sound(UnitKind kind) noexcept;
[[nodiscard]] int movement_sound(UnitKind kind) noexcept;
[[nodiscard]] int trained_sound(UnitKind kind) noexcept;
[[nodiscard]] int selected_sound(BuildingKind kind) noexcept;
[[nodiscard]] int scheduled_attack_sound(UnitKind kind) noexcept;
[[nodiscard]] int scheduled_death_sound(UnitKind kind) noexcept;
[[nodiscard]] int scheduled_death_animation_slp(UnitKind kind) noexcept;
[[nodiscard]] std::pair<int, int> scheduled_attack_animation(UnitKind kind) noexcept;
[[nodiscard]] bool scheduled_building_has_death_sound(BuildingKind kind) noexcept;

}  // namespace aoe

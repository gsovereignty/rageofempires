#pragma once

#include "aoe/types.hpp"

namespace aoe {

// Returns a live-DAT-backed command acknowledgement sound, or -1 when this
// represented unit must not borrow another line's acknowledgement.
[[nodiscard]] int accepted_command_sound(UnitKind kind) noexcept;
[[nodiscard]] int selected_sound(UnitKind kind) noexcept;
[[nodiscard]] int movement_sound(UnitKind kind) noexcept;
[[nodiscard]] int trained_sound(UnitKind kind) noexcept;
[[nodiscard]] int selected_sound(BuildingKind kind) noexcept;

}  // namespace aoe

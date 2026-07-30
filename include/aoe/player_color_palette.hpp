#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aoe/player_roster.hpp"

namespace aoe {

inline constexpr std::array<std::size_t, 8> player_color_palette_bases{
    16, 32, 48, 64, 96, 112, 128, 80,
};

inline constexpr std::size_t player_color_source_count = 10;

[[nodiscard]] constexpr std::optional<std::size_t>
resolve_player_color_palette_index(
    PlayerSlotId slot,
    std::uint8_t source_index
) noexcept {
    if (source_index >= player_color_source_count) return std::nullopt;
    if (slot.is_neutral()) {
        return static_cast<std::size_t>(source_index);
    }
    const auto index = slot.index();
    if (!index || *index >= player_color_palette_bases.size()) {
        return std::nullopt;
    }
    return player_color_palette_bases[*index] + source_index;
}

[[nodiscard]] constexpr std::optional<PlayerSlotId>
legacy_slp_player_slot(unsigned player) noexcept {
    if (player == 0) return PlayerSlotId::neutral();
    return PlayerSlotId::from_index(player - 1);
}

}  // namespace aoe

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "aoe/types.hpp"

namespace aoe {

// Stable format/protocol IDs. Never derive these values from Player ordinals.
inline constexpr int blue_player_wire_id = 0;
inline constexpr int red_player_wire_id = 1;
inline constexpr int neutral_player_wire_id = 2;
inline constexpr std::size_t legacy_playable_player_count = 2;

[[nodiscard]] constexpr bool is_playable_player(Player player) noexcept {
    return player == Player::blue || player == Player::red;
}

[[nodiscard]] constexpr bool is_valid_player(Player player) noexcept {
    return is_playable_player(player) || player == Player::neutral;
}

[[nodiscard]] constexpr std::optional<std::size_t> player_slot_index(
    Player player
) noexcept {
    if (player == Player::blue) return 0U;
    if (player == Player::red) return 1U;
    return std::nullopt;
}

[[nodiscard]] constexpr int encode_player_wire(Player player) noexcept {
    if (player == Player::blue) return blue_player_wire_id;
    if (player == Player::red) return red_player_wire_id;
    if (player == Player::neutral) return neutral_player_wire_id;
    return -1;
}

[[nodiscard]] constexpr std::optional<int> encode_legacy_entity_owner_wire(
    EntityOwner owner
) noexcept {
    const auto player = owner.legacy_player();
    return player
        ? std::optional<int>{encode_player_wire(*player)}
        : std::nullopt;
}

[[nodiscard]] constexpr std::optional<Player> decode_player_wire(
    int wire_id
) noexcept {
    if (wire_id == blue_player_wire_id) return Player::blue;
    if (wire_id == red_player_wire_id) return Player::red;
    if (wire_id == neutral_player_wire_id) return Player::neutral;
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view player_wire_name(
    Player player
) noexcept {
    if (player == Player::blue) return "blue";
    if (player == Player::red) return "red";
    if (player == Player::neutral) return "neutral";
    return {};
}

[[nodiscard]] constexpr std::optional<Player> decode_player_wire_name(
    std::string_view name
) noexcept {
    if (name == "blue") return Player::blue;
    if (name == "red") return Player::red;
    if (name == "neutral") return Player::neutral;
    return std::nullopt;
}

[[nodiscard]] constexpr bool is_playable_player_color(
    std::string_view color
) noexcept {
    return color == "blue" || color == "red";
}

}  // namespace aoe

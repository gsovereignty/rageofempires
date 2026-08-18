#include "aoe/ui_perspective.hpp"

namespace aoe {

Player multiplayer_local_player(bool hosting) {
    return hosting ? Player::blue : Player::red;
}

Player opposing_player(Player player) {
    return player == Player::blue ? Player::red : Player::blue;
}

bool belongs_to_local_view(Player owner, Player local_player) {
    return owner == local_player;
}

std::string multiplayer_slot_presence(
    Player slot,
    Player local_player,
    std::string_view peer_id
) {
    if (slot == local_player) return "LOCAL";
    if (peer_id.empty() || peer_id == "open") return "OPEN SLOT";
    constexpr std::size_t visible_identity_characters = 12;
    return "PEER " + std::string{peer_id.substr(
        0, visible_identity_characters
    )};
}

}  // namespace aoe

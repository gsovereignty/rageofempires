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

}  // namespace aoe

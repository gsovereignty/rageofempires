#pragma once

#include <string>
#include <string_view>

#include "aoe/simulation.hpp"

namespace aoe {

[[nodiscard]] Player multiplayer_local_player(bool hosting);
[[nodiscard]] Player opposing_player(Player player);
[[nodiscard]] bool belongs_to_local_view(Player owner, Player local_player);
[[nodiscard]] std::string multiplayer_slot_presence(
    Player slot,
    Player local_player,
    std::string_view peer_id
);

}  // namespace aoe

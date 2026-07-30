#pragma once

#include "aoe/simulation.hpp"

namespace aoe {

[[nodiscard]] Player multiplayer_local_player(bool hosting);
[[nodiscard]] Player opposing_player(Player player);
[[nodiscard]] bool belongs_to_local_view(Player owner, Player local_player);

}  // namespace aoe

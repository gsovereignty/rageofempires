#include <cstdlib>
#include <iostream>

#include "aoe/ui_perspective.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    require(
        aoe::multiplayer_local_player(true) == aoe::Player::blue,
        "host must own blue view"
    );
    require(
        aoe::multiplayer_local_player(false) == aoe::Player::red,
        "join must own red view"
    );
    require(
        aoe::opposing_player(aoe::Player::blue) == aoe::Player::red,
        "blue opponent must be red"
    );
    require(
        aoe::opposing_player(aoe::Player::red) == aoe::Player::blue,
        "red opponent must be blue"
    );
    require(
        aoe::belongs_to_local_view(
            aoe::Player::red, aoe::Player::red
        ),
        "red view must own red entities"
    );
    require(
        !aoe::belongs_to_local_view(
            aoe::Player::blue, aoe::Player::red
        ),
        "red view must not own blue entities"
    );
    require(
        aoe::multiplayer_slot_presence(
            aoe::Player::blue, aoe::Player::blue, "host-public-key"
        ) == "LOCAL",
        "local lobby slot must identify itself"
    );
    require(
        aoe::multiplayer_slot_presence(
            aoe::Player::red, aoe::Player::blue, "open"
        ) == "OPEN SLOT",
        "unoccupied lobby slot must remain visibly open"
    );
    require(
        aoe::multiplayer_slot_presence(
            aoe::Player::red,
            aoe::Player::blue,
            "1997a573ffd3ae140232209df7226f31"
        ) == "PEER 1997a573ffd3",
        "occupied lobby slot must show joined peer identity"
    );
    return 0;
}

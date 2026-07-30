#include "aoe/player_codec.hpp"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

}  // namespace

int main() {
    constexpr std::array<std::uint8_t, 3> golden_wire_bytes{
        static_cast<std::uint8_t>(
            aoe::encode_player_wire(aoe::Player::blue)
        ),
        static_cast<std::uint8_t>(
            aoe::encode_player_wire(aoe::Player::red)
        ),
        static_cast<std::uint8_t>(
            aoe::encode_player_wire(aoe::Player::neutral)
        ),
    };
    static_assert(
        golden_wire_bytes == std::array<std::uint8_t, 3>{0, 1, 2}
    );
    static_assert(aoe::encode_player_wire(aoe::Player::blue) == 0);
    static_assert(aoe::encode_player_wire(aoe::Player::red) == 1);
    static_assert(aoe::encode_player_wire(aoe::Player::neutral) == 2);
    static_assert(aoe::player_wire_name(aoe::Player::blue) == "blue");
    static_assert(aoe::player_wire_name(aoe::Player::red) == "red");
    static_assert(aoe::player_wire_name(aoe::Player::neutral) == "neutral");

    check(
        aoe::decode_player_wire(0) == aoe::Player::blue,
        "wire ID 0 remains blue"
    );
    check(
        aoe::decode_player_wire(1) == aoe::Player::red,
        "wire ID 1 remains red"
    );
    check(
        aoe::decode_player_wire(2) == aoe::Player::neutral,
        "wire ID 2 remains neutral"
    );
    check(!aoe::decode_player_wire(-1), "negative wire ID rejected");
    check(!aoe::decode_player_wire(3), "unknown wire ID rejected");
    check(
        aoe::decode_player_wire_name("blue") == aoe::Player::blue &&
            aoe::decode_player_wire_name("red") == aoe::Player::red &&
            aoe::decode_player_wire_name("neutral") == aoe::Player::neutral,
        "legacy text names remain stable"
    );
    check(
        !aoe::decode_player_wire_name("player3"),
        "unsupported slot name rejected"
    );
    check(
        aoe::player_slot_index(aoe::Player::blue) == 0U &&
            aoe::player_slot_index(aoe::Player::red) == 1U &&
            !aoe::player_slot_index(aoe::Player::neutral),
        "playable slot validation excludes neutral"
    );
    check(
        aoe::is_playable_player_color("blue") &&
            aoe::is_playable_player_color("red") &&
            !aoe::is_playable_player_color("neutral"),
        "playable color validation remains blue/red"
    );

    if (failures == 0) std::cout << "player codec tests passed\n";
    return failures == 0 ? 0 : 1;
}

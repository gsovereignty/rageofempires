#include "aoe/player_roster.hpp"

#include <iostream>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

aoe::MatchRosterSlot human_slot(
    std::size_t index,
    std::string id,
    bool cooperative = false
) {
    return {
        *aoe::PlayerSlotId::from_index(index),
        true,
        *aoe::TeamId::numbered(1),
        cooperative,
        {{std::move(id), aoe::RosterControllerKind::human}},
    };
}

}  // namespace

int main() {
    static_assert(aoe::PlayerSlotId::neutral().stable_id() == 8);
    static_assert(
        aoe::encode_player_slot_id(
            *aoe::PlayerSlotId::from_index(7)
        ) == 7
    );
    static_assert(aoe::player_slot_color(
        *aoe::PlayerSlotId::from_index(0)
    ) == "blue");
    static_assert(aoe::player_slot_color(
        *aoe::PlayerSlotId::from_index(7)
    ) == "orange");
    static_assert(
        aoe::player_slot_to_legacy(
            *aoe::PlayerSlotId::from_index(0)
        ) == aoe::Player::blue
    );
    static_assert(
        !aoe::player_slot_to_legacy(
            *aoe::PlayerSlotId::from_index(2)
        )
    );

    check(!aoe::PlayerSlotId::from_index(8), "slot 8 rejected");
    check(!aoe::decode_player_slot_id(-1), "negative slot rejected");
    check(!aoe::decode_player_slot_id(9), "unknown slot rejected");
    check(
        aoe::decode_player_slot_name("player3") ==
            aoe::PlayerSlotId::from_index(2) &&
            aoe::decode_player_slot_name("green") ==
                aoe::PlayerSlotId::from_index(2),
        "canonical name and color decode same slot"
    );
    check(!aoe::TeamId::numbered(0), "team zero requires none");
    check(!aoe::TeamId::numbered(5), "team five rejected");

    auto valid = aoe::MatchRoster::create({
        human_slot(0, "host"),
        {
            *aoe::PlayerSlotId::from_index(1),
            true,
            *aoe::TeamId::numbered(2),
            false,
            {{"ai-1", aoe::RosterControllerKind::computer}},
        },
    });
    check(valid.has_value(), "human/computer roster accepted");
    check(
        valid && valid->slot(
            *aoe::PlayerSlotId::from_index(0)
        ).occupied,
        "occupied slot retained"
    );

    auto cooperative = human_slot(2, "coop-a", true);
    cooperative.controllers.push_back({
        "coop-b", aoe::RosterControllerKind::human,
    });
    check(
        aoe::MatchRoster::create({cooperative}).has_value(),
        "explicit cooperative shared control accepted"
    );
    check(
        !aoe::MatchRoster::create({
            human_slot(0, "same"),
            human_slot(1, "same"),
        }),
        "controller ID uniqueness enforced"
    );
    check(
        !aoe::MatchRoster::create({
            human_slot(0, "first"),
            human_slot(0, "second"),
        }),
        "slot uniqueness enforced"
    );
    auto invalid_shared = human_slot(3, "solo", true);
    check(
        !aoe::MatchRoster::create({invalid_shared}),
        "cooperative flag requires multiple humans"
    );

    if (failures == 0) std::cout << "player roster tests passed\n";
    return failures == 0 ? 0 : 1;
}

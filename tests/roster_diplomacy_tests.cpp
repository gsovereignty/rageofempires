#include "aoe/roster_diplomacy.hpp"

#include <iostream>

namespace {

int failures{};

void check(bool condition, const char* message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

aoe::MatchRosterSlot slot(
    std::size_t index,
    int team,
    std::string controller
) {
    return {
        *aoe::PlayerSlotId::from_index(index),
        true,
        team == 0 ? aoe::TeamId::none()
                  : *aoe::TeamId::numbered(team),
        false,
        {{std::move(controller), aoe::RosterControllerKind::human}},
    };
}

}  // namespace

int main() {
    const auto one = *aoe::PlayerSlotId::from_index(0);
    const auto two = *aoe::PlayerSlotId::from_index(1);
    const auto three = *aoe::PlayerSlotId::from_index(2);

    aoe::RosterDiplomacy legacy =
        aoe::RosterDiplomacy::legacy_blue_red();
    check(
        legacy.stance(one, one) == aoe::Diplomacy::ally &&
            legacy.stance(one, two) == aoe::Diplomacy::enemy &&
            legacy.stance(two, one) == aoe::Diplomacy::enemy,
        "legacy blue/red defaults preserved"
    );
    check(
        legacy.rules().allied_victory &&
            legacy.rules().shared_vision,
        "legacy ally behavior flags preserved"
    );
    check(
        legacy.canonical_hash() ==
            "roster-diplomacy-fnv1a64:2e70a68f200a3dfd",
        "legacy canonical hash remains golden"
    );

    const auto roster = aoe::MatchRoster::create({
        slot(0, 1, "a"),
        slot(1, 1, "b"),
        slot(2, 2, "c"),
    });
    auto diplomacy = aoe::RosterDiplomacy::create(*roster);
    check(diplomacy.has_value(), "valid diplomacy table created");
    check(
        diplomacy->stance(one, two) == aoe::Diplomacy::ally &&
            diplomacy->stance(two, one) == aoe::Diplomacy::ally &&
            diplomacy->stance(one, three) == aoe::Diplomacy::enemy,
        "team initialization is symmetric"
    );
    check(
        diplomacy->shares_victory(one, two) &&
            diplomacy->shares_vision(one, two),
        "mutual allies share enabled victory and vision"
    );
    check(
        diplomacy->set_stance(one, two, aoe::Diplomacy::neutral) &&
            diplomacy->stance(two, one) == aoe::Diplomacy::ally,
        "directed stance mutation stays directed"
    );
    check(
        !diplomacy->shares_victory(one, two) &&
            !diplomacy->shares_vision(one, two),
        "directed stance affects viewer and mutual victory"
    );
    check(
        diplomacy->set_symmetric_stance(
            one, two, aoe::Diplomacy::ally
        ),
        "symmetric stance helper updates both directions"
    );
    check(
        !diplomacy->set_stance(
            one, one, aoe::Diplomacy::enemy
        ),
        "same-slot stance remains ally"
    );

    const std::string canonical = diplomacy->canonical();
    check(
        canonical ==
            aoe::RosterDiplomacy::create(*roster)->canonical(),
        "canonical serialization deterministic"
    );
    check(
        diplomacy->canonical_hash() ==
            aoe::RosterDiplomacy::create(*roster)->canonical_hash(),
        "canonical hash deterministic"
    );
    check(
        diplomacy->canonical_hash().starts_with(
            "roster-diplomacy-fnv1a64:"
        ),
        "canonical hash has stable algorithm tag"
    );

    auto cooperative_slot = slot(3, 0, "coop-a");
    cooperative_slot.cooperative_control = true;
    cooperative_slot.controllers.push_back({
        "coop-b", aoe::RosterControllerKind::human,
    });
    const auto cooperative_roster =
        aoe::MatchRoster::create({cooperative_slot});
    const auto cooperative =
        aoe::RosterDiplomacy::create(*cooperative_roster);
    const auto four = *aoe::PlayerSlotId::from_index(3);
    check(
        cooperative->cooperative_control(four) &&
            cooperative->stance(four, four) ==
                aoe::Diplomacy::ally &&
            cooperative->shares_victory(four, four) &&
            cooperative->shares_vision(four, four),
        "cooperative controllers share same-slot semantics"
    );

    if (failures == 0) {
        std::cout << "roster diplomacy tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

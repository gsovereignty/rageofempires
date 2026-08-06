#include "aoe/animation_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main() {
    using aoe::UnitKind;
    using aoe::animation::Layout;
    using aoe::animation::State;

    assert(aoe::animation::classify_layout(15, 8, 6, 75) ==
           Layout::mirrored);
    assert(aoe::animation::classify_layout(10, 8, 0, 80) ==
           Layout::full);
    assert(aoe::animation::classify_layout(10, 8, 6, 52) ==
           Layout::ambiguous);

    const auto villager_move =
        aoe::animation::binding(UnitKind::villager, State::move);
    assert(villager_move);
    assert(villager_move->graphic_id == 1288);
    assert(villager_move->slp_id == 1484);
    assert(villager_move->frames_per_angle == 15);
    assert(villager_move->layout == Layout::mirrored);

    assert(aoe::animation::binding(UnitKind::militia, State::attack)->slp_id ==
           987);
    assert(aoe::animation::binding(UnitKind::knight, State::move)->slp_id ==
           673);
    assert(aoe::animation::binding(UnitKind::archer, State::idle)->layout ==
           Layout::ambiguous);
    assert(!aoe::animation::binding(UnitKind::villager, State::build));
    assert(aoe::animation::cadence_selector_proved);
    assert(aoe::animation::attack_release_delay_ticks(UnitKind::knight) == 0);
    assert(aoe::animation::attack_release_delay_ticks(UnitKind::archer) == 2);
    assert(aoe::animation::attack_release_delay_ticks(UnitKind::skirmisher) == 3);
    assert(
        aoe::animation::attack_release_delay_ticks(
            UnitKind::throwing_axeman
        ) == 7
    );
    assert(
        aoe::animation::attack_release_delay_ticks(
            UnitKind::bombard_cannon
        ) == 2
    );
    assert(aoe::animation::attack_release_delay_ticks_for_dat_id(4) == 2);
    assert(aoe::animation::attack_release_delay_ticks_for_dat_id(74) == 0);
    assert(!aoe::animation::direction_selector_proved);
}

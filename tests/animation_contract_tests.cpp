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
    assert(!aoe::animation::binding(UnitKind::archer, State::idle));
    const auto villager_build =
        aoe::animation::binding(UnitKind::villager, State::build);
    assert(villager_build && villager_build->graphic_id == 1598 &&
           villager_build->slp_id == 1496);
    const auto villager_repair =
        aoe::animation::binding(UnitKind::villager, State::repair);
    assert(villager_repair == villager_build);
    assert(aoe::animation::exact_role_bindings().size() == 414);
    const auto king = aoe::animation::binding(
        UnitKind::king, aoe::animation::Role::standing
    );
    assert(king && king->graphic_id == 1851 && king->slp_id == 1767);
    assert(king->replay_delay_seconds == 1.0);
    const auto town_center = aoe::animation::binding(
        aoe::BuildingKind::town_center,
        aoe::animation::Role::construction
    );
    assert(town_center && town_center->graphic_id == 121 &&
           town_center->slp_id == 239);
    assert(!aoe::animation::binding(
        UnitKind::sheep, aoe::animation::Role::attack
    ));
    assert(aoe::animation::cadence_selector_proved);
    assert(aoe::animation::direction_selector_proved);
    const aoe::TilePosition origin{4, 4};
    const aoe::TilePosition directions[] = {
        {5, 5}, {4, 5}, {3, 5}, {3, 4},
        {3, 3}, {4, 3}, {5, 3}, {5, 4},
    };
    const std::size_t frames[] = {2, 1, 0, 1, 2, 3, 4, 3};
    const bool flips[] = {true, true, false, false,
                          false, false, false, true};
    for (int logical = 0; logical < 8; ++logical) {
        assert(aoe::animation::logical_direction(
                   origin, directions[logical], 8
               ) == logical);
        const auto selected = aoe::animation::select_frame(
            logical, 0, 1, 8, 6, 5
        );
        assert(selected && selected->frame_index == frames[logical]);
        assert(selected->flip_horizontal == flips[logical]);
    }
    assert(!aoe::animation::logical_direction(origin, origin, 8));
    assert(aoe::animation::logical_direction(origin, {5, 4}, 16) == 14);
    assert((aoe::animation::select_frame(0, 2, 3, 2, 1, 3) ==
            aoe::animation::FrameSelection{2, false}));
    assert((aoe::animation::select_frame(7, 2, 3, 2, 1, 3) ==
            aoe::animation::FrameSelection{2, true}));
    assert(aoe::animation::scale_logical_angle(3, 8, 16) == 6);
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
}

#include "aoe/animation_contract.hpp"

#include <algorithm>
#include <cmath>

namespace aoe::animation {
namespace {

struct UnitDatId {
    UnitKind kind;
    std::uint16_t dat_id;
};

struct BuildingDatId {
    BuildingKind kind;
    std::uint16_t dat_id;
};

#include "../generated/animation_binding_catalog.inc"

static_assert(std::size(generated_unit_dat_ids) == unit_kind_count);

template<typename Kind, typename Record, std::size_t Size>
std::optional<std::uint16_t> dat_id_for(
    Kind kind,
    const Record (&records)[Size]
) {
    const auto found = std::ranges::find(records, kind, &Record::kind);
    return found == std::end(records)
        ? std::nullopt
        : std::optional<std::uint16_t>{found->dat_id};
}

}  // namespace

std::span<const ExactRoleBinding> exact_role_bindings() {
    return generated_exact_role_bindings;
}

std::optional<Binding> binding_for_dat_id(
    ObjectCategory category,
    std::uint16_t dat_id,
    Role role
) {
    const auto found = std::ranges::find_if(
        generated_exact_role_bindings,
        [category, dat_id, role](const ExactRoleBinding& candidate) {
            return candidate.category == category &&
                candidate.dat_id == dat_id && candidate.role == role;
        }
    );
    return found == std::end(generated_exact_role_bindings)
        ? std::nullopt
        : std::optional<Binding>{found->art};
}

std::optional<Binding> binding(UnitKind kind, Role role) {
    const auto dat_id = dat_id_for(kind, generated_unit_dat_ids);
    return dat_id
        ? binding_for_dat_id(ObjectCategory::unit, *dat_id, role)
        : std::nullopt;
}

std::optional<Binding> binding(BuildingKind kind, Role role) {
    const auto dat_id = dat_id_for(kind, generated_building_dat_ids);
    return dat_id
        ? binding_for_dat_id(ObjectCategory::building, *dat_id, role)
        : std::nullopt;
}

std::optional<Binding> binding(UnitKind kind, State state) {
    if (kind == UnitKind::villager && state == State::gather_hunt) {
        return Binding{
            1602, 1528, 15, 8, 75,
            0.10000000149011612, 0.0, 6, 7, 20,
            Layout::mirrored,
        };
    }
    switch (state) {
        case State::idle: return binding(kind, Role::standing);
        case State::move: return binding(kind, Role::walking);
        case State::attack: return binding(kind, Role::attack);
        case State::build: return binding(kind, Role::construction);
        case State::repair: return binding(kind, Role::repair);
        case State::gather_hunt:
        case State::building_work:
        case State::building_attack:
            return std::nullopt;
    }
    return std::nullopt;
}

int attack_release_delay_ticks(UnitKind kind) noexcept {
    // generated/animation_evidence.json records the exact VER 5.7 DAT attack
    // frame delay and attack-graphic seconds/frame. FUN_00407910 releases the
    // attack only after the active graphic reaches that frame. The world
    // scheduler supplies 0.2 seconds per authoritative update.
    int frame_delay{};
    double seconds_per_frame{};
    switch (kind) {
        case UnitKind::archer:
        case UnitKind::crossbowman:
            frame_delay = 5; seconds_per_frame = 0.07000000029802322; break;
        case UnitKind::arbalester:
            frame_delay = 5; seconds_per_frame = 0.07000000029802322; break;
        case UnitKind::skirmisher:
        case UnitKind::elite_skirmisher:
            frame_delay = 5; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::longbowman:
        case UnitKind::elite_longbowman:
        case UnitKind::plumed_archer:
        case UnitKind::elite_plumed_archer:
            frame_delay = 5; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::throwing_axeman:
            frame_delay = 12; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::elite_throwing_axeman:
            frame_delay = 8; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::chu_ko_nu:
        case UnitKind::elite_chu_ko_nu:
            frame_delay = 3; seconds_per_frame = 0.07000000029802322; break;
        case UnitKind::mameluke:
            frame_delay = 6; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::janissary:
        case UnitKind::conquistador:
        case UnitKind::elite_conquistador:
            frame_delay = 4; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::mangudai:
        case UnitKind::cavalry_archer:
        case UnitKind::heavy_cavalry_archer:
            frame_delay = 10; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::scorpion:
        case UnitKind::heavy_scorpion:
        case UnitKind::bombard_cannon:
            frame_delay = 7; seconds_per_frame = 0.029999999329447746; break;
        case UnitKind::trebuchet:
            frame_delay = 6; seconds_per_frame = 0.10000000149011612; break;
        case UnitKind::hand_cannoneer:
            frame_delay = 5; seconds_per_frame = 0.07000000029802322; break;
        default:
            return 0;
    }
    return static_cast<int>(std::ceil(
        static_cast<double>(frame_delay) * seconds_per_frame / 0.2
    ));
}

int attack_release_delay_ticks_for_dat_id(
    std::uint16_t object_id
) noexcept {
    switch (object_id) {
        case 4: return attack_release_delay_ticks(UnitKind::archer);
        case 7: return attack_release_delay_ticks(UnitKind::skirmisher);
        case 24: return attack_release_delay_ticks(UnitKind::crossbowman);
        case 492: return attack_release_delay_ticks(UnitKind::arbalester);
        case 6: return attack_release_delay_ticks(UnitKind::elite_skirmisher);
        case 8: return attack_release_delay_ticks(UnitKind::longbowman);
        case 530: return attack_release_delay_ticks(UnitKind::elite_longbowman);
        case 281: return attack_release_delay_ticks(UnitKind::throwing_axeman);
        case 531:
            return attack_release_delay_ticks(UnitKind::elite_throwing_axeman);
        case 73: return attack_release_delay_ticks(UnitKind::chu_ko_nu);
        case 559: return attack_release_delay_ticks(UnitKind::elite_chu_ko_nu);
        case 282: return attack_release_delay_ticks(UnitKind::mameluke);
        case 46: return attack_release_delay_ticks(UnitKind::janissary);
        case 11: return attack_release_delay_ticks(UnitKind::mangudai);
        case 763: return attack_release_delay_ticks(UnitKind::plumed_archer);
        case 765:
            return attack_release_delay_ticks(UnitKind::elite_plumed_archer);
        case 771: return attack_release_delay_ticks(UnitKind::conquistador);
        case 773:
            return attack_release_delay_ticks(UnitKind::elite_conquistador);
        case 279: return attack_release_delay_ticks(UnitKind::scorpion);
        case 542: return attack_release_delay_ticks(UnitKind::heavy_scorpion);
        case 42: return attack_release_delay_ticks(UnitKind::trebuchet);
        case 39: return attack_release_delay_ticks(UnitKind::cavalry_archer);
        case 474:
            return attack_release_delay_ticks(UnitKind::heavy_cavalry_archer);
        case 5: return attack_release_delay_ticks(UnitKind::hand_cannoneer);
        case 36: return attack_release_delay_ticks(UnitKind::bombard_cannon);
        default: return 0;
    }
}

}  // namespace aoe::animation

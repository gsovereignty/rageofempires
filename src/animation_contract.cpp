#include "aoe/animation_contract.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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

std::optional<std::uint8_t> logical_direction(
    TilePosition from,
    TilePosition to,
    std::int32_t angle_count
) noexcept {
    if (angle_count <= 0 || angle_count > 256 || from == to) {
        return std::nullopt;
    }
    const double dx = static_cast<double>(to.x - from.x);
    const double dy = static_cast<double>(to.y - from.y);
    double radians = std::atan2(dy - dx, dx + dy);
    if (radians < 0.0) radians += 2.0 * std::numbers::pi;
    const double step = 2.0 * std::numbers::pi /
        static_cast<double>(angle_count);
    const auto angle = static_cast<std::int32_t>(
        std::floor(radians / step + 0.5)
    ) % angle_count;
    return static_cast<std::uint8_t>(angle);
}

std::optional<FrameSelection> select_frame(
    std::int32_t logical_angle,
    std::int32_t action_frame,
    std::int32_t frames_per_angle,
    std::int32_t angle_count,
    std::int32_t mirroring_mode,
    std::size_t physical_frame_count
) noexcept {
    if (frames_per_angle <= 0 || angle_count <= 0 || logical_angle < 0 ||
        logical_angle > 255 || action_frame < 0 ||
        action_frame >= frames_per_angle) {
        return std::nullopt;
    }
    const std::size_t expected_frames = mirroring_mode == 0
        ? static_cast<std::size_t>(frames_per_angle) *
              static_cast<std::size_t>(angle_count)
        : angle_count == 2
            ? static_cast<std::size_t>(frames_per_angle)
            : mirroring_mode >= angle_count / 4 &&
                  mirroring_mode < angle_count
                ? static_cast<std::size_t>(frames_per_angle) *
                      static_cast<std::size_t>(
                          mirroring_mode - angle_count / 4 + 1
                      )
                : 0U;
    if (physical_frame_count != expected_frames) return std::nullopt;
    if (mirroring_mode != 0 && angle_count == 2) {
        return FrameSelection{
            static_cast<std::size_t>(action_frame), logical_angle != 0
        };
    }
    if (logical_angle >= angle_count) return std::nullopt;
    std::int32_t stored_angle = logical_angle;
    bool flip = false;
    if (mirroring_mode != 0) {
        const std::int32_t quarter = angle_count / 4;
        if (logical_angle > mirroring_mode || logical_angle < quarter) {
            const std::int32_t half = angle_count / 2;
            stored_angle = (logical_angle > half
                ? half - logical_angle + angle_count
                : half - logical_angle) - quarter;
            flip = true;
        } else {
            stored_angle = logical_angle - quarter;
        }
    }
    if (stored_angle < 0) return std::nullopt;
    const std::size_t frame = static_cast<std::size_t>(stored_angle) *
        static_cast<std::size_t>(frames_per_angle) +
        static_cast<std::size_t>(action_frame);
    if (frame >= physical_frame_count) return std::nullopt;
    return FrameSelection{frame, flip};
}

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

#include "aoe/animation_contract.hpp"

#include <cmath>

namespace aoe::animation {
namespace {

Binding make(
    std::int32_t graphic,
    std::int32_t slp,
    std::int16_t frames,
    double duration,
    std::int32_t physical
) {
    return {
        graphic, slp, frames, 8, physical, duration, 6,
        classify_layout(frames, 8, 6, physical),
    };
}

}  // namespace

std::optional<Binding> binding(UnitKind kind, State state) {
    switch (kind) {
    case UnitKind::villager:
        switch (state) {
        case State::idle: return make(1284, 1479, 15, 1.0, 75);
        case State::move: return make(1288, 1484, 15, 0.0697500035, 75);
        case State::attack: return make(1278, 1473, 15, 0.0700000003, 75);
        case State::gather_hunt:
            return make(1602, 1528, 15, 0.1000000015, 75);
        default: return std::nullopt;
        }
    case UnitKind::militia:
        switch (state) {
        case State::idle: return make(1102, 993, 6, 1.0, 30);
        case State::move: return make(1106, 997, 12, 0.0697500035, 60);
        case State::attack: return make(1096, 987, 10, 0.1000000015, 50);
        default: return std::nullopt;
        }
    case UnitKind::archer:
        switch (state) {
        case State::idle:
            return Binding{
                633, 8, 10, 8, 52, 0.200000003, 6,
                Layout::ambiguous,
            };
        case State::move: return make(637, 12, 10, 0.0697500035, 50);
        case State::attack: return make(627, 2, 10, 0.0700000003, 50);
        default: return std::nullopt;
        }
    case UnitKind::knight:
        switch (state) {
        case State::idle: return make(933, 669, 10, 0.200000003, 50);
        case State::move: return make(937, 673, 10, 0.1099999994, 50);
        case State::attack: return make(927, 663, 10, 0.1350000054, 50);
        default: return std::nullopt;
        }
    case UnitKind::king:
        switch (state) {
        case State::idle: return make(1851, 1767, 6, 1.0, 30);
        case State::move: return make(1855, 1771, 10, 0.1, 50);
        default: return std::nullopt;
        }
    default:
        return std::nullopt;
    }
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

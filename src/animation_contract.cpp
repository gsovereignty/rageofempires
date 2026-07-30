#include "aoe/animation_contract.hpp"

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
    default:
        return std::nullopt;
    }
}

}  // namespace aoe::animation

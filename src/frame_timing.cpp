#include "aoe/frame_timing.hpp"

#include <algorithm>
#include <stdexcept>

namespace aoe {

std::chrono::milliseconds authoritative_tick_duration(
    SinglePlayerSpeed speed,
    std::optional<int> multiplayer_cadence_ms
) {
    if (multiplayer_cadence_ms) {
        if (*multiplayer_cadence_ms <= 0) {
            throw std::invalid_argument(
                "multiplayer tick cadence must be positive"
            );
        }
        return std::chrono::milliseconds(*multiplayer_cadence_ms);
    }
    switch (speed) {
        case SinglePlayerSpeed::slow:
            return std::chrono::milliseconds(400);
        case SinglePlayerSpeed::normal:
            return std::chrono::milliseconds(200);
        case SinglePlayerSpeed::fast:
            return std::chrono::milliseconds(100);
    }
    return std::chrono::milliseconds(200);
}

void FixedStepAccumulator::add(FrameDuration elapsed) {
    if (elapsed > FrameDuration::zero()) {
        accumulated_ += elapsed;
    }
}

bool FixedStepAccumulator::step_due(
    std::chrono::milliseconds tick_duration
) const {
    return accumulated_ >= tick_duration;
}

void FixedStepAccumulator::consume_step(
    std::chrono::milliseconds tick_duration
) {
    if (tick_duration <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("tick duration must be positive");
    }
    if (accumulated_ < tick_duration) {
        throw std::logic_error("no fixed simulation step is due");
    }
    accumulated_ -= tick_duration;
}

void FixedStepAccumulator::reset() {
    accumulated_ = FrameDuration::zero();
}

float FixedStepAccumulator::interpolation_alpha(
    std::chrono::milliseconds tick_duration
) const {
    if (tick_duration <= std::chrono::milliseconds::zero()) {
        return 1.0F;
    }
    return std::clamp(
        std::chrono::duration<float>(accumulated_).count() /
            std::chrono::duration<float>(tick_duration).count(),
        0.0F,
        1.0F
    );
}

FrameDuration FixedStepAccumulator::remainder() const {
    return accumulated_;
}

std::uint64_t unit_animation_tick_from_milliseconds(
    std::uint64_t elapsed_ms,
    std::uint64_t unit_id
) {
    // Legacy animation selection advances once per two animation ticks.
    // A 50 ms source tick therefore gives walking art a 100 ms frame period.
    return elapsed_ms / 50U + unit_id * 2U;
}

}  // namespace aoe

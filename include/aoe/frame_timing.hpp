#pragma once

#include "aoe/settings.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace aoe {

using FrameDuration = std::chrono::steady_clock::duration;

[[nodiscard]] std::chrono::milliseconds authoritative_tick_duration(
    SinglePlayerSpeed speed,
    std::optional<int> multiplayer_cadence_ms = std::nullopt
);

class FixedStepAccumulator {
public:
    void add(FrameDuration elapsed);
    [[nodiscard]] bool step_due(std::chrono::milliseconds tick_duration) const;
    void consume_step(std::chrono::milliseconds tick_duration);
    void reset();

    [[nodiscard]] float interpolation_alpha(
        std::chrono::milliseconds tick_duration
    ) const;
    [[nodiscard]] FrameDuration remainder() const;

private:
    FrameDuration accumulated_{};
};

[[nodiscard]] std::uint64_t unit_animation_tick_from_milliseconds(
    std::uint64_t elapsed_ms,
    std::uint64_t unit_id
);

[[nodiscard]] std::uint64_t unit_animation_frame_from_milliseconds(
    std::uint64_t elapsed_ms,
    std::uint64_t unit_id
);

// The original animated-object path consumes world delta, so animation time
// follows authoritative five-Hz world updates rather than wall-clock cadence.
[[nodiscard]] std::uint64_t simulation_animation_time_milliseconds(
    std::uint64_t completed_ticks,
    float interpolation_alpha
);

}  // namespace aoe

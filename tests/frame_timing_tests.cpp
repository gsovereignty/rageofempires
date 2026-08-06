#include "aoe/frame_timing.hpp"

#include <chrono>
#include <iostream>

namespace {

int failures{};

void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

}  // namespace

int main() {
    using namespace std::chrono_literals;

    expect(
        aoe::authoritative_tick_duration(
            aoe::SinglePlayerSpeed::slow
        ) == 400ms,
        "slow cadence"
    );
    expect(
        aoe::authoritative_tick_duration(
            aoe::SinglePlayerSpeed::normal
        ) == 200ms,
        "normal cadence"
    );
    expect(
        aoe::authoritative_tick_duration(
            aoe::SinglePlayerSpeed::fast
        ) == 100ms,
        "fast cadence"
    );
    expect(
        aoe::authoritative_tick_duration(
            aoe::SinglePlayerSpeed::slow, 75
        ) == 75ms,
        "multiplayer cadence overrides local speed"
    );

    aoe::FixedStepAccumulator accumulator;
    accumulator.add(450ms);
    int steps{};
    while (accumulator.step_due(200ms)) {
        accumulator.consume_step(200ms);
        ++steps;
    }
    expect(steps == 2, "late frame catches up every complete step");
    expect(accumulator.remainder() == 50ms, "catch-up preserves remainder");
    expect(
        accumulator.interpolation_alpha(200ms) > 0.249F &&
            accumulator.interpolation_alpha(200ms) < 0.251F,
        "alpha derives from remainder and cadence"
    );

    accumulator.reset();
    accumulator.add(50ms);
    expect(
        accumulator.interpolation_alpha(100ms) > 0.499F &&
            accumulator.interpolation_alpha(100ms) < 0.501F,
        "fast cadence uses full interpolation range"
    );

    expect(
        aoe::unit_animation_tick_from_milliseconds(50, 3) == 7,
        "composite animation retains 50 ms source tick"
    );
    expect(
        aoe::unit_animation_tick_from_milliseconds(100, 3) == 8,
        "composite animation source advances every 50 ms"
    );

    expect(
        aoe::unit_animation_frame_from_milliseconds(50, 3) == 3,
        "direct unit animation retains frame at 50 ms"
    );
    expect(
        aoe::unit_animation_frame_from_milliseconds(99, 3) == 3,
        "direct unit animation retains frame before 100 ms boundary"
    );
    expect(
        aoe::unit_animation_frame_from_milliseconds(100, 3) == 4,
        "direct unit animation advances at 100 ms boundary"
    );
    expect(
        aoe::unit_animation_frame_from_milliseconds(99, 9) == 9 &&
            aoe::unit_animation_frame_from_milliseconds(100, 9) == 10,
        "direct unit animation preserves stable per-unit phase"
    );
    expect(
        aoe::simulation_animation_time_milliseconds(4, 0.5F) == 900,
        "animation time follows five-Hz world time plus interpolation"
    );
    expect(
        aoe::simulation_animation_time_milliseconds(4, -1.0F) == 800 &&
            aoe::simulation_animation_time_milliseconds(4, 2.0F) == 1000,
        "animation interpolation is bounded to one world update"
    );

    if (failures == 0) {
        std::cout << "frame timing tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

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
        aoe::unit_animation_tick_from_milliseconds(99, 3) == 7,
        "animation retains stable per-unit phase before boundary"
    );
    expect(
        aoe::unit_animation_tick_from_milliseconds(100, 3) == 8,
        "animation advances from elapsed milliseconds"
    );

    if (failures == 0) {
        std::cout << "frame timing tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

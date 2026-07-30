#pragma once

#include <compare>
#include <cstdint>

namespace aoe::cursor {

inline constexpr std::int32_t resource_id = 51000;
inline constexpr std::uint8_t frame_count = 19;

enum class State : std::uint8_t {
    normal,
    select,
    move,
    attack,
    gather,
    build,
    repair,
    heal,
    convert,
    invalid,
    scroll_north,
    scroll_north_east,
    scroll_east,
    scroll_south_east,
    scroll_south,
    scroll_south_west,
    scroll_west,
    scroll_north_west,
    modal_busy,
};

enum class Evidence : std::uint8_t {
    exact_executable_selector,
    unknown_fallback,
};

struct Selection {
    std::uint8_t frame{};
    Evidence evidence{Evidence::unknown_fallback};
    std::uint32_t cadence_ms{};

    auto operator<=>(const Selection&) const = default;
};

// FUN_004dcca0 directly selects one of 19 SLP records and has no cadence
// logic. FUN_005b2f20 proves normal/IDC_ARROW -> 0; FUN_005b2ec0 proves
// modal busy/IDC_WAIT -> 6.
[[nodiscard]] constexpr Selection select(State state) {
    switch (state) {
    case State::normal:
        return {0, Evidence::exact_executable_selector, 0};
    case State::modal_busy:
        return {6, Evidence::exact_executable_selector, 0};
    default:
        return {0, Evidence::unknown_fallback, 0};
    }
}

[[nodiscard]] constexpr bool is_proved(State state) {
    return select(state).evidence == Evidence::exact_executable_selector;
}

}  // namespace aoe::cursor

#include "aoe/cursor_contract.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>

namespace {

using aoe::cursor::Evidence;
using aoe::cursor::State;

void exact_executable_selectors_are_pinned() {
    const auto normal = aoe::cursor::select(State::normal);
    assert(normal.frame == 0);
    assert(normal.evidence == Evidence::exact_executable_selector);
    assert(normal.cadence_ms == 0);

    const auto busy = aoe::cursor::select(State::modal_busy);
    assert(busy.frame == 6);
    assert(busy.evidence == Evidence::exact_executable_selector);
    assert(busy.cadence_ms == 0);
}

void unproved_gameplay_states_fail_closed() {
    constexpr std::array unknown{
        State::select,
        State::move,
        State::attack,
        State::gather,
        State::build,
        State::repair,
        State::heal,
        State::convert,
        State::invalid,
        State::scroll_north,
        State::scroll_north_east,
        State::scroll_east,
        State::scroll_south_east,
        State::scroll_south,
        State::scroll_south_west,
        State::scroll_west,
        State::scroll_north_west,
    };
    for (const State state : unknown) {
        const auto selection = aoe::cursor::select(state);
        assert(selection.frame == 0);
        assert(selection.evidence == Evidence::unknown_fallback);
        assert(selection.cadence_ms == 0);
        assert(!aoe::cursor::is_proved(state));
    }
}

}  // namespace

int main() {
    exact_executable_selectors_are_pinned();
    unproved_gameplay_states_fail_closed();
}

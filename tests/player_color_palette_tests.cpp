#include "aoe/player_color_palette.hpp"

#include <iostream>

namespace {
int failures{};

void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    constexpr std::array<std::size_t, 8> expected{
        16, 32, 48, 64, 96, 112, 128, 80,
    };
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto slot = aoe::PlayerSlotId::from_index(index);
        expect(slot.has_value(), "playable slot exists");
        if (!slot) continue;
        for (std::uint8_t source = 0; source < 10; ++source) {
            expect(
                aoe::resolve_player_color_palette_index(*slot, source) ==
                    expected[index] + source,
                "exact playable ramp index"
            );
        }
    }
    expect(
        aoe::resolve_player_color_palette_index(
            aoe::PlayerSlotId::neutral(), 7
        ) == 7,
        "neutral source index unchanged"
    );
    expect(
        !aoe::resolve_player_color_palette_index(
            *aoe::PlayerSlotId::from_index(2), 10
        ),
        "unsupported source index fails closed"
    );
    expect(
        aoe::legacy_slp_player_slot(1) ==
            aoe::PlayerSlotId::from_index(0),
        "legacy blue stays slot zero"
    );
    expect(
        aoe::legacy_slp_player_slot(2) ==
            aoe::PlayerSlotId::from_index(1),
        "legacy red stays slot one"
    );
    expect(
        !aoe::legacy_slp_player_slot(9),
        "unsupported legacy player never aliases red"
    );
    return failures == 0 ? 0 : 1;
}

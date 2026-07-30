#pragma once

#include <compare>
#include <cstdint>
#include <optional>

#include "aoe/game_rules.hpp"

namespace aoe::ui_icons {

enum class Evidence : std::uint8_t {
    exact_executable_dispatch,
    unknown,
};

struct Binding {
    std::int32_t sheet{};
    std::int32_t frame{};
    Evidence evidence{Evidence::unknown};

    auto operator<=>(const Binding&) const = default;
};

inline constexpr std::int32_t command_sheet = 50721;
inline constexpr std::int32_t technology_sheet = 50729;
inline constexpr std::int32_t unit_sheet = 50730;
inline constexpr std::int32_t normal_chrome_frame = 36;
inline constexpr std::int32_t pressed_chrome_frame = 37;

struct ButtonVisual {
    std::int32_t chrome_frame{};
    std::int32_t icon_frame{};
    std::int32_t icon_offset_x{};
    std::int32_t icon_offset_y{};

    auto operator<=>(const ButtonVisual&) const = default;
};

[[nodiscard]] constexpr ButtonVisual button_visual(
    bool pressed,
    std::int32_t icon_frame
) {
    return {
        pressed ? pressed_chrome_frame : normal_chrome_frame,
        icon_frame,
        pressed ? 1 : 0,
        pressed ? 1 : 0,
    };
}

[[nodiscard]] std::optional<Binding> technology(std::int32_t dat_icon_id);
[[nodiscard]] std::optional<Binding> ordinary_unit(std::int32_t dat_icon_id);
[[nodiscard]] std::optional<Binding> training_unit(UnitKind kind);

}  // namespace aoe::ui_icons

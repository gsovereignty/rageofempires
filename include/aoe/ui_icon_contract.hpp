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

[[nodiscard]] std::optional<Binding> technology(std::int32_t dat_icon_id);
[[nodiscard]] std::optional<Binding> technology_icon(Technology technology);
[[nodiscard]] std::optional<Binding> ordinary_unit(std::int32_t dat_icon_id);
[[nodiscard]] std::optional<Binding> training_unit(UnitKind kind);

}  // namespace aoe::ui_icons

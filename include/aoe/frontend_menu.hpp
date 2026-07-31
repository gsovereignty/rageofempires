#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace aoe {

enum class FrontendMenuScreen {
    main_menu,
    single_player_menu,
    random_map_setup,
    campaign_browser,
    custom_campaign_browser,
    custom_scenario_browser,
    saved_game_browser,
};

enum class FrontendMenuCommand {
    none,
    learn_to_play,
    open_single_player,
    open_history,
    open_multiplayer,
    open_map_editor,
    open_options,
    show_zone_unavailable,
    exit_game,
    open_campaigns,
    open_random_map,
    open_regicide,
    open_death_match,
    open_custom_campaign,
    open_custom_scenario,
    open_saved_game,
    close_flyout,
};

struct FrontendMenuRect {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] bool contains(float point_x, float point_y) const;
    auto operator<=>(const FrontendMenuRect&) const = default;
};

struct FrontendMenuItem {
    std::string_view label;
    FrontendMenuRect bounds;
    FrontendMenuCommand command{FrontendMenuCommand::none};
    bool enabled{true};
    std::string_view help;
};

struct FrontendLogicalTransform {
    float scale{1.0F};
    float offset_x{};
    float offset_y{};
    int logical_width{800};
    int logical_height{600};

    [[nodiscard]] std::optional<std::array<float, 2>>
    window_to_logical(float window_x, float window_y) const;
};

struct FrontendMenuActivation {
    FrontendMenuScreen screen{FrontendMenuScreen::main_menu};
    FrontendMenuCommand command{FrontendMenuCommand::none};
    bool activate{};
};

inline constexpr int frontend_logical_width = 800;
inline constexpr int frontend_logical_height = 600;

[[nodiscard]] FrontendLogicalTransform frontend_logical_transform(
    int window_width,
    int window_height
);
[[nodiscard]] std::span<const FrontendMenuItem> main_menu_items();
[[nodiscard]] std::span<const FrontendMenuItem> single_player_menu_items();
[[nodiscard]] std::span<const FrontendMenuItem> frontend_menu_items(
    FrontendMenuScreen screen
);
[[nodiscard]] std::optional<std::size_t> frontend_menu_hit_test(
    FrontendMenuScreen screen,
    float logical_x,
    float logical_y
);
[[nodiscard]] std::size_t move_frontend_menu_focus(
    FrontendMenuScreen screen,
    std::size_t current,
    int delta
);
[[nodiscard]] FrontendMenuActivation activate_frontend_menu_item(
    FrontendMenuScreen screen,
    std::size_t index
);
[[nodiscard]] FrontendMenuActivation close_frontend_menu(
    FrontendMenuScreen screen
);

}  // namespace aoe

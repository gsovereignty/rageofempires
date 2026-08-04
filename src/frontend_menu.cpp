#include "aoe/frontend_menu.hpp"

#include <algorithm>

namespace aoe {
namespace {

constexpr std::array main_items{
    FrontendMenuItem{
        "Learn to Play", {42, 104, 250, 42},
        FrontendMenuCommand::learn_to_play, false,
        "Learn the basics of commanding an empire.",
    },
    FrontendMenuItem{
        "Single Player", {42, 154, 250, 42},
        FrontendMenuCommand::open_single_player, true,
        "Play a single-player game.",
    },
    FrontendMenuItem{
        "Arabia vs AI", {42, 204, 250, 42},
        FrontendMenuCommand::open_ai_arabia, true,
        "Play standard Arabia against the built-in AI.",
    },
    FrontendMenuItem{
        "History", {42, 254, 250, 42},
        FrontendMenuCommand::open_history, true,
        "Explore the history behind Age of Empires II.",
    },
    FrontendMenuItem{
        "Multiplayer", {42, 304, 250, 42},
        FrontendMenuCommand::open_multiplayer, true,
        "Create or join a supported multiplayer game.",
    },
    FrontendMenuItem{
        "Map Editor", {42, 354, 250, 42},
        FrontendMenuCommand::open_map_editor, true,
        "Create and edit a scenario.",
    },
    FrontendMenuItem{
        "Options", {42, 404, 250, 42},
        FrontendMenuCommand::open_options, true,
        "Change game settings.",
    },
    FrontendMenuItem{
        "Zone", {42, 454, 250, 42},
        FrontendMenuCommand::show_zone_unavailable, true,
        "MSN Gaming Zone service is no longer available.",
    },
    FrontendMenuItem{
        "Exit", {42, 504, 250, 42},
        FrontendMenuCommand::exit_game, true,
        "Exit Age of Empires II.",
    },
};

// AoK-HD-patched FUN_006042a0 creates these image controls. Bounds and
// normal/focus frame pairs are direct executable constants except first
// control, whose omitted constructor bounds are recovered by registering
// frame 10 against background frame 0. Four-frame group inventory is archive
// evidence; meanings of frames 2 and 3 remain intentionally unassigned.
constexpr std::array native_main_controls{
    NativeFrontendControl{
        FrontendMenuCommand::open_single_player,
        {532, 9, 192, 258}, 10, 9500, 31000,
    },
    NativeFrontendControl{
        FrontendMenuCommand::open_multiplayer,
        {495, 265, 161, 188}, 14, 9501, 31001,
    },
    NativeFrontendControl{
        FrontendMenuCommand::learn_to_play,
        {135, 2, 218, 254}, 22, 9503, 31003,
    },
    NativeFrontendControl{
        FrontendMenuCommand::open_map_editor,
        {410, 344, 123, 97}, 26, 9504, 31004,
    },
    NativeFrontendControl{
        FrontendMenuCommand::open_history,
        {290, 198, 160, 147}, 30, 9505, 31005,
    },
    NativeFrontendControl{
        FrontendMenuCommand::open_options,
        {295, 439, 137, 139}, 34, 9506, 31006,
    },
    NativeFrontendControl{
        FrontendMenuCommand::exit_game,
        {174, 631, 230, 137}, 46, 9509, 31009,
    },
};

constexpr std::array ai_arabia_size_items{
    FrontendMenuItem{
        "Tiny (2 players)", {476, 153, 260, 39},
        FrontendMenuCommand::launch_ai_arabia_tiny, true,
        "120x120: standard size for a 1v1 match.",
    },
    FrontendMenuItem{
        "Small (4 players)", {476, 204, 260, 39},
        FrontendMenuCommand::launch_ai_arabia_small, true,
        "144x144: more room for a longer 1v1 match.",
    },
    FrontendMenuItem{
        "Medium (6 players)", {476, 254, 260, 39},
        FrontendMenuCommand::launch_ai_arabia_medium, true,
        "168x168: broad two-player Arabia battlefield.",
    },
    FrontendMenuItem{
        "Large (8 players)", {476, 319, 260, 39},
        FrontendMenuCommand::launch_ai_arabia_large, true,
        "220x220: largest common multiplayer preset.",
    },
};

constexpr std::array single_player_items{
    FrontendMenuItem{
        "Campaigns", {476, 89, 260, 39},
        FrontendMenuCommand::open_campaigns, true,
        "Battle with Joan of Arc, Genghis Khan, King Saladin, or "
        "Frederick Barbarossa in a series of related games.",
    },
    FrontendMenuItem{
        "Random Map", {476, 153, 260, 39},
        FrontendMenuCommand::open_random_map, true,
        "Start a standard single-player random-map game.",
    },
    FrontendMenuItem{
        "Regicide", {476, 204, 260, 39},
        FrontendMenuCommand::open_regicide, false,
        "Protect your king and defeat every enemy king.",
    },
    FrontendMenuItem{
        "Death Match", {476, 254, 260, 39},
        FrontendMenuCommand::open_death_match, false,
        "Begin with high resources for a fast-start battle.",
    },
    FrontendMenuItem{
        "Custom Campaign", {476, 319, 260, 39},
        FrontendMenuCommand::open_custom_campaign, true,
        "Select a supported installed custom campaign.",
    },
    FrontendMenuItem{
        "Custom Scenario", {476, 369, 260, 39},
        FrontendMenuCommand::open_custom_scenario, true,
        "Select a supported standalone custom scenario.",
    },
    FrontendMenuItem{
        "Saved Game", {476, 434, 260, 39},
        FrontendMenuCommand::open_saved_game, true,
        "Resume a saved single-player game.",
    },
};

}  // namespace

bool FrontendMenuRect::contains(float point_x, float point_y) const {
    return point_x >= x && point_y >= y &&
        point_x < x + width && point_y < y + height;
}

bool FrontendHitMask::contains(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height ||
        opaque.size() != static_cast<std::size_t>(width * height)) {
        return false;
    }
    return opaque[static_cast<std::size_t>(y * width + x)] != 0;
}

std::optional<std::array<float, 2>>
FrontendLogicalTransform::window_to_logical(
    float window_x,
    float window_y
) const {
    if (scale <= 0.0F) return std::nullopt;
    const float logical_x = (window_x - offset_x) / scale;
    const float logical_y = (window_y - offset_y) / scale;
    if (logical_x < 0.0F || logical_y < 0.0F ||
        logical_x >= static_cast<float>(logical_width) ||
        logical_y >= static_cast<float>(logical_height)) {
        return std::nullopt;
    }
    return std::array{logical_x, logical_y};
}

FrontendLogicalTransform frontend_logical_transform(
    int window_width,
    int window_height
) {
    if (window_width <= 0 || window_height <= 0) {
        return {0.0F, 0.0F, 0.0F};
    }
    // Cover the window. Cropping a little background is preferable to
    // exposing black gutters around an otherwise full-screen menu.
    const float scale = std::max(
        static_cast<float>(window_width) /
            static_cast<float>(frontend_logical_width),
        static_cast<float>(window_height) /
            static_cast<float>(frontend_logical_height)
    );
    return {
        scale,
        (static_cast<float>(window_width) -
            static_cast<float>(frontend_logical_width) * scale) * 0.5F,
        (static_cast<float>(window_height) -
            static_cast<float>(frontend_logical_height) * scale) * 0.5F,
    };
}

FrontendLogicalTransform native_frontend_logical_transform(
    int window_width,
    int window_height
) {
    if (window_width <= 0 || window_height <= 0) {
        return {0.0F, 0.0F, 0.0F, native_frontend_logical_width,
                native_frontend_logical_height};
    }
    const float scale = std::min(
        static_cast<float>(window_width) /
            static_cast<float>(native_frontend_logical_width),
        static_cast<float>(window_height) /
            static_cast<float>(native_frontend_logical_height)
    );
    return {
        scale,
        (static_cast<float>(window_width) -
            static_cast<float>(native_frontend_logical_width) * scale) * 0.5F,
        (static_cast<float>(window_height) -
            static_cast<float>(native_frontend_logical_height) * scale) * 0.5F,
        native_frontend_logical_width,
        native_frontend_logical_height,
    };
}

std::span<const FrontendMenuItem> main_menu_items() {
    return main_items;
}

std::span<const NativeFrontendControl> native_main_menu_controls() {
    return native_main_controls;
}

std::optional<std::size_t> native_frontend_hit_test(
    std::span<const NativeFrontendControl> controls,
    std::span<const FrontendHitMask> masks,
    float logical_x,
    float logical_y
) {
    if (controls.size() != masks.size()) return std::nullopt;
    for (std::size_t index = controls.size(); index-- > 0;) {
        const auto& control = controls[index];
        if (!control.bounds.contains(logical_x, logical_y)) continue;
        const int x = static_cast<int>(logical_x - control.bounds.x);
        const int y = static_cast<int>(logical_y - control.bounds.y);
        if (masks[index].contains(x, y)) return index;
    }
    return std::nullopt;
}

std::span<const FrontendMenuItem> single_player_menu_items() {
    return single_player_items;
}

std::span<const FrontendMenuItem> ai_arabia_size_menu_items() {
    return ai_arabia_size_items;
}

std::span<const FrontendMenuItem> frontend_menu_items(
    FrontendMenuScreen screen
) {
    if (screen == FrontendMenuScreen::main_menu) {
        return main_items;
    }
    if (screen == FrontendMenuScreen::single_player_menu) {
        return single_player_items;
    }
    if (screen == FrontendMenuScreen::ai_arabia_size_menu) {
        return ai_arabia_size_items;
    }
    return {};
}

std::optional<std::size_t> frontend_menu_hit_test(
    FrontendMenuScreen screen,
    float logical_x,
    float logical_y
) {
    const auto items = frontend_menu_items(screen);
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].bounds.contains(logical_x, logical_y)) {
            return index;
        }
    }
    return std::nullopt;
}

std::size_t move_frontend_menu_focus(
    FrontendMenuScreen screen,
    std::size_t current,
    int delta
) {
    const auto items = frontend_menu_items(screen);
    if (items.empty()) return 0;
    const int count = static_cast<int>(items.size());
    int index = static_cast<int>(current % items.size());
    const int direction = delta < 0 ? -1 : 1;
    index = (index + direction + count) % count;
    return static_cast<std::size_t>(index);
}

FrontendMenuActivation activate_frontend_menu_item(
    FrontendMenuScreen screen,
    std::size_t index
) {
    const auto items = frontend_menu_items(screen);
    if (index >= items.size() || !items[index].enabled) {
        return {screen, FrontendMenuCommand::none, false};
    }
    const FrontendMenuCommand command = items[index].command;
    if (command == FrontendMenuCommand::open_single_player) {
        return {
            FrontendMenuScreen::single_player_menu,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_ai_arabia) {
        return {
            FrontendMenuScreen::ai_arabia_size_menu,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_random_map ||
        command == FrontendMenuCommand::open_regicide ||
        command == FrontendMenuCommand::open_death_match) {
        return {
            FrontendMenuScreen::random_map_setup,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_campaigns) {
        return {
            FrontendMenuScreen::campaign_browser,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_custom_campaign) {
        return {
            FrontendMenuScreen::custom_campaign_browser,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_custom_scenario) {
        return {
            FrontendMenuScreen::custom_scenario_browser,
            command,
            true,
        };
    }
    if (command == FrontendMenuCommand::open_saved_game) {
        return {
            FrontendMenuScreen::saved_game_browser,
            command,
            true,
        };
    }
    return {screen, command, true};
}

FrontendMenuActivation close_frontend_menu(
    FrontendMenuScreen screen
) {
    if (screen == FrontendMenuScreen::single_player_menu ||
        screen == FrontendMenuScreen::ai_arabia_size_menu) {
        return {
            FrontendMenuScreen::main_menu,
            FrontendMenuCommand::close_flyout,
            true,
        };
    }
    return {screen, FrontendMenuCommand::none, false};
}

}  // namespace aoe

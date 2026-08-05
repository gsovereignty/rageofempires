#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace aoe {

enum class SinglePlayerSpeed { slow, normal, fast };
enum class MinimapMode { normal, combat, economic };

struct ReconstructionSettings {
    static constexpr int current_version = 5;

    SinglePlayerSpeed game_speed{SinglePlayerSpeed::normal};
    // Original sliders store attenuation: 0 is loudest, 99 is quietest.
    int music_volume{50};
    int effects_volume{0};
    bool fullscreen{};
    int screen_width{1280};
    int screen_height{720};
    int scroll_speed{100};
    int mouse_speed{100};
    bool edge_scroll{true};
    bool fog{true};
    bool minimap{true};
    MinimapMode minimap_mode{MinimapMode::normal};
    std::string locale{"en"};
    std::string language_file;
    // Editable, persistent bindings for represented global actions.
    std::array<int, 8> hotkeys{{27, 1073741886, 1073741887, 1073741888,
                                1073741889, 1073741890, 13, 9}};

    bool operator==(const ReconstructionSettings&) const = default;
};

enum class SettingsLoadStatus { missing, current, migrated, invalid };

struct SettingsLoadResult {
    ReconstructionSettings settings;
    SettingsLoadStatus status{SettingsLoadStatus::missing};
    std::string message;
};

bool validate_settings(
    const ReconstructionSettings& settings,
    std::string& error
);
SettingsLoadResult load_settings(const std::filesystem::path& path);
bool save_settings_atomic(
    const ReconstructionSettings& settings,
    const std::filesystem::path& path,
    std::string& error
);

}  // namespace aoe

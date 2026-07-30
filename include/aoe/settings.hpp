#pragma once

#include <filesystem>
#include <string>

namespace aoe {

enum class SinglePlayerSpeed { slow, normal, fast };

struct ReconstructionSettings {
    static constexpr int current_version = 2;

    SinglePlayerSpeed game_speed{SinglePlayerSpeed::normal};
    int music_volume{70};
    int effects_volume{70};
    int combat_volume{100};
    int interface_volume{100};
    int ambient_volume{100};
    bool fullscreen{};
    int scroll_speed{100};
    bool edge_scroll{true};
    bool fog{true};
    bool minimap{true};
    std::string locale{"en"};
    std::string language_file;

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

#include "aoe/settings.hpp"

#include <filesystem>
#include <fstream>
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
    const auto directory =
        std::filesystem::temp_directory_path() / "aoe-settings-tests";
    std::filesystem::create_directories(directory);
    const auto path = directory / "settings.txt";
    aoe::ReconstructionSettings settings;
    settings.game_speed = aoe::SinglePlayerSpeed::fast;
    settings.music_volume = 35;
    settings.effects_volume = 64;
    settings.fullscreen = true;
    settings.screen_width = 1920;
    settings.screen_height = 1080;
    settings.scroll_speed = 175;
    settings.mouse_speed = 125;
    settings.locale = "pt";
    settings.language_file = "strings/pt.lang";
    settings.minimap_mode = aoe::MinimapMode::economic;
    std::string error;
    expect(aoe::save_settings_atomic(settings, path, error), "atomic save");
    const auto loaded = aoe::load_settings(path);
    expect(
        loaded.status == aoe::SettingsLoadStatus::current,
        "current version detected"
    );
    expect(loaded.settings == settings, "round trip preserves settings");
    expect(!std::filesystem::exists(path.string() + ".tmp"), "temp removed");

    const auto legacy = directory / "legacy.txt";
    {
        std::ofstream output{legacy};
        output <<
            "aoe-reconstruction-settings 1\n"
            "game_speed=slow\nmusic_volume=40\neffects_volume=55\n"
            "fullscreen=false\nscroll_speed=100\nedge_scroll=true\n"
            "fog=true\nminimap=true\nlocale=en\nlanguage_file=\n";
    }
    const auto migrated = aoe::load_settings(legacy);
    expect(
        migrated.status == aoe::SettingsLoadStatus::migrated,
        "version one migrated"
    );
    expect(
        migrated.settings.music_volume == 60 &&
        migrated.settings.effects_volume == 45,
        "migration converts loudness percentages to attenuation"
    );
    expect(
        migrated.settings.minimap_mode == aoe::MinimapMode::normal,
        "old settings migrate to normal minimap mode"
    );

    {
        std::ofstream output{path};
        output << "aoe-reconstruction-settings 3\nscroll_speed=999\n";
    }
    expect(
        aoe::load_settings(path).status == aoe::SettingsLoadStatus::invalid,
        "invalid file rejected"
    );
    settings.hotkeys[1] = settings.hotkeys[0];
    expect(
        !aoe::save_settings_atomic(settings, path, error),
        "conflicting hotkeys rejected"
    );
    std::filesystem::remove_all(directory);
    if (failures == 0) std::cout << "settings tests passed\n";
    return failures == 0 ? 0 : 1;
}

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
    settings.scroll_speed = 175;
    settings.locale = "pt";
    settings.language_file = "strings/pt.lang";
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
        migrated.settings.combat_volume == 55 &&
        migrated.settings.interface_volume == 55 &&
        migrated.settings.ambient_volume == 55,
        "migration derives category volumes"
    );

    {
        std::ofstream output{path};
        output << "aoe-reconstruction-settings 2\nscroll_speed=999\n";
    }
    expect(
        aoe::load_settings(path).status == aoe::SettingsLoadStatus::invalid,
        "invalid file rejected"
    );
    std::filesystem::remove_all(directory);
    if (failures == 0) std::cout << "settings tests passed\n";
    return failures == 0 ? 0 : 1;
}

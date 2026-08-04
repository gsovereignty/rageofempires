#include "aoe/settings.hpp"

#include <charconv>
#include <fstream>
#include <map>
#include <system_error>

namespace aoe {
namespace {

bool parse_int(std::string_view text, int& value) {
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

bool parse_bool(std::string_view text, bool& value) {
    if (text == "true" || text == "1") {
        value = true;
        return true;
    }
    if (text == "false" || text == "0") {
        value = false;
        return true;
    }
    return false;
}

std::string speed_name(SinglePlayerSpeed speed) {
    switch (speed) {
        case SinglePlayerSpeed::slow: return "slow";
        case SinglePlayerSpeed::normal: return "normal";
        case SinglePlayerSpeed::fast: return "fast";
    }
    return "normal";
}

bool parse_speed(std::string_view text, SinglePlayerSpeed& speed) {
    if (text == "slow") speed = SinglePlayerSpeed::slow;
    else if (text == "normal") speed = SinglePlayerSpeed::normal;
    else if (text == "fast") speed = SinglePlayerSpeed::fast;
    else return false;
    return true;
}

}  // namespace

bool validate_settings(
    const ReconstructionSettings& settings,
    std::string& error
) {
    const auto valid_volume = [](int value) {
        return value >= 0 && value <= 99;
    };
    if (!valid_volume(settings.music_volume) ||
        !valid_volume(settings.effects_volume)) {
        error = "volume attenuation must be between 0 and 99";
        return false;
    }
    if (settings.scroll_speed < 25 || settings.scroll_speed > 400) {
        error = "scroll speed must be between 25 and 400";
        return false;
    }
    if (settings.locale.empty() || settings.locale.size() > 32) {
        error = "locale must contain 1 to 32 characters";
        return false;
    }
    if (settings.locale.find_first_of("\r\n=") != std::string::npos ||
        settings.language_file.find_first_of("\r\n") != std::string::npos) {
        error = "locale and language file must fit one settings line";
        return false;
    }
    return true;
}

SettingsLoadResult load_settings(const std::filesystem::path& path) {
    SettingsLoadResult result;
    std::ifstream input{path};
    if (!input) {
        if (std::filesystem::exists(path)) {
            result.status = SettingsLoadStatus::invalid;
            result.message = "settings file could not be opened";
        }
        return result;
    }
    std::string header;
    std::getline(input, header);
    int version{};
    constexpr std::string_view prefix{"aoe-reconstruction-settings "};
    if (!header.starts_with(prefix) ||
        !parse_int(std::string_view{header}.substr(prefix.size()), version) ||
        (version < 1 || version > ReconstructionSettings::current_version)) {
        result.status = SettingsLoadStatus::invalid;
        result.message = "unsupported settings header";
        return result;
    }
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            result.status = SettingsLoadStatus::invalid;
            result.message = "malformed settings line";
            return result;
        }
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    auto required = [&](std::string_view key) -> const std::string* {
        const auto found = values.find(std::string{key});
        return found == values.end() ? nullptr : &found->second;
    };
    const std::string* speed = required("game_speed");
    const std::string* music = required("music_volume");
    const std::string* effects = required("effects_volume");
    const std::string* fullscreen = required("fullscreen");
    const std::string* scroll = required("scroll_speed");
    const std::string* edge = required("edge_scroll");
    const std::string* fog = required("fog");
    const std::string* minimap = required("minimap");
    const std::string* locale = required("locale");
    if (!speed || !music || !effects || !fullscreen || !scroll || !edge ||
        !fog || !minimap || !locale ||
        !parse_speed(*speed, result.settings.game_speed) ||
        !parse_int(*music, result.settings.music_volume) ||
        !parse_int(*effects, result.settings.effects_volume) ||
        !parse_bool(*fullscreen, result.settings.fullscreen) ||
        !parse_int(*scroll, result.settings.scroll_speed) ||
        !parse_bool(*edge, result.settings.edge_scroll) ||
        !parse_bool(*fog, result.settings.fog) ||
        !parse_bool(*minimap, result.settings.minimap)) {
        result.status = SettingsLoadStatus::invalid;
        result.message = "missing or malformed setting";
        return result;
    }
    result.settings.locale = *locale;
    if (const std::string* file = required("language_file")) {
        result.settings.language_file = *file;
    }
    if (version < 3) {
        // Versions 1-2 stored linear loudness percentages. Preserve their
        // audible direction while migrating to original attenuation sliders.
        result.settings.music_volume =
            std::min(99, 100 - result.settings.music_volume);
        result.settings.effects_volume =
            std::min(99, 100 - result.settings.effects_volume);
    }
    std::string error;
    if (!validate_settings(result.settings, error)) {
        result.status = SettingsLoadStatus::invalid;
        result.message = error;
        return result;
    }
    result.status =
        version < ReconstructionSettings::current_version
            ? SettingsLoadStatus::migrated
            : SettingsLoadStatus::current;
    return result;
}

bool save_settings_atomic(
    const ReconstructionSettings& settings,
    const std::filesystem::path& path,
    std::string& error
) {
    if (!validate_settings(settings, error)) return false;
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "could not create settings directory";
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::trunc};
        if (!output) {
            error = "could not create temporary settings file";
            return false;
        }
        output << "aoe-reconstruction-settings 3\n"
               << "game_speed=" << speed_name(settings.game_speed) << '\n'
               << "music_volume=" << settings.music_volume << '\n'
               << "effects_volume=" << settings.effects_volume << '\n'
               << "fullscreen=" << settings.fullscreen << '\n'
               << "scroll_speed=" << settings.scroll_speed << '\n'
               << "edge_scroll=" << settings.edge_scroll << '\n'
               << "fog=" << settings.fog << '\n'
               << "minimap=" << settings.minimap << '\n'
               << "locale=" << settings.locale << '\n'
               << "language_file=" << settings.language_file << '\n';
        output.flush();
        if (!output) {
            error = "could not write temporary settings file";
            return false;
        }
    }
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary);
        error = "could not replace settings file atomically";
        return false;
    }
    return true;
}

}  // namespace aoe

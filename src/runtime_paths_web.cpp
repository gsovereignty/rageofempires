#include "aoe/runtime_paths.hpp"

#include <SDL3/SDL.h>

namespace aoe {

std::filesystem::path runtime_resources_directory() {
    return "/resources";
}

std::filesystem::path runtime_game_data_directory() {
    return "/game_data";
}

std::filesystem::path user_data_directory() {
    return "/user";
}

std::filesystem::path user_settings_directory() {
    return "/user/settings";
}

std::filesystem::path user_autosave_directory() {
    return "/user/autosave";
}

std::optional<std::filesystem::path> configured_asset_root() {
    if (const char* disabled = SDL_getenv("AOE_DISABLE_LEGACY_ASSETS");
        disabled != nullptr && disabled[0] != '\0' &&
        disabled[0] != '0') {
        return std::nullopt;
    }
    return runtime_game_data_directory();
}

}  // namespace aoe

#include "aoe/runtime_paths.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace aoe {
namespace {

std::filesystem::path executable_base_directory() {
    const char* raw_base = SDL_GetBasePath();
    if (raw_base == nullptr) {
        throw std::runtime_error(
            std::string{"cannot locate executable directory: "} +
            SDL_GetError()
        );
    }
    return raw_base;
}

}  // namespace

std::filesystem::path runtime_resources_directory() {
    const std::filesystem::path base = executable_base_directory();
    const std::filesystem::path bundled = base / "../Resources/resources";
    if (std::filesystem::is_directory(bundled)) return bundled;
    return base / "resources";
}

std::filesystem::path runtime_game_data_directory() {
    const std::filesystem::path base = executable_base_directory();
    const std::filesystem::path bundled = base / "../Resources/game_data";
    if (std::filesystem::is_directory(bundled)) return bundled;
    return base / "game_data";
}

std::filesystem::path user_data_directory() {
    char* raw_path = SDL_GetPrefPath(
        "Software Archaeology",
        "AoE Archaeology"
    );
    if (raw_path == nullptr) {
        throw std::runtime_error(
            std::string{"cannot locate user data directory: "} +
            SDL_GetError()
        );
    }

    const std::filesystem::path path{raw_path};
    SDL_free(raw_path);
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path user_settings_directory() {
    return user_data_directory();
}

std::filesystem::path user_autosave_directory() {
    return user_data_directory();
}

std::optional<std::filesystem::path> startup_autosave_path() {
    return std::nullopt;
}

void request_persistence_sync() {}

PersistenceSyncStatus persistence_sync_status() {
    return PersistenceSyncStatus::not_required;
}

std::optional<std::chrono::milliseconds> maximum_frame_elapsed() {
    return std::nullopt;
}

bool postgame_restart_reinitializes_application() {
    return false;
}

void request_application_restart() {}

bool consume_application_restart_request() {
    return false;
}

std::optional<std::filesystem::path> configured_asset_root() {
    if (const char* disabled = SDL_getenv("AOE_DISABLE_LEGACY_ASSETS");
        disabled != nullptr && disabled[0] != '\0' &&
        disabled[0] != '0') {
        return std::nullopt;
    }
    const std::filesystem::path base = executable_base_directory();
    const std::filesystem::path candidates[] = {
        base / "game_data",
        base / "../Resources/game_data",
    };
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::is_regular_file(
                candidate / "Data/graphics.drs"
            ) &&
            std::filesystem::is_regular_file(
                candidate / "Data/interfac.drs"
            )) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }
    return std::nullopt;
}

}  // namespace aoe

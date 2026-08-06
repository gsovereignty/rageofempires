#include "aoe/runtime_paths.hpp"

#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>

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

std::optional<std::filesystem::path> startup_autosave_path() {
    return user_autosave_directory() / "browser-autosave.txt";
}

EM_JS(void, request_browser_idb_sync, (), {
    if (Module.persistenceSyncStatus === 'pending') return;
    Module.persistenceSyncStatus = 'pending';
    FS.syncfs(false, function (error) {
      Module.persistenceSyncStatus = error ? 'failed' : 'succeeded';
      Module.persistenceSyncError = error ? String(error) : String();
    });
});

EM_JS(int, browser_idb_sync_status, (), {
    switch (Module.persistenceSyncStatus) {
      case 'pending': return 1;
      case 'succeeded': return 2;
      case 'failed': return 3;
      default: return 0;
    }
});

void request_persistence_sync() {
    request_browser_idb_sync();
}

PersistenceSyncStatus persistence_sync_status() {
    return static_cast<PersistenceSyncStatus>(browser_idb_sync_status());
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

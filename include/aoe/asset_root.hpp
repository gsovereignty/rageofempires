#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <optional>

namespace aoe {

inline std::optional<std::filesystem::path> configured_asset_root() {
    if (const char* disabled = SDL_getenv("AOE_DISABLE_LEGACY_ASSETS");
        disabled != nullptr && disabled[0] != '\0' &&
        disabled[0] != '0') {
        return std::nullopt;
    }
    const std::filesystem::path base = SDL_GetBasePath();
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

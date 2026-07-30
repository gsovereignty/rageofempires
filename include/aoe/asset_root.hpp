#pragma once

#include <cstdlib>
#include <filesystem>
#include <optional>

namespace aoe {

inline std::optional<std::filesystem::path> configured_asset_root() {
    if (const char* root = std::getenv("AOE_ASSET_ROOT");
        root != nullptr && *root != '\0') {
        return std::filesystem::path{root};
    }

#ifdef AOE_DEFAULT_ASSET_ROOT
    const std::filesystem::path root{AOE_DEFAULT_ASSET_ROOT};
    if (std::filesystem::is_directory(root / "Data")) {
        return root;
    }
#endif

    return std::nullopt;
}

}  // namespace aoe

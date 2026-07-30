#pragma once

#include <filesystem>
#include <optional>

namespace aoe {

inline std::optional<std::filesystem::path> configured_asset_root() {
    // Commercial archives are research input, never a product dependency.
    // Runtime rendering and audio use reconstruction-owned procedural/native
    // fallbacks when no packaged archive root exists.
    return std::nullopt;
}

}  // namespace aoe

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aoe/legacy_assets.hpp"

namespace aoe {

enum class UiAssetRole {
    hud_background,
    action_sheet,
    portrait_frame,
    default_cursor,
};

enum class UiMappingConfidence {
    archive_verified,
    reference_inferred,
};

enum class UiIconClassification {
    exact,
    missing,
    unknown,
};

struct UiIconBinding {
    std::string key;
    std::optional<std::int32_t> dat_icon_id;
    std::optional<std::int32_t> slp_resource_id;
    std::optional<std::size_t> frame_index;
    UiIconClassification classification{UiIconClassification::unknown};
};

struct UiIconSheet {
    std::int32_t resource_id{};
    std::size_t frame_count{};
};

struct UiAssetMapping {
    UiAssetRole role;
    std::int32_t resource_id{};
    std::size_t frame_index{};
    std::size_t expected_frame_count{};
    int expected_width{};
    int expected_height{};
    UiMappingConfidence confidence{UiMappingConfidence::reference_inferred};
};

struct UiAssetAuditEntry {
    UiAssetMapping mapping;
    bool present{};
    bool decoded{};
    bool metadata_matches{};
    std::string diagnostic;
};

struct UiAssetAudit {
    bool archive_available{};
    bool palette_available{};
    std::vector<UiAssetAuditEntry> entries;
};

[[nodiscard]] const std::vector<UiAssetMapping>& ui_asset_mappings();
[[nodiscard]] const UiAssetMapping& ui_asset_mapping(UiAssetRole role);
[[nodiscard]] UiAssetAudit audit_ui_assets(
    const std::filesystem::path& installation_root
);
[[nodiscard]] std::vector<UiIconSheet> inventory_ui_icon_sheets(
    const std::filesystem::path& installation_root
);
[[nodiscard]] RgbaFrame decode_ui_icon(
    const std::filesystem::path& installation_root,
    const UiIconBinding& binding
);

}  // namespace aoe

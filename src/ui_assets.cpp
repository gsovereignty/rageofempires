#include "aoe/ui_assets.hpp"

#include <exception>
#include <stdexcept>

#include "aoe/legacy_assets.hpp"

namespace aoe {

const std::vector<UiAssetMapping>& ui_asset_mappings() {
    static const std::vector<UiAssetMapping> mappings{
        {
            UiAssetRole::hud_background,
            51141,
            0,
            1,
            1280,
            1024,
            UiMappingConfidence::archive_verified,
        },
        {
            UiAssetRole::action_sheet,
            50721,
            0,
            69,
            36,
            36,
            UiMappingConfidence::archive_verified,
        },
        {
            UiAssetRole::portrait_frame,
            50713,
            0,
            4,
            54,
            54,
            UiMappingConfidence::reference_inferred,
        },
        {
            UiAssetRole::default_cursor,
            51000,
            0,
            19,
            24,
            32,
            UiMappingConfidence::archive_verified,
        },
    };
    return mappings;
}

const UiAssetMapping& ui_asset_mapping(UiAssetRole role) {
    for (const UiAssetMapping& mapping : ui_asset_mappings()) {
        if (mapping.role == role) return mapping;
    }
    throw std::invalid_argument("unknown UI asset role");
}

UiAssetAudit audit_ui_assets(
    const std::filesystem::path& installation_root
) {
    UiAssetAudit result;
    const std::filesystem::path archive_path =
        installation_root / "Data/interfac.drs";
    try {
        const DrsArchive archive{archive_path};
        result.archive_available = true;
        LegacyPalette palette;
        try {
            palette = LegacyPalette::from_jasc(
                archive.read("bina", 50500)
            );
            result.palette_available = true;
        } catch (const std::exception&) {
        }
        for (const UiAssetMapping& mapping : ui_asset_mappings()) {
            UiAssetAuditEntry entry;
            entry.mapping = mapping;
            entry.present =
                archive.contains("slp", mapping.resource_id);
            if (!entry.present) {
                entry.diagnostic = "SLP resource absent";
            } else if (!result.palette_available) {
                entry.diagnostic = "interface palette absent";
            } else {
                try {
                    const std::vector<std::byte> bytes =
                        archive.read("slp", mapping.resource_id);
                    const std::size_t count = slp_frame_count(bytes);
                    const RgbaFrame frame = decode_slp_frame(
                        bytes, palette, mapping.frame_index
                    );
                    entry.decoded = true;
                    entry.metadata_matches =
                        count == mapping.expected_frame_count &&
                        frame.width == mapping.expected_width &&
                        frame.height == mapping.expected_height;
                    entry.diagnostic = entry.metadata_matches
                        ? "metadata matches supplied archive"
                        : "decoded metadata differs";
                } catch (const std::exception& error) {
                    entry.diagnostic = error.what();
                }
            }
            result.entries.push_back(std::move(entry));
        }
    } catch (const std::exception& error) {
        for (const UiAssetMapping& mapping : ui_asset_mappings()) {
            result.entries.push_back({
                mapping,
                false,
                false,
                false,
                error.what(),
            });
        }
    }
    return result;
}

std::vector<UiIconSheet> inventory_ui_icon_sheets(
    const std::filesystem::path& installation_root
) {
    const DrsArchive archive{installation_root / "Data/interfac.drs"};
    std::vector<UiIconSheet> result;
    for (const std::int32_t id : archive.resource_ids("slp")) {
        try {
            result.push_back({
                id,
                slp_frame_count(archive.read("slp", id)),
            });
        } catch (const std::exception&) {
            // Inventory remains bounded to valid SLP resources.
        }
    }
    return result;
}

RgbaFrame decode_ui_icon(
    const std::filesystem::path& installation_root,
    const UiIconBinding& binding
) {
    if (binding.classification != UiIconClassification::exact ||
        !binding.slp_resource_id || !binding.frame_index) {
        throw std::invalid_argument(
            "UI icon binding lacks exact SLP/frame evidence"
        );
    }
    const DrsArchive archive{installation_root / "Data/interfac.drs"};
    const LegacyPalette palette = LegacyPalette::from_jasc(
        archive.read("bina", 50500)
    );
    const auto bytes = archive.read("slp", *binding.slp_resource_id);
    if (*binding.frame_index >= slp_frame_count(bytes)) {
        throw std::out_of_range("UI icon frame outside SLP");
    }
    return decode_slp_frame(bytes, palette, *binding.frame_index);
}

}  // namespace aoe

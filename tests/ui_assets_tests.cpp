#include "aoe/ui_assets.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>

int main() {
    int failures{};
    const auto check = [&](bool value, const char* message) {
        if (!value) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    const auto& mappings = aoe::ui_asset_mappings();
    check(mappings.size() == 4, "bounded mapping count");
    std::set<aoe::UiAssetRole> roles;
    for (const auto& mapping : mappings) {
        check(roles.insert(mapping.role).second, "unique role");
        check(mapping.resource_id > 0, "positive resource ID");
        check(
            mapping.frame_index < mapping.expected_frame_count,
            "frame index bounded"
        );
        check(
            mapping.expected_width > 0 &&
            mapping.expected_height > 0,
            "expected dimensions"
        );
    }

    const auto absent = aoe::audit_ui_assets(
        "/definitely/not/an/aoe/install"
    );
    check(!absent.archive_available, "missing archive fallback");
    check(
        absent.entries.size() == mappings.size(),
        "fallback reports every role"
    );
    try {
        (void)aoe::decode_ui_icon(
            "/definitely/not/an/aoe/install",
            {"unit.villager", 1, std::nullopt, std::nullopt,
             aoe::UiIconClassification::unknown}
        );
        check(false, "unknown icon relationship rejected");
    } catch (const std::invalid_argument&) {
    }

    if (const char* root = std::getenv("AOE_RESEARCH_ASSET_ROOT")) {
        const auto live = aoe::audit_ui_assets(root);
        check(live.archive_available, "live archive available");
        check(live.palette_available, "live palette available");
        for (const auto& entry : live.entries) {
            check(entry.present, "live mapping present");
            check(entry.decoded, "live mapping decoded");
            check(entry.metadata_matches, "live metadata matches");
        }
        const auto sheets = aoe::inventory_ui_icon_sheets(root);
        const auto has_sheet = [&sheets](std::int32_t id, std::size_t frames) {
            return std::ranges::any_of(
                sheets,
                [id, frames](const aoe::UiIconSheet& sheet) {
                    return sheet.resource_id == id &&
                           sheet.frame_count == frames;
                }
            );
        };
        check(has_sheet(50721, 69), "action sheet inventory");
        check(has_sheet(50729, 118), "118-frame sheet inventory");
        check(has_sheet(50730, 134), "134-frame sheet inventory");
    }
    return failures == 0 ? 0 : 1;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aoe/legacy_scenario.hpp"
#include "aoe/campaign.hpp"

namespace aoe {

enum class LegacyCampaignImportStatus {
    inspected,
    unsupported_version,
    malformed,
    io_error,
};

struct LegacyCampaignEntry {
    std::string name;
    std::string filename;
    std::uint64_t offset{};
    std::vector<std::byte> raw_payload;
    LegacyScenarioImportResult scenario;
};

struct LegacyCampaignRawRange {
    std::uint64_t offset{};
    std::vector<std::byte> bytes;
};

struct LegacyCampaignImportResult {
    LegacyCampaignImportStatus status{LegacyCampaignImportStatus::malformed};
    std::string version;
    std::string name;
    std::vector<LegacyCampaignEntry> entries;
    std::vector<LegacyCampaignRawRange> unindexed_payload;
    std::size_t decoded_scenarios{};
    std::size_t unsupported_scenarios{};
    std::string diagnostic;
    // Exact source image. Keeping this permits lossless round-trip/export,
    // including unknown version-specific header fields and index gaps.
    std::vector<std::byte> original_bytes;
};

LegacyCampaignImportResult inspect_legacy_campaign(
    const std::filesystem::path& path
);
LegacyCampaignImportResult inspect_legacy_campaign_bytes(
    std::span<const std::byte> bytes
);
std::vector<std::byte> serialize_legacy_campaign(
    const LegacyCampaignImportResult& campaign
);
void save_legacy_campaign(
    const LegacyCampaignImportResult& campaign,
    const std::filesystem::path& path
);
Campaign import_legacy_campaign(
    const std::filesystem::path& campaign_path,
    const std::filesystem::path& dat_path,
    const std::filesystem::path& install_root
);

}  // namespace aoe

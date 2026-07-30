#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aoe/types.hpp"

namespace aoe {

struct CampaignScenarioEntry {
    int id{};
    std::filesystem::path path;
    std::string name;
};

struct Campaign {
    std::string id;
    std::string name;
    std::string description;
    Player human_player{Player::blue};
    std::vector<CampaignScenarioEntry> scenarios;
    std::string manifest_digest;
};

struct CampaignProgress {
    std::string campaign_id;
    std::string manifest_digest;
    std::vector<int> completed;
    int highest_unlocked{};
};

enum class CampaignProgressStatus {
    current,
    missing,
    stale,
};

struct CampaignProgressLoad {
    CampaignProgressStatus status{CampaignProgressStatus::missing};
    CampaignProgress progress;
};

Campaign load_campaign(const std::filesystem::path& path);
void save_campaign(
    const Campaign& campaign,
    const std::filesystem::path& path
);
CampaignProgress fresh_campaign_progress(const Campaign& campaign);
CampaignProgressLoad load_campaign_progress(
    const Campaign& campaign,
    const std::filesystem::path& path
);
void save_campaign_progress_atomic(
    const Campaign& campaign,
    const CampaignProgress& progress,
    const std::filesystem::path& path
);
const CampaignScenarioEntry& current_campaign_scenario(
    const Campaign& campaign,
    const CampaignProgress& progress
);
std::optional<CampaignScenarioEntry> next_campaign_scenario(
    const Campaign& campaign,
    const CampaignProgress& progress
);
bool commit_campaign_outcome(
    const Campaign& campaign,
    int scenario_id,
    MatchOutcome outcome,
    CampaignProgress& progress,
    const std::filesystem::path& progress_path
);

}  // namespace aoe

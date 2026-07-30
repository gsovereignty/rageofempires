#include "aoe/campaign.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "aoe/scenario.hpp"

namespace aoe {
namespace {

bool valid_text(const std::string& value, bool allow_empty = false) {
    return (allow_empty || !value.empty()) && value.size() <= 4096 &&
        value.find_first_of("\r\n") == std::string::npos;
}

bool valid_id(const std::string& value) {
    if (value.empty() || value.size() > 128) return false;
    return std::ranges::all_of(value, [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '-' || character == '_';
    });
}

void require_end(std::istringstream& input, int line) {
    if (input.fail()) {
        throw std::runtime_error(
            "campaign line " + std::to_string(line) + ": malformed data"
        );
    }
    input >> std::ws;
    if (input.eof()) {
        input.clear();
        return;
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error(
            "campaign line " + std::to_string(line) + ": trailing data"
        );
    }
    input.clear();
}

std::string normalized_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "cannot read campaign scenario: " + path.string()
        );
    }
    std::string raw(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    std::string normalized;
    normalized.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        if (raw[index] == '\r' && index + 1 < raw.size() &&
            raw[index + 1] == '\n') {
            continue;
        }
        normalized.push_back(raw[index]);
    }
    return normalized;
}

std::string canonical_manifest(const Campaign& campaign) {
    std::ostringstream output;
    output << "aoe-campaign 1\n"
           << "id " << std::quoted(campaign.id) << '\n'
           << "name " << std::quoted(campaign.name) << '\n';
    if (!campaign.description.empty()) {
        output << "description " << std::quoted(campaign.description) << '\n';
    }
    output << "human-player "
           << (campaign.human_player == Player::blue ? "blue" : "red")
           << '\n';
    for (const auto& entry : campaign.scenarios) {
        output << "scenario " << entry.id << ' '
               << std::quoted(entry.path.generic_string()) << ' '
               << std::quoted(entry.name);
        if (!entry.briefing_audio.empty() ||
            !entry.debrief_audio.empty()) {
            output << ' ' << std::quoted(entry.briefing_audio)
                   << ' ' << std::quoted(entry.debrief_audio);
        }
        output << '\n';
    }
    return output.str();
}

bool valid_audio_filename(const std::string& filename) {
    if (filename.empty()) return true;
    const std::filesystem::path path{filename};
    std::string extension = path.extension().string();
    std::ranges::transform(
        extension, extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return path == path.filename() &&
        (extension == ".mp3" || extension == ".wav");
}

std::string digest_campaign(
    const Campaign& manifest_view,
    const Campaign& content
) {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto absorb = [&hash](const std::string& bytes) {
        for (const unsigned char byte : bytes) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
    };
    const auto absorb_field = [&absorb](const std::string& domain,
                                        const std::string& bytes) {
        absorb(domain);
        absorb(std::to_string(bytes.size()));
        absorb(":");
        absorb(bytes);
    };
    absorb_field("manifest:", canonical_manifest(manifest_view));
    for (std::size_t index = 0; index < content.scenarios.size(); ++index) {
        const auto& entry = content.scenarios[index];
        absorb_field("scenario-index:", std::to_string(index));
        absorb_field(
            "scenario-path:",
            manifest_view.scenarios[index].path.generic_string()
        );
        absorb_field("scenario-content:", normalized_bytes(entry.path));
    }
    std::ostringstream output;
    output << "fnv1a64-v1:" << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return output.str();
}

void validate_progress(
    const Campaign& campaign,
    const CampaignProgress& progress
) {
    if (progress.campaign_id != campaign.id ||
        progress.manifest_digest != campaign.manifest_digest) {
        throw std::invalid_argument("campaign progress identity mismatch");
    }
    if (progress.completed.size() > campaign.scenarios.size()) {
        throw std::invalid_argument("invalid campaign completion prefix");
    }
    for (std::size_t index = 0; index < progress.completed.size(); ++index) {
        if (progress.completed[index] != campaign.scenarios[index].id) {
            throw std::invalid_argument("noncontiguous campaign progress");
        }
    }
    const int expected = progress.completed.size() == campaign.scenarios.size()
        ? campaign.scenarios.back().id
        : campaign.scenarios[progress.completed.size()].id;
    if (progress.highest_unlocked != expected) {
        throw std::invalid_argument("invalid campaign unlock");
    }
}

bool is_human_victory(Player player, MatchOutcome outcome) {
    return outcome == MatchOutcome::allied_victory ||
        (player == Player::blue &&
         outcome == MatchOutcome::blue_victory) ||
        (player == Player::red &&
         outcome == MatchOutcome::red_victory);
}

}  // namespace

Campaign load_campaign(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open campaign manifest");
    Campaign campaign;
    const auto base = std::filesystem::weakly_canonical(
        path.parent_path().empty() ? std::filesystem::path{"."}
                                   : path.parent_path()
    );
    std::set<int> ids;
    std::set<std::string> names;
    std::set<std::filesystem::path> resolved_paths;
    bool header = false;
    bool saw_id = false;
    bool saw_name = false;
    bool saw_description = false;
    bool saw_player = false;
    bool saw_scenario = false;
    int prior_id = 0;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::istringstream record(line);
        std::string keyword;
        record >> keyword;
        if (!header) {
            int version{};
            record >> version;
            require_end(record, line_number);
            if (keyword != "aoe-campaign" || version != 1) {
                throw std::runtime_error("unsupported campaign header");
            }
            header = true;
        } else if (keyword == "id" && !saw_id && !saw_name) {
            record >> std::quoted(campaign.id);
            require_end(record, line_number);
            if (!record || !valid_id(campaign.id)) {
                throw std::runtime_error("invalid campaign id");
            }
            saw_id = true;
        } else if (keyword == "name" && saw_id && !saw_name) {
            record >> std::quoted(campaign.name);
            require_end(record, line_number);
            if (!record || !valid_text(campaign.name)) {
                throw std::runtime_error("invalid campaign name");
            }
            saw_name = true;
        } else if (keyword == "description" && saw_name &&
                   !saw_player && !saw_description) {
            record >> std::quoted(campaign.description);
            require_end(record, line_number);
            if (!record || !valid_text(campaign.description, true)) {
                throw std::runtime_error("invalid campaign description");
            }
            saw_description = true;
        } else if (keyword == "human-player" && saw_name && !saw_player) {
            std::string player;
            record >> player;
            require_end(record, line_number);
            if (player != "blue" && player != "red") {
                throw std::runtime_error("invalid campaign human player");
            }
            campaign.human_player =
                player == "blue" ? Player::blue : Player::red;
            saw_player = true;
        } else if (keyword == "scenario" && saw_player) {
            CampaignScenarioEntry entry;
            std::string relative;
            record >> entry.id >> std::quoted(relative) >>
                std::quoted(entry.name);
            const bool base_fields_valid = static_cast<bool>(record);
            if (base_fields_valid) {
                record >> std::ws;
                if (!record.eof()) {
                    record >> std::quoted(entry.briefing_audio) >>
                        std::quoted(entry.debrief_audio);
                } else {
                    record.clear();
                }
            }
            require_end(record, line_number);
            const std::filesystem::path relative_path(relative);
            bool bad_segment = relative_path.empty() ||
                relative_path.is_absolute() ||
                relative_path.extension() != ".scenario";
            for (const auto& segment : relative_path) {
                if (segment.empty() || segment == "." || segment == "..") {
                    bad_segment = true;
                }
            }
            if (!base_fields_valid || !record ||
                entry.id <= prior_id || bad_segment ||
                !valid_text(entry.name) || !ids.insert(entry.id).second ||
                !names.insert(entry.name).second ||
                !valid_audio_filename(entry.briefing_audio) ||
                !valid_audio_filename(entry.debrief_audio)) {
                throw std::runtime_error("invalid campaign scenario entry");
            }
            const auto resolved =
                std::filesystem::weakly_canonical(base / relative_path);
            const auto relative_to_base = resolved.lexically_relative(base);
            if (relative_to_base.empty() ||
                *relative_to_base.begin() == ".." ||
                !resolved_paths.insert(resolved).second ||
                !std::filesystem::is_regular_file(resolved)) {
                throw std::runtime_error("unsafe campaign scenario path");
            }
            (void)load_scenario(resolved);
            entry.path = resolved;
            campaign.scenarios.push_back(std::move(entry));
            prior_id = campaign.scenarios.back().id;
            saw_scenario = true;
        } else {
            throw std::runtime_error(
                "campaign line " + std::to_string(line_number) +
                ": unexpected record"
            );
        }
    }
    if (!header || !saw_id || !saw_name || !saw_player || !saw_scenario) {
        throw std::runtime_error("incomplete campaign manifest");
    }
    Campaign digest_input = campaign;
    for (auto& entry : digest_input.scenarios) {
        entry.path = entry.path.lexically_relative(base);
    }
    campaign.manifest_digest = digest_campaign(digest_input, campaign);
    return campaign;
}

void save_campaign(
    const Campaign& campaign,
    const std::filesystem::path& path
) {
    if (!valid_id(campaign.id) || !valid_text(campaign.name) ||
        !valid_text(campaign.description, true) ||
        (campaign.human_player != Player::blue &&
         campaign.human_player != Player::red) ||
        campaign.scenarios.empty()) {
        throw std::invalid_argument("invalid campaign");
    }
    Campaign relative = campaign;
    const auto base = std::filesystem::weakly_canonical(
        path.parent_path().empty() ? std::filesystem::path{"."}
                                   : path.parent_path()
    );
    std::set<int> ids;
    std::set<std::string> names;
    std::set<std::filesystem::path> paths;
    int prior_id = 0;
    for (auto& entry : relative.scenarios) {
        const auto resolved = std::filesystem::weakly_canonical(
            entry.path.is_absolute() ? entry.path : base / entry.path
        );
        const auto relative_path = resolved.lexically_relative(base);
        if (entry.id <= prior_id || !ids.insert(entry.id).second ||
            !valid_text(entry.name) || !names.insert(entry.name).second ||
            !valid_audio_filename(entry.briefing_audio) ||
            !valid_audio_filename(entry.debrief_audio) ||
            relative_path.empty() || *relative_path.begin() == ".." ||
            relative_path.extension() != ".scenario" ||
            !paths.insert(resolved).second ||
            !std::filesystem::is_regular_file(resolved)) {
            throw std::invalid_argument("invalid campaign scenario entry");
        }
        (void)load_scenario(resolved);
        entry.path = relative_path;
        prior_id = entry.id;
    }
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write campaign manifest");
    output << canonical_manifest(relative);
    if (!output) throw std::runtime_error("cannot write campaign manifest");
}

CampaignProgress fresh_campaign_progress(const Campaign& campaign) {
    return {
        campaign.id,
        campaign.manifest_digest,
        {},
        campaign.scenarios.front().id,
    };
}

CampaignProgressLoad load_campaign_progress(
    const Campaign& campaign,
    const std::filesystem::path& path
) {
    if (!std::filesystem::exists(path)) {
        return {
            CampaignProgressStatus::missing,
            fresh_campaign_progress(campaign),
        };
    }
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    if (!input || magic != "aoe-campaign-progress" || version != 1) {
        throw std::runtime_error("invalid campaign progress header");
    }
    CampaignProgress progress;
    bool saw_id = false;
    bool saw_digest = false;
    bool saw_unlocked = false;
    std::string record;
    while (input >> record) {
        if (record == "campaign-id") {
            if (saw_id) {
                throw std::runtime_error("duplicate campaign progress id");
            }
            input >> std::quoted(progress.campaign_id);
            saw_id = true;
        } else if (record == "manifest-digest") {
            if (saw_digest) {
                throw std::runtime_error(
                    "duplicate campaign progress digest"
                );
            }
            input >> std::quoted(progress.manifest_digest);
            saw_digest = true;
        } else if (record == "completed") {
            int id{};
            input >> id;
            progress.completed.push_back(id);
        } else if (record == "unlocked") {
            if (saw_unlocked) {
                throw std::runtime_error(
                    "duplicate campaign progress unlock"
                );
            }
            input >> progress.highest_unlocked;
            saw_unlocked = true;
        } else {
            throw std::runtime_error("unknown campaign progress record");
        }
        if (!input) throw std::runtime_error("malformed campaign progress");
    }
    if (!saw_id || !saw_digest || !saw_unlocked) {
        throw std::runtime_error("incomplete campaign progress");
    }
    if (progress.campaign_id != campaign.id ||
        progress.manifest_digest != campaign.manifest_digest) {
        return {
            CampaignProgressStatus::stale,
            fresh_campaign_progress(campaign),
        };
    }
    validate_progress(campaign, progress);
    return {CampaignProgressStatus::current, std::move(progress)};
}

void save_campaign_progress_atomic(
    const Campaign& campaign,
    const CampaignProgress& progress,
    const std::filesystem::path& path
) {
    validate_progress(campaign, progress);
    const auto parent = path.parent_path().empty()
        ? std::filesystem::path{"."}
        : path.parent_path();
    std::filesystem::create_directories(parent);
    static std::atomic<std::uint64_t> sequence{};
    const auto temporary = parent /
        (path.filename().string() + ".tmp." +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()
         ) + "." + std::to_string(sequence.fetch_add(1)));
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) throw std::runtime_error("cannot write campaign progress");
        output << "aoe-campaign-progress 1\n"
               << "campaign-id " << std::quoted(progress.campaign_id) << '\n'
               << "manifest-digest "
               << std::quoted(progress.manifest_digest) << '\n';
        for (const int id : progress.completed) {
            output << "completed " << id << '\n';
        }
        output << "unlocked " << progress.highest_unlocked << '\n';
        output.flush();
        if (!output) {
            throw std::runtime_error("cannot flush campaign progress");
        }
    }
#if !defined(_WIN32)
    const int temporary_fd = ::open(temporary.c_str(), O_RDONLY);
    if (temporary_fd < 0 || ::fsync(temporary_fd) != 0) {
        if (temporary_fd >= 0) ::close(temporary_fd);
        std::filesystem::remove(temporary);
        throw std::runtime_error("cannot sync campaign progress");
    }
    ::close(temporary_fd);
#endif
#if defined(_WIN32)
    const bool replaced = MoveFileExW(
        temporary.c_str(),
        path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    ) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    const bool replaced = !error;
#endif
    if (!replaced) {
        std::filesystem::remove(temporary);
        throw std::runtime_error(
            "cannot atomically replace campaign progress"
        );
    }
#if !defined(_WIN32)
    const int directory_fd = ::open(parent.c_str(), O_RDONLY);
    if (directory_fd < 0 || ::fsync(directory_fd) != 0) {
        if (directory_fd >= 0) ::close(directory_fd);
        throw std::runtime_error(
            "campaign progress committed but directory sync failed"
        );
    }
    ::close(directory_fd);
#endif
}

const CampaignScenarioEntry& current_campaign_scenario(
    const Campaign& campaign,
    const CampaignProgress& progress
) {
    validate_progress(campaign, progress);
    const auto found = std::ranges::find(
        campaign.scenarios, progress.highest_unlocked,
        &CampaignScenarioEntry::id
    );
    if (found == campaign.scenarios.end()) {
        throw std::invalid_argument("campaign has no current scenario");
    }
    return *found;
}

std::optional<CampaignScenarioEntry> next_campaign_scenario(
    const Campaign& campaign,
    const CampaignProgress& progress
) {
    validate_progress(campaign, progress);
    if (progress.completed.size() >= campaign.scenarios.size()) {
        return std::nullopt;
    }
    return campaign.scenarios[progress.completed.size()];
}

bool commit_campaign_outcome(
    const Campaign& campaign,
    int scenario_id,
    MatchOutcome outcome,
    CampaignProgress& progress,
    const std::filesystem::path& progress_path
) {
    validate_progress(campaign, progress);
    if (!is_human_victory(campaign.human_player, outcome)) return false;
    if (scenario_id != progress.highest_unlocked ||
        progress.completed.size() >= campaign.scenarios.size() ||
        campaign.scenarios[progress.completed.size()].id != scenario_id) {
        return false;
    }
    CampaignProgress updated = progress;
    updated.completed.push_back(scenario_id);
    updated.highest_unlocked =
        updated.completed.size() == campaign.scenarios.size()
        ? scenario_id
        : campaign.scenarios[updated.completed.size()].id;
    save_campaign_progress_atomic(campaign, updated, progress_path);
    progress = std::move(updated);
    return true;
}

}  // namespace aoe

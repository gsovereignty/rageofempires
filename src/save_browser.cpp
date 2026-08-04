#include "aoe/save_browser.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <ctime>

#include "aoe/format_versions.hpp"
#include "aoe/legacy_campaign.hpp"
#include "aoe/save_game.hpp"

namespace aoe {
namespace {

std::string formatted_time(const std::filesystem::path& path) {
    std::error_code error;
    const auto stamp = std::filesystem::last_write_time(path, error);
    if (error) return "TIME UNAVAILABLE";
    const auto system_stamp =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            stamp - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now()
        );
    const std::time_t value =
        std::chrono::system_clock::to_time_t(system_stamp);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M");
    return output.str();
}

std::optional<int> header_version(
    const std::filesystem::path& path,
    std::string_view expected
) {
    std::ifstream input{path};
    std::string marker;
    int version{};
    if (!(input >> marker >> version) || marker != expected) return {};
    return version;
}

BrowserEntry inspect(const std::filesystem::path& path) {
    BrowserEntry entry;
    entry.filename = path.filename().string();
    entry.modified_time = formatted_time(path);
    const std::string extension = path.extension().string();
    if (extension == ".cpn" || extension == ".cpx" ||
        extension == ".cpx2") {
        entry.kind = BrowserFileKind::campaign;
        const LegacyCampaignImportResult campaign = inspect_legacy_campaign(path);
        if (campaign.status == LegacyCampaignImportStatus::inspected) {
            entry.status = BrowserFileStatus::compatible;
            entry.diagnostic = campaign.name + " (" +
                std::to_string(campaign.entries.size()) + " SCENARIOS)";
        } else if (campaign.status ==
                   LegacyCampaignImportStatus::unsupported_version) {
            entry.status = BrowserFileStatus::incompatible;
            entry.diagnostic = campaign.diagnostic;
        } else {
            entry.status = BrowserFileStatus::corrupt;
            entry.diagnostic = std::string{"CORRUPT CAMPAIGN: "} +
                campaign.diagnostic;
        }
        return entry;
    }
    if (extension == ".mgz" || extension == ".mgl" ||
        extension == ".aoe2record" || extension == ".sav") {
        entry.kind = BrowserFileKind::legacy_commercial;
        entry.status = BrowserFileStatus::inspect_only;
        entry.diagnostic = "COMMERCIAL FORMAT: INSPECT-ONLY";
        return entry;
    }
    const auto save_version =
        header_version(path, "AOE-ARCHAEOLOGY-SAVE");
    const auto replay_version =
        header_version(path, "AOE-ARCHAEOLOGY-REPLAY");
    if (save_version) {
        entry.kind = BrowserFileKind::save;
        entry.version = *save_version;
        if (*save_version != reconstruction_save_version) {
            entry.status = BrowserFileStatus::incompatible;
            entry.diagnostic = "INCOMPATIBLE SAVE VERSION";
            return entry;
        }
        try {
            const Simulation simulation = load_game(path);
            entry.tick = simulation.tick_number();
            entry.outcome = simulation.outcome();
            entry.civilization = simulation.civilization(Player::blue);
            entry.status = BrowserFileStatus::compatible;
            entry.diagnostic = "READY TO LOAD";
        } catch (const std::exception& error) {
            entry.status = BrowserFileStatus::corrupt;
            entry.diagnostic = std::string{"CORRUPT SAVE: "} + error.what();
        }
        return entry;
    }
    if (replay_version) {
        entry.kind = BrowserFileKind::replay;
        entry.version = *replay_version;
        if (*replay_version != reconstruction_command_schema_version) {
            entry.status = BrowserFileStatus::incompatible;
            entry.diagnostic = "INCOMPATIBLE REPLAY VERSION";
            return entry;
        }
        try {
            const Replay replay = load_replay(path);
            entry.command_count = replay.commands().size();
            if (!replay.commands().empty()) {
                entry.tick = replay.commands().back().tick;
            }
            entry.status = BrowserFileStatus::compatible;
            entry.diagnostic = "READY TO PLAY";
        } catch (const std::exception& error) {
            entry.status = BrowserFileStatus::corrupt;
            entry.diagnostic = std::string{"CORRUPT REPLAY: "} + error.what();
        }
        return entry;
    }
    entry.kind = BrowserFileKind::unknown;
    entry.status = BrowserFileStatus::corrupt;
    entry.diagnostic = "UNRECOGNIZED PROJECT FILE";
    return entry;
}

template<class Writer>
bool write_atomic(
    const std::filesystem::path& path,
    bool allow_overwrite,
    Writer writer,
    std::string& error
) {
    if (std::filesystem::exists(path) && !allow_overwrite) {
        error = "overwrite confirmation required";
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    try {
        writer(temporary);
        std::error_code rename_error;
        std::filesystem::rename(temporary, path, rename_error);
        if (rename_error) {
            std::filesystem::remove(temporary);
            error = "atomic replacement failed";
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        std::filesystem::remove(temporary);
        error = exception.what();
        return false;
    }
}

}  // namespace

bool valid_save_slot_name(std::string_view name) {
    return !name.empty() && name.size() <= 32 &&
        std::ranges::all_of(name, [](unsigned char value) {
            return std::isalnum(value) || value == '-' || value == '_';
        });
}

std::vector<BrowserEntry> browse_user_data_files(
    const std::filesystem::path& user_data
) {
    std::vector<BrowserEntry> entries;
    std::error_code error;
    if (!std::filesystem::is_directory(user_data, error)) return entries;
    for (const auto& item : std::filesystem::directory_iterator(
             user_data, std::filesystem::directory_options::skip_permission_denied,
             error)) {
        if (error || item.is_symlink(error) || !item.is_regular_file(error)) {
            continue;
        }
        const std::string extension = item.path().extension().string();
        if (extension == ".save" || extension == ".replay" ||
            extension == ".cpn" || extension == ".cpx" ||
            extension == ".cpx2" ||
            extension == ".mgz" || extension == ".mgl" ||
            extension == ".aoe2record" || extension == ".sav") {
            entries.push_back(inspect(item.path()));
        }
    }
    std::ranges::sort(entries, {}, &BrowserEntry::filename);
    return entries;
}

bool save_slot_atomic(
    const Simulation& simulation,
    const std::filesystem::path& user_data,
    std::string_view slot,
    bool allow_overwrite,
    std::string& error
) {
    if (!valid_save_slot_name(slot)) {
        error = "slot name must use 1-32 letters, digits, '-' or '_'";
        return false;
    }
    std::filesystem::create_directories(user_data);
    const auto path = user_data / (std::string{slot} + ".save");
    return write_atomic(
        path, allow_overwrite,
        [&](const auto& temporary) { save_game(simulation, temporary); },
        error
    );
}

bool replay_slot_atomic(
    const Replay& replay,
    const std::filesystem::path& user_data,
    std::string_view slot,
    bool allow_overwrite,
    std::string& error
) {
    if (!valid_save_slot_name(slot)) {
        error = "slot name must use 1-32 letters, digits, '-' or '_'";
        return false;
    }
    std::filesystem::create_directories(user_data);
    const auto path = user_data / (std::string{slot} + ".replay");
    return write_atomic(
        path, allow_overwrite,
        [&](const auto& temporary) { save_replay(replay, temporary); },
        error
    );
}

std::filesystem::path bounded_browser_path(
    const std::filesystem::path& user_data,
    const BrowserEntry& entry
) {
    if (entry.filename.empty() ||
        std::filesystem::path{entry.filename}.filename() != entry.filename) {
        throw std::invalid_argument("unbounded browser filename");
    }
    return user_data / entry.filename;
}

}  // namespace aoe

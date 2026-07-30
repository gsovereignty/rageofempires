#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aoe/game_command.hpp"
#include "aoe/simulation.hpp"

namespace aoe {

enum class BrowserFileKind { save, replay, legacy_commercial, unknown };
enum class BrowserFileStatus { compatible, incompatible, corrupt, inspect_only };

struct BrowserEntry {
    std::string filename;
    BrowserFileKind kind{BrowserFileKind::unknown};
    BrowserFileStatus status{BrowserFileStatus::corrupt};
    std::optional<int> version;
    std::optional<std::uint64_t> tick;
    std::optional<MatchOutcome> outcome;
    std::optional<Civilization> civilization;
    std::optional<std::size_t> command_count;
    std::string modified_time;
    std::string diagnostic;
};

bool valid_save_slot_name(std::string_view name);
std::vector<BrowserEntry> browse_user_data_files(
    const std::filesystem::path& user_data
);
bool save_slot_atomic(
    const Simulation& simulation,
    const std::filesystem::path& user_data,
    std::string_view slot,
    bool allow_overwrite,
    std::string& error
);
bool replay_slot_atomic(
    const Replay& replay,
    const std::filesystem::path& user_data,
    std::string_view slot,
    bool allow_overwrite,
    std::string& error
);
std::filesystem::path bounded_browser_path(
    const std::filesystem::path& user_data,
    const BrowserEntry& entry
);

}  // namespace aoe

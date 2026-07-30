#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "aoe/types.hpp"

namespace aoe {

struct ResourceStatistics {
    std::uint64_t food{};
    std::uint64_t wood{};
    std::uint64_t gold{};
    std::uint64_t stone{};

    auto operator<=>(const ResourceStatistics&) const = default;
};

struct AgeTimingStatistics {
    std::optional<std::uint64_t> feudal;
    std::optional<std::uint64_t> castle;
    std::optional<std::uint64_t> imperial;

    auto operator<=>(const AgeTimingStatistics&) const = default;
};

struct PlayerStatistics {
    ResourceStatistics gathered;
    ResourceStatistics tribute_sent;
    ResourceStatistics tribute_received;
    std::uint64_t units_created{};
    std::uint64_t units_lost{};
    std::uint64_t units_killed{};
    std::uint64_t buildings_built{};
    std::uint64_t buildings_lost{};
    std::uint64_t buildings_razed{};
    std::uint64_t conversions{};
    std::uint64_t relics_collected{};
    std::uint64_t technologies_researched{};
    std::uint64_t wonders_built{};
    AgeTimingStatistics age_times;

    auto operator<=>(const PlayerStatistics&) const = default;
};

struct StatisticsTimelineSample {
    std::uint64_t tick{};
    std::array<int, 8> score{};
    std::array<int, 8> population{};
    std::array<ResourceStatistics, 8> gathered{};

    auto operator<=>(const StatisticsTimelineSample&) const = default;
};

struct MatchStatistics {
    std::array<PlayerStatistics, 8> players;
    std::vector<StatisticsTimelineSample> timeline;
    std::array<int, 8> current_score{};
    std::array<bool, 8> active_slots{};
    std::array<int, 8> team_numbers{};

    [[nodiscard]] const PlayerStatistics& for_player(Player player) const {
        return players[player == Player::red ? 1U : 0U];
    }

    auto operator<=>(const MatchStatistics&) const = default;
};

struct LegacyStatisticsTimelineSample {
    std::uint64_t tick{};
    std::array<int, 2> score{};
    std::array<int, 2> population{};
    std::array<ResourceStatistics, 2> gathered{};
};

struct LegacyMatchStatistics {
    std::array<PlayerStatistics, 2> players;
    std::vector<LegacyStatisticsTimelineSample> timeline;
    std::array<int, 2> current_score{};
};

}  // namespace aoe

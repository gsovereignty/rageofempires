#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aoe/match_statistics.hpp"
#include "aoe/types.hpp"

namespace aoe {

enum class StatisticsTab { economy, military, society, technology, timeline };

struct StatisticsRow {
    std::string label;
    std::optional<std::uint64_t> blue;
    std::optional<std::uint64_t> red;

    bool operator==(const StatisticsRow&) const = default;
};

struct StatisticsGraphPoint {
    float x{};
    float blue{};
    float red{};
};

std::vector<StatisticsRow> statistics_rows(
    const MatchStatistics& statistics,
    StatisticsTab tab
);
std::vector<StatisticsGraphPoint> score_graph_points(
    const MatchStatistics& statistics
);
std::string statistics_victory_cause(
    MatchOutcome outcome,
    VictoryCountdownKind blue_kind,
    VictoryCountdownKind red_kind
);

}  // namespace aoe

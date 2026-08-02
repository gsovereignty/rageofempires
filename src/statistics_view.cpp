#include "aoe/statistics_view.hpp"

#include <algorithm>

namespace aoe {
namespace {

std::uint64_t resource_total(const ResourceStatistics& value) {
    return value.food + value.wood + value.gold + value.stone;
}

StatisticsRow row(
    std::string label, std::uint64_t blue, std::uint64_t red
) {
    return {std::move(label), blue, red};
}

StatisticsRow unavailable(std::string label) {
    return {std::move(label), std::nullopt, std::nullopt};
}

}  // namespace

std::vector<StatisticsRow> statistics_rows(
    const MatchStatistics& statistics,
    StatisticsTab tab
) {
    const PlayerStatistics& blue = statistics.players[0];
    const PlayerStatistics& red = statistics.players[1];
    if (tab == StatisticsTab::economy) {
        return {
            row("FOOD GATHERED", blue.gathered.food, red.gathered.food),
            row("WOOD GATHERED", blue.gathered.wood, red.gathered.wood),
            row("GOLD GATHERED", blue.gathered.gold, red.gathered.gold),
            row("STONE GATHERED", blue.gathered.stone, red.gathered.stone),
            row("TOTAL GATHERED", resource_total(blue.gathered),
                resource_total(red.gathered)),
            row("TRIBUTE SENT", resource_total(blue.tribute_sent),
                resource_total(red.tribute_sent)),
            row("TRIBUTE RECEIVED", resource_total(blue.tribute_received),
                resource_total(red.tribute_received)),
        };
    }
    if (tab == StatisticsTab::military) {
        return {
            row("UNITS CREATED", blue.units_created, red.units_created),
            row("UNITS KILLED", blue.units_killed, red.units_killed),
            row("UNITS LOST", blue.units_lost, red.units_lost),
            row("BUILDINGS RAZED", blue.buildings_razed,
                red.buildings_razed),
            row("BUILDINGS LOST", blue.buildings_lost,
                red.buildings_lost),
            row("CONVERSIONS", blue.conversions, red.conversions),
            unavailable("LARGEST ARMY"),
        };
    }
    if (tab == StatisticsTab::society) {
        return {
            row("BUILDINGS BUILT", blue.buildings_built,
                red.buildings_built),
            row("WONDERS BUILT", blue.wonders_built, red.wonders_built),
            row("RELICS COLLECTED", blue.relics_collected,
                red.relics_collected),
            unavailable("MAP EXPLORED"),
            unavailable("VILLAGER HIGH"),
        };
    }
    if (tab == StatisticsTab::technology) {
        const auto timing = [](const std::optional<std::uint64_t>& value) {
            return value;
        };
        return {
            row("TECHNOLOGIES RESEARCHED", blue.technologies_researched,
                red.technologies_researched),
            {"FEUDAL AGE TICK", timing(blue.age_times.feudal),
                timing(red.age_times.feudal)},
            {"CASTLE AGE TICK", timing(blue.age_times.castle),
                timing(red.age_times.castle)},
            {"IMPERIAL AGE TICK", timing(blue.age_times.imperial),
                timing(red.age_times.imperial)},
            unavailable("TECHNOLOGY PERCENT"),
        };
    }
    return {
        row("CURRENT SCORE", statistics.current_score[0],
            statistics.current_score[1]),
        row("SAMPLES", statistics.timeline.size(),
            statistics.timeline.size()),
    };
}

std::vector<StatisticsGraphPoint> score_graph_points(
    const MatchStatistics& statistics
) {
    if (statistics.timeline.empty()) return {};
    const float last_tick = static_cast<float>(
        statistics.timeline.back().tick
    );
    int maximum = 1;
    for (const auto& sample : statistics.timeline) {
        maximum = std::max({maximum, sample.score[0], sample.score[1]});
    }
    std::vector<StatisticsGraphPoint> result;
    result.reserve(statistics.timeline.size());
    for (const auto& sample : statistics.timeline) {
        result.push_back({
            last_tick > 0.0F
                ? static_cast<float>(sample.tick) / last_tick : 0.0F,
            static_cast<float>(sample.score[0]) /
                static_cast<float>(maximum),
            static_cast<float>(sample.score[1]) /
                static_cast<float>(maximum),
        });
    }
    return result;
}

std::string statistics_victory_cause(
    MatchOutcome outcome,
    VictoryCountdownKind blue_kind,
    VictoryCountdownKind red_kind
) {
    const VictoryCountdownKind winner_kind =
        outcome == MatchOutcome::blue_victory ? blue_kind :
        outcome == MatchOutcome::red_victory ? red_kind :
        VictoryCountdownKind::none;
    if (winner_kind == VictoryCountdownKind::wonder) {
        return "WONDER COUNTDOWN";
    }
    if (winner_kind == VictoryCountdownKind::relic) {
        return "RELIC COUNTDOWN";
    }
    if (outcome == MatchOutcome::ongoing) return "MATCH IN PROGRESS";
    if (outcome == MatchOutcome::allied_victory) {
        return "ALLIED VICTORY";
    }
    if (outcome == MatchOutcome::blue_victory ||
        outcome == MatchOutcome::red_victory) {
        return "CONQUEST";
    }
    return "DRAW";
}

}  // namespace aoe

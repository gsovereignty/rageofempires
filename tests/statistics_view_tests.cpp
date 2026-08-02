#include "aoe/statistics_view.hpp"

#include <cmath>
#include <iostream>

namespace {
int failures{};
void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    aoe::MatchStatistics statistics;
    statistics.players[0].gathered = {10, 20, 30, 40};
    statistics.players[1].gathered = {1, 2, 3, 4};
    statistics.players[0].units_killed = 7;
    statistics.timeline = {
        {100, {10, 20}, {2, 3}, {}},
        {200, {40, 20}, {4, 3}, {}},
    };
    statistics.current_score = {40, 20};
    const auto economy =
        aoe::statistics_rows(statistics, aoe::StatisticsTab::economy);
    expect(
        economy[4].blue == 100 && economy[4].red == 10,
        "economy total wrong"
    );
    const auto military =
        aoe::statistics_rows(statistics, aoe::StatisticsTab::military);
    expect(military[1].blue == 7, "tracked military value absent");
    expect(
        !military.back().blue && !military.back().red,
        "untracked largest army invented"
    );
    const auto graph = aoe::score_graph_points(statistics);
    expect(
        graph.size() == 2 && std::abs(graph.back().x - 1.0F) < 0.001F &&
        std::abs(graph.back().blue - 1.0F) < 0.001F,
        "timeline normalization wrong"
    );
    expect(
        aoe::statistics_victory_cause(
            aoe::MatchOutcome::blue_victory,
            aoe::VictoryCountdownKind::wonder,
            aoe::VictoryCountdownKind::none
        ) == "WONDER COUNTDOWN",
        "tracked victory cause absent"
    );
    expect(
        aoe::statistics_victory_cause(
            aoe::MatchOutcome::red_victory,
            aoe::VictoryCountdownKind::none,
            aoe::VictoryCountdownKind::none
        ) == "CONQUEST",
        "standard victory cause absent"
    );
    if (failures == 0) std::cout << "statistics view tests passed\n";
    return failures == 0 ? 0 : 1;
}

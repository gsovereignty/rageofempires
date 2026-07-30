#pragma once

#include <filesystem>

#include "aoe/simulation.hpp"

namespace aoe {

enum class ComputerDifficulty {
    easiest,
    easy,
    moderate,
    hard,
    hardest,
    // Source-compatibility names used by reconstruction saves before the
    // commercial five-level contract was recovered.
    standard = moderate,
    expert = hardest,
};

// Commercial target search uses one effective LOS on the two lower
// difficulties and two effective LOS on the other three.
[[nodiscard]] int computer_target_acquisition_radius(
    ComputerDifficulty difficulty,
    int effective_line_of_sight
);

enum class ComputerStrategyPhase {
    opening,
    developing,
    pressure,
    conquest,
};

enum class ComputerObjective {
    scout,
    defend,
    attack,
    naval,
    transport,
    trade,
    relic,
    wonder,
    regroup,
};

struct ComputerPlayerStatus {
    ComputerStrategyPhase phase{ComputerStrategyPhase::opening};
    Age age_goal{Age::feudal};
    std::array<int, 4> resource_workers{};
    int villagers{};
    int melee_units{};
    int ranged_units{};
    int cavalry_units{};
    int siege_units{};
    int naval_units{};
    UnitKind desired_counter{UnitKind::militia};
    ComputerObjective objective{ComputerObjective::scout};
    std::optional<TilePosition> target;
    TilePosition home{-1, -1};
    TilePosition rally{-1, -1};
    bool retreating{};
};

struct ComputerPlayerState {
    Player player{Player::red};
    ComputerDifficulty difficulty{ComputerDifficulty::moderate};
    std::uint64_t last_command_tick{};
    std::uint64_t last_attack_tick{};
    std::uint64_t strategy_epoch{};
    EntityId last_target_id{};
    TilePosition home_anchor{-1, -1};
    TilePosition rally_point{-1, -1};
    bool retreating{};
};

// Small deterministic opponent. It sees only public simulation state and
// submits the same explicit commands available to other controllers.
class ComputerPlayer {
public:
    explicit ComputerPlayer(
        Player player,
        ComputerDifficulty difficulty = ComputerDifficulty::moderate
    );

    void update(Simulation& simulation);
    [[nodiscard]] Player player() const { return state_.player; }
    [[nodiscard]] ComputerDifficulty difficulty() const {
        return state_.difficulty;
    }
    void set_difficulty(ComputerDifficulty difficulty);
    [[nodiscard]] const ComputerPlayerState& state() const {
        return state_;
    }
    [[nodiscard]] const ComputerPlayerStatus& status() const {
        return status_;
    }
    void restore_state(ComputerPlayerState state);

private:
    ComputerPlayerState state_;
    ComputerPlayerStatus status_;
};

void save_computer_player(
    const ComputerPlayer& computer,
    const std::filesystem::path& path
);
ComputerPlayer load_computer_player(const std::filesystem::path& path);

}  // namespace aoe

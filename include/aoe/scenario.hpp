#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aoe/simulation.hpp"

namespace aoe {

struct UnitPlacement {
    UnitKind kind;
    EntityOwner owner;
    TilePosition position;
    std::optional<TilePosition> garrisoned_in;
    std::optional<TilePosition> attack_move_destination;
    std::optional<TilePosition> patrol_destination;
    std::optional<TilePosition> guard_target;
    bool guard_target_is_building{};
    std::vector<TilePosition> waypoints;
    UnitStance stance{UnitStance::aggressive};
    std::optional<int> food_remaining;
};

struct BuildingPlacement {
    BuildingKind kind;
    EntityOwner owner;
    TilePosition position;
    std::optional<TilePosition> rally_point;
    std::optional<int> hit_points;
    std::optional<int> resource_amount;
};

// Deterministic metadata for the reconstruction's custom scenario format.
// Trigger condition/effect strings are preserved for tooling; Simulation does
// not execute them.
struct ScenarioObjective {
    int id{};
    Player player{Player::blue};
    bool required{true};
    bool hidden{};
    std::string description;
};

struct ScenarioTrigger {
    ScenarioTrigger() = default;
    ScenarioTrigger(
        int source_id,
        int source_priority,
        bool source_enabled,
        bool source_looping,
        std::string source_condition,
        std::string source_effect
    ) :
        id(source_id),
        priority(source_priority),
        enabled(source_enabled),
        looping(source_looping),
        conditions{std::move(source_condition)},
        effects{std::move(source_effect)} {}
    ScenarioTrigger(
        int source_id,
        int source_priority,
        bool source_enabled,
        bool source_looping,
        std::vector<std::string> source_conditions,
        std::vector<std::string> source_effects
    ) :
        id(source_id),
        priority(source_priority),
        enabled(source_enabled),
        looping(source_looping),
        conditions(std::move(source_conditions)),
        effects(std::move(source_effects)) {}
    int id{};
    int priority{};
    bool enabled{true};
    bool looping{};
    std::vector<std::string> conditions;
    std::vector<std::string> effects;
};

struct ScenarioRosterEntry {
    MatchRosterSlot roster;
    Economy economy{100, 200, 200, 200};
    Age age{Age::dark};
    Civilization civilization{Civilization::generic};
    std::vector<Technology> technologies;
    FormationKind formation{FormationKind::compact};
};

struct DirectedDiplomacyRecord {
    PlayerSlotId from;
    PlayerSlotId to;
    Diplomacy stance{Diplomacy::enemy};
};

struct Scenario {
    explicit Scenario(int width, int height) : map(width, height) {}

    GameMap map;
    Economy blue_economy{100, 200};
    Economy red_economy{100, 200};
    Age blue_age{Age::dark};
    Age red_age{Age::dark};
    Diplomacy blue_red_diplomacy{Diplomacy::enemy};
    Civilization blue_civilization{Civilization::generic};
    Civilization red_civilization{Civilization::generic};
    std::vector<Technology> blue_technologies;
    std::vector<Technology> red_technologies;
    std::vector<UnitPlacement> units;
    std::vector<BuildingPlacement> buildings;
    std::vector<ScenarioObjective> objectives;
    std::vector<ScenarioTrigger> triggers;
    bool strict_trigger_syntax{true};
    MatchRules match_rules{};
    FormationKind blue_formation{FormationKind::compact};
    FormationKind red_formation{FormationKind::compact};
    bool enforce_civilization_availability{true};
    bool roster_schema{};
    std::vector<ScenarioRosterEntry> roster_entries;
    std::vector<DirectedDiplomacyRecord> directed_diplomacy;
};

Scenario load_scenario(const std::filesystem::path& path);
void save_scenario(
    const Scenario& scenario,
    const std::filesystem::path& path
);
Simulation create_simulation(const Scenario& scenario);

}  // namespace aoe

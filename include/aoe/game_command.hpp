#pragma once

#include <filesystem>
#include <variant>
#include <vector>

#include "aoe/simulation.hpp"

namespace aoe {

struct MoveUnitCommand {
    EntityId unit;
    TilePosition destination;
};

struct StopUnitCommand {
    EntityId unit;
};

struct GatherUnitCommand {
    EntityId villager;
    EntityId herdable;
};

struct AttackMoveCommand {
    EntityId unit;
    TilePosition destination;
};

struct AttackGroundCommand {
    EntityId unit;
    TilePosition destination;
};

struct ConvertUnitCommand {
    EntityId monk;
    EntityId target;
};

struct HealUnitCommand {
    EntityId monk;
    EntityId target;
};

struct CollectRelicCommand {
    EntityId monk;
    EntityId relic;
};

struct DepositRelicCommand {
    EntityId monk;
    EntityId monastery;
};

struct BuyResourceCommand {
    Player player;
    MarketResource resource;
};

struct SellResourceCommand {
    Player player;
    MarketResource resource;
};

struct TributeResourceCommand {
    Player from;
    Player to;
    ResourceKind resource;
    int amount;
};

struct SetDiplomacyCommand {
    Player player;
    Player other;
    Diplomacy relation;
};

struct TradeRouteCommand {
    EntityId cart;
    EntityId market;
};

struct SetCivilizationCommand {
    Player player;
    Civilization civilization;
};

struct EmbarkCommand {
    EntityId unit;
    EntityId transport;
};

struct DisembarkCommand {
    EntityId transport;
    TilePosition shore;
};

struct PatrolCommand {
    EntityId unit;
    TilePosition destination;
};

struct GuardCommand {
    EntityId unit;
    EntityId target;
    bool target_is_building;
};

struct QueueWaypointCommand {
    EntityId unit;
    TilePosition destination;
};

struct SetStanceCommand {
    EntityId unit;
    UnitStance stance;
};

struct DeleteEntityCommand {
    EntityId entity;
    bool is_building;
};

struct PackTrebuchetCommand {
    EntityId unit;
    bool pack;
};

struct ConstructBuildingCommand {
    EntityId builder;
    BuildingKind kind;
    TilePosition position;
};

struct QueueUnitCommand {
    EntityId building;
    UnitKind kind;
};

struct QueueCommercialObjectCommand {
    EntityId building;
    CommercialObjectIdentity identity;
};

struct CommercialTaskCommand {
    EntityId unit;
    std::uint16_t task_id{};
    EntityId target{};
    bool target_is_building{};
    TilePosition destination{};
};

struct SetRallyPointCommand {
    EntityId building;
    TilePosition destination;
};

struct CancelProductionCommand {
    EntityId building;
};

struct ReseedFarmCommand {
    EntityId building;
    bool legacy_immediate{};
};

struct UngarrisonCommand {
    EntityId building;
};

struct TownBellCommand {
    EntityId building;
};

struct AdvanceAgeCommand {
    EntityId building;
};

struct ResearchTechnologyCommand {
    EntityId building;
    Technology technology;
};
struct ResearchCommercialTechnologyCommand {
    EntityId building;
    CommercialTechnologyId technology;
};
struct ResignCommand {
    Player player;
};
struct SetFormationKindCommand {
    Player player;
    FormationKind kind;
};
struct MoveFormationCommand {
    std::vector<EntityId> units;
    TilePosition destination;
    FormationKind kind;
    FormationOrderKind order{FormationOrderKind::move};
    EntityId guard_target{};
    bool guard_target_is_building{};
};

using GameCommand = std::variant<
    MoveUnitCommand,
    StopUnitCommand,
    GatherUnitCommand,
    AttackMoveCommand,
    AttackGroundCommand,
    ConvertUnitCommand,
    HealUnitCommand,
    CollectRelicCommand,
    DepositRelicCommand,
    BuyResourceCommand,
    SellResourceCommand,
    TributeResourceCommand,
    SetDiplomacyCommand,
    TradeRouteCommand,
    SetCivilizationCommand,
    EmbarkCommand,
    DisembarkCommand,
    PatrolCommand,
    GuardCommand,
    QueueWaypointCommand,
    SetStanceCommand,
    DeleteEntityCommand,
    PackTrebuchetCommand,
    ConstructBuildingCommand,
    QueueUnitCommand,
    QueueCommercialObjectCommand,
    CommercialTaskCommand,
    SetRallyPointCommand,
    CancelProductionCommand,
    ReseedFarmCommand,
    UngarrisonCommand,
    TownBellCommand,
    AdvanceAgeCommand,
    ResearchTechnologyCommand,
    ResearchCommercialTechnologyCommand,
    ResignCommand,
    SetFormationKindCommand,
    MoveFormationCommand
>;

struct ScheduledCommand {
    std::uint64_t tick;
    std::optional<PlayerSlotId> source;
    GameCommand command;
};

bool execute(Simulation& simulation, const GameCommand& command);
bool execute(
    Simulation& simulation,
    const GameCommand& command,
    std::optional<PlayerSlotId> source
);

class Replay {
public:
    void record(std::uint64_t tick, GameCommand command);
    void record(
        std::uint64_t tick,
        PlayerSlotId source,
        GameCommand command
    );
    void reset_playback();
    void apply_current_tick(Simulation& simulation);

    [[nodiscard]] const std::vector<ScheduledCommand>& commands() const {
        return commands_;
    }
    [[nodiscard]] bool playback_finished() const {
        return next_command_ >= commands_.size();
    }

private:
    std::vector<ScheduledCommand> commands_;
    std::size_t next_command_{};
};

void save_replay(const Replay& replay, const std::filesystem::path& path);
Replay load_replay(const std::filesystem::path& path);

}  // namespace aoe

#include "aoe/game_command.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <type_traits>

#include "aoe/format_versions.hpp"
#include "aoe/player_codec.hpp"

namespace aoe {
namespace {

std::optional<EntityOwner> unit_owner(
    const Simulation& simulation, EntityId id
) {
    const auto found = std::ranges::find(
        simulation.units(), id, &Unit::id
    );
    return found == simulation.units().end()
        ? std::nullopt : std::optional<EntityOwner>{found->owner};
}

std::optional<EntityOwner> building_owner(
    const Simulation& simulation, EntityId id
) {
    const auto found = std::ranges::find(
        simulation.buildings(), id, &Building::id
    );
    return found == simulation.buildings().end()
        ? std::nullopt : std::optional<EntityOwner>{found->owner};
}

std::optional<EntityOwner> command_player(
    const Simulation& simulation, const GameCommand& command
) {
    return std::visit(
        [&simulation](const auto& value) -> std::optional<EntityOwner> {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (
                std::is_same_v<Type, BuyResourceCommand> ||
                std::is_same_v<Type, SellResourceCommand> ||
                std::is_same_v<Type, SetCivilizationCommand> ||
                std::is_same_v<Type, ResignCommand> ||
                std::is_same_v<Type, SetFormationKindCommand>
            ) {
                return value.player;
            } else if constexpr (
                std::is_same_v<Type, TributeResourceCommand>
            ) {
                return value.from;
            } else if constexpr (
                std::is_same_v<Type, SetDiplomacyCommand>
            ) {
                return value.player;
            } else if constexpr (
                std::is_same_v<Type, QueueUnitCommand> ||
                std::is_same_v<Type, SetRallyPointCommand> ||
                std::is_same_v<Type, CancelProductionCommand> ||
                std::is_same_v<Type, ReseedFarmCommand> ||
                std::is_same_v<Type, UngarrisonCommand> ||
                std::is_same_v<Type, AdvanceAgeCommand> ||
                std::is_same_v<Type, ResearchTechnologyCommand>
            ) {
                return building_owner(simulation, value.building);
            } else if constexpr (
                std::is_same_v<Type, DeleteEntityCommand>
            ) {
                return value.is_building
                    ? building_owner(simulation, value.entity)
                    : unit_owner(simulation, value.entity);
            } else if constexpr (
                std::is_same_v<Type, ConstructBuildingCommand>
            ) {
                return unit_owner(simulation, value.builder);
            } else if constexpr (
                std::is_same_v<Type, DisembarkCommand>
            ) {
                return unit_owner(simulation, value.transport);
            } else if constexpr (
                std::is_same_v<Type, MoveFormationCommand>
            ) {
                return value.units.empty()
                    ? std::nullopt
                    : unit_owner(simulation, value.units.front());
            } else if constexpr (
                std::is_same_v<Type, GatherUnitCommand>
            ) {
                return unit_owner(simulation, value.villager);
            } else if constexpr (
                std::is_same_v<Type, ConvertUnitCommand> ||
                std::is_same_v<Type, HealUnitCommand> ||
                std::is_same_v<Type, CollectRelicCommand> ||
                std::is_same_v<Type, DepositRelicCommand>
            ) {
                return unit_owner(simulation, value.monk);
            } else if constexpr (
                std::is_same_v<Type, TradeRouteCommand>
            ) {
                return unit_owner(simulation, value.cart);
            } else {
                return unit_owner(simulation, value.unit);
            }
        },
        command
    );
}

}  // namespace

bool execute(Simulation& simulation, const GameCommand& command) {
    return execute(simulation, command, std::nullopt);
}

bool execute(
    Simulation& simulation,
    const GameCommand& command,
    std::optional<PlayerSlotId> source
) {
    const auto owner = command_player(simulation, command);
    if (!owner) {
        return false;
    }
    const auto resolved = entity_owner_slot(*owner);
    if (!resolved || resolved->is_neutral() ||
        (source && *source != *resolved) ||
        !simulation.player_commands_allowed(*resolved)) return false;
    return std::visit(
        [&simulation](const auto& value) {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Type, MoveUnitCommand>) {
                return simulation.command_unit(value.unit, value.destination);
            } else if constexpr (std::is_same_v<Type, StopUnitCommand>) {
                return simulation.stop_unit(value.unit);
            } else if constexpr (
                std::is_same_v<Type, GatherUnitCommand>
            ) {
                return simulation.command_gather_unit(
                    value.villager,
                    value.herdable
                );
            } else if constexpr (std::is_same_v<Type, AttackMoveCommand>) {
                return simulation.command_attack_move(
                    value.unit,
                    value.destination
                );
            } else if constexpr (
                std::is_same_v<Type, AttackGroundCommand>
            ) {
                return simulation.command_attack_ground(
                    value.unit,
                    value.destination
                );
            } else if constexpr (
                std::is_same_v<Type, ConvertUnitCommand>
            ) {
                return simulation.command_convert(
                    value.monk,
                    value.target
                );
            } else if constexpr (std::is_same_v<Type, HealUnitCommand>) {
                return simulation.command_heal(value.monk, value.target);
            } else if constexpr (
                std::is_same_v<Type, CollectRelicCommand>
            ) {
                return simulation.command_collect_relic(
                    value.monk,
                    value.relic
                );
            } else if constexpr (
                std::is_same_v<Type, DepositRelicCommand>
            ) {
                return simulation.command_deposit_relic(
                    value.monk,
                    value.monastery
                );
            } else if constexpr (
                std::is_same_v<Type, BuyResourceCommand>
            ) {
                return simulation.buy_resource(value.player, value.resource);
            } else if constexpr (
                std::is_same_v<Type, SellResourceCommand>
            ) {
                return simulation.sell_resource(value.player, value.resource);
            } else if constexpr (
                std::is_same_v<Type, TributeResourceCommand>
            ) {
                return simulation.tribute_resource(
                    value.from, value.to, value.resource, value.amount
                );
            } else if constexpr (
                std::is_same_v<Type, SetDiplomacyCommand>
            ) {
                return simulation.set_diplomacy(
                    value.player, value.other, value.relation
                );
            } else if constexpr (
                std::is_same_v<Type, TradeRouteCommand>
            ) {
                return simulation.command_trade_route(
                    value.cart, value.market
                );
            } else if constexpr (
                std::is_same_v<Type, SetCivilizationCommand>
            ) {
                return simulation.set_civilization(
                    value.player, value.civilization
                );
            } else if constexpr (std::is_same_v<Type, EmbarkCommand>) {
                return simulation.command_embark(
                    value.unit, value.transport
                );
            } else if constexpr (std::is_same_v<Type, DisembarkCommand>) {
                return simulation.command_disembark(
                    value.transport, value.shore
                );
            } else if constexpr (std::is_same_v<Type, PatrolCommand>) {
                return simulation.command_patrol(
                    value.unit,
                    value.destination
                );
            } else if constexpr (std::is_same_v<Type, GuardCommand>) {
                return simulation.command_guard(
                    value.unit,
                    value.target,
                    value.target_is_building
                );
            } else if constexpr (
                std::is_same_v<Type, QueueWaypointCommand>
            ) {
                return simulation.queue_waypoint(
                    value.unit,
                    value.destination
                );
            } else if constexpr (std::is_same_v<Type, SetStanceCommand>) {
                return simulation.set_unit_stance(
                    value.unit,
                    value.stance
                );
            } else if constexpr (
                std::is_same_v<Type, DeleteEntityCommand>
            ) {
                return value.is_building
                    ? simulation.delete_building(value.entity)
                    : simulation.delete_unit(value.entity);
            } else if constexpr (
                std::is_same_v<Type, PackTrebuchetCommand>
            ) {
                return simulation.command_pack_trebuchet(
                    value.unit, value.pack
                );
            } else if constexpr (
                std::is_same_v<Type, ConstructBuildingCommand>
            ) {
                return simulation.construct_building_at(
                    value.builder,
                    value.kind,
                    value.position
                );
            } else if constexpr (std::is_same_v<Type, QueueUnitCommand>) {
                return simulation.queue_unit_at(value.building, value.kind);
            } else if constexpr (
                std::is_same_v<Type, SetRallyPointCommand>
            ) {
                return simulation.set_rally_point(
                    value.building,
                    value.destination
                );
            } else if constexpr (
                std::is_same_v<Type, CancelProductionCommand>
            ) {
                return simulation.cancel_production_at(value.building);
            } else if constexpr (std::is_same_v<Type, ReseedFarmCommand>) {
                return value.legacy_immediate
                    ? simulation.reseed_farm_immediately(value.building)
                    : simulation.reseed_farm(value.building);
            } else if constexpr (std::is_same_v<Type, UngarrisonCommand>) {
                return simulation.ungarrison_at(value.building);
            } else if constexpr (std::is_same_v<Type, AdvanceAgeCommand>) {
                return simulation.advance_age_at(value.building);
            } else if constexpr (
                std::is_same_v<Type, ResearchTechnologyCommand>
            ) {
                return simulation.research_technology_at(
                    value.building,
                    value.technology
                );
            } else if constexpr (std::is_same_v<Type, ResignCommand>) {
                return simulation.resign(value.player);
            } else if constexpr (
                std::is_same_v<Type, SetFormationKindCommand>
            ) {
                return simulation.set_formation_kind(
                    value.player, value.kind
                );
            } else if constexpr (
                std::is_same_v<Type, MoveFormationCommand>
            ) {
                return simulation.command_formation_order(
                    value.units, value.destination, value.kind,
                    value.order, value.guard_target,
                    value.guard_target_is_building
                );
            } else {
                return false;
            }
        },
        command
    );
}

void Replay::record(std::uint64_t tick, GameCommand command) {
    if (!commands_.empty() && tick < commands_.back().tick) {
        throw std::invalid_argument(
            "replay commands must be recorded in tick order"
        );
    }
    commands_.push_back({tick, std::nullopt, std::move(command)});
}

void Replay::record(
    std::uint64_t tick,
    PlayerSlotId source,
    GameCommand command
) {
    if (source.is_neutral()) {
        throw std::invalid_argument("neutral cannot source commands");
    }
    if (!commands_.empty() && tick < commands_.back().tick) {
        throw std::invalid_argument(
            "replay commands must be recorded in tick order"
        );
    }
    commands_.push_back({tick, source, std::move(command)});
}

void Replay::reset_playback() {
    next_command_ = 0;
}

void Replay::apply_current_tick(Simulation& simulation) {
    while (
        next_command_ < commands_.size() &&
        commands_[next_command_].tick == simulation.tick_number()
    ) {
        if (!execute(
                simulation,
                commands_[next_command_].command,
                commands_[next_command_].source
            )) {
            throw std::runtime_error(
                "replay command rejected; simulation diverged"
            );
        }
        ++next_command_;
    }
}

void save_replay(const Replay& replay, const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not open replay file");
    }
    output << "AOE-ARCHAEOLOGY-REPLAY "
           << reconstruction_command_schema_version << '\n';
    for (const ScheduledCommand& scheduled : replay.commands()) {
        output << "source "
               << (scheduled.source
                       ? static_cast<int>(scheduled.source->stable_id())
                       : -1)
               << '\n';
        std::visit(
            [&output, tick = scheduled.tick](const auto& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, MoveUnitCommand>) {
                    output << "move " << tick << ' ' << value.unit << ' '
                           << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, StopUnitCommand>
                ) {
                    output << "stop " << tick << ' ' << value.unit << '\n';
                } else if constexpr (
                    std::is_same_v<Type, GatherUnitCommand>
                ) {
                    output << "gather-unit " << tick << ' '
                           << value.villager << ' '
                           << value.herdable << '\n';
                } else if constexpr (
                    std::is_same_v<Type, AttackMoveCommand>
                ) {
                    output << "attack-move " << tick << ' ' << value.unit
                           << ' ' << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, AttackGroundCommand>
                ) {
                    output << "attack-ground " << tick << ' '
                           << value.unit << ' '
                           << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, ConvertUnitCommand>
                ) {
                    output << "convert " << tick << ' ' << value.monk
                           << ' ' << value.target << '\n';
                } else if constexpr (
                    std::is_same_v<Type, HealUnitCommand>
                ) {
                    output << "heal " << tick << ' ' << value.monk
                           << ' ' << value.target << '\n';
                } else if constexpr (
                    std::is_same_v<Type, CollectRelicCommand>
                ) {
                    output << "collect-relic " << tick << ' '
                           << value.monk << ' ' << value.relic << '\n';
                } else if constexpr (
                    std::is_same_v<Type, DepositRelicCommand>
                ) {
                    output << "deposit-relic " << tick << ' '
                           << value.monk << ' ' << value.monastery << '\n';
                } else if constexpr (
                    std::is_same_v<Type, BuyResourceCommand>
                ) {
                    output << "buy-resource " << tick << ' '
                           << encode_player_wire(value.player) << ' '
                           << static_cast<int>(value.resource) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SellResourceCommand>
                ) {
                    output << "sell-resource " << tick << ' '
                           << encode_player_wire(value.player) << ' '
                           << static_cast<int>(value.resource) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, TributeResourceCommand>
                ) {
                    output << "tribute " << tick << ' '
                           << encode_player_wire(value.from) << ' '
                           << encode_player_wire(value.to) << ' '
                           << static_cast<int>(value.resource) << ' '
                           << value.amount << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SetDiplomacyCommand>
                ) {
                    output << "diplomacy " << tick << ' '
                           << encode_player_wire(value.player) << ' '
                           << encode_player_wire(value.other) << ' '
                           << static_cast<int>(value.relation) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, TradeRouteCommand>
                ) {
                    output << "trade-route " << tick << ' ' << value.cart
                           << ' ' << value.market << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SetCivilizationCommand>
                ) {
                    output << "civilization " << tick << ' '
                           << encode_player_wire(value.player) << ' '
                           << static_cast<int>(value.civilization) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, EmbarkCommand>
                ) {
                    output << "embark " << tick << ' ' << value.unit << ' '
                           << value.transport << '\n';
                } else if constexpr (
                    std::is_same_v<Type, DisembarkCommand>
                ) {
                    output << "disembark " << tick << ' '
                           << value.transport << ' ' << value.shore.x << ' '
                           << value.shore.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, PatrolCommand>
                ) {
                    output << "patrol " << tick << ' ' << value.unit << ' '
                           << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, GuardCommand>
                ) {
                    output << "guard " << tick << ' ' << value.unit << ' '
                           << value.target << ' '
                           << value.target_is_building << '\n';
                } else if constexpr (
                    std::is_same_v<Type, QueueWaypointCommand>
                ) {
                    output << "waypoint " << tick << ' ' << value.unit << ' '
                           << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SetStanceCommand>
                ) {
                    output << "stance " << tick << ' ' << value.unit << ' '
                           << static_cast<int>(value.stance) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, DeleteEntityCommand>
                ) {
                    output << "delete " << tick << ' ' << value.entity << ' '
                           << value.is_building << '\n';
                } else if constexpr (
                    std::is_same_v<Type, PackTrebuchetCommand>
                ) {
                    output << "pack-trebuchet " << tick << ' ' << value.unit
                           << ' ' << value.pack << '\n';
                } else if constexpr (
                    std::is_same_v<Type, ConstructBuildingCommand>
                ) {
                    output << "build " << tick << ' ' << value.builder << ' '
                           << static_cast<int>(value.kind) << ' '
                           << value.position.x << ' ' << value.position.y
                           << '\n';
                } else if constexpr (
                    std::is_same_v<Type, QueueUnitCommand>
                ) {
                    output << "queue " << tick << ' ' << value.building << ' '
                           << static_cast<int>(value.kind) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SetRallyPointCommand>
                ) {
                    output << "rally " << tick << ' ' << value.building << ' '
                           << value.destination.x << ' '
                           << value.destination.y << '\n';
                } else if constexpr (
                    std::is_same_v<Type, CancelProductionCommand>
                ) {
                    output << "cancel-production " << tick << ' '
                           << value.building << '\n';
                } else if constexpr (
                    std::is_same_v<Type, ReseedFarmCommand>
                ) {
                    output << (value.legacy_immediate
                                   ? "reseed " : "queue-farm ")
                           << tick << ' ' << value.building
                           << '\n';
                } else if constexpr (
                    std::is_same_v<Type, UngarrisonCommand>
                ) {
                    output << "ungarrison " << tick << ' ' << value.building
                           << '\n';
                } else if constexpr (
                    std::is_same_v<Type, AdvanceAgeCommand>
                ) {
                    output << "advance " << tick << ' ' << value.building
                           << '\n';
                } else if constexpr (
                    std::is_same_v<Type, ResearchTechnologyCommand>
                ) {
                    output << "research " << tick << ' ' << value.building
                           << ' ' << static_cast<int>(value.technology)
                           << '\n';
                } else if constexpr (std::is_same_v<Type, ResignCommand>) {
                    output << "resign " << tick << ' '
                           << encode_player_wire(value.player) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, SetFormationKindCommand>
                ) {
                    output << "formation-kind " << tick << ' '
                           << encode_player_wire(value.player) << ' '
                           << static_cast<int>(value.kind) << '\n';
                } else if constexpr (
                    std::is_same_v<Type, MoveFormationCommand>
                ) {
                    output << "formation-move " << tick << ' '
                           << static_cast<int>(value.kind) << ' '
                           << static_cast<int>(value.order) << ' '
                           << value.destination.x << ' '
                           << value.destination.y << ' '
                           << value.guard_target << ' '
                           << value.guard_target_is_building << ' '
                           << value.units.size();
                    for (EntityId id : value.units) output << ' ' << id;
                    output << '\n';
                }
            },
            scheduled.command
        );
    }
}

Replay load_replay(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string magic;
    int version{};
    input >> magic >> version;
    if (!input || magic != "AOE-ARCHAEOLOGY-REPLAY" ||
        version < 1 ||
        version > reconstruction_command_schema_version) {
        throw std::runtime_error("unsupported or corrupt replay file");
    }

    Replay replay;
    std::vector<std::optional<PlayerSlotId>> native_sources;
    const auto in_range = [](int value, auto first, auto last) {
        return value >= static_cast<int>(first) &&
            value <= static_cast<int>(last);
    };
    const auto valid_player = [](int value) {
        const auto player = decode_player_wire(value);
        return player && is_playable_player(*player);
    };
    std::string record;
    while (input >> record) {
        std::uint64_t tick{};
        if (record == "source" && version >= 63) {
            int source{};
            input >> source;
            if (native_sources.size() != replay.commands().size()) {
                throw std::runtime_error(
                    "duplicate or dangling replay command source"
                );
            }
            if (source == -1) {
                native_sources.push_back(std::nullopt);
            } else {
                const auto decoded = decode_player_slot_id(source);
                if (!decoded || decoded->is_neutral()) {
                    throw std::runtime_error(
                        "invalid replay command source"
                    );
                }
                native_sources.push_back(*decoded);
            }
        } else if (record == "move") {
            MoveUnitCommand command;
            input >> tick >> command.unit >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (record == "stop" && version >= 23) {
            StopUnitCommand command;
            input >> tick >> command.unit;
            replay.record(tick, command);
        } else if (record == "gather-unit" && version >= 31) {
            GatherUnitCommand command;
            input >> tick >> command.villager >> command.herdable;
            replay.record(tick, command);
        } else if (record == "attack-move" && version >= 24) {
            AttackMoveCommand command;
            input >> tick >> command.unit >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (record == "attack-ground" && version >= 30) {
            AttackGroundCommand command;
            input >> tick >> command.unit >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (record == "convert" && version >= 32) {
            ConvertUnitCommand command;
            input >> tick >> command.monk >> command.target;
            replay.record(tick, command);
        } else if (record == "heal" && version >= 33) {
            HealUnitCommand command;
            input >> tick >> command.monk >> command.target;
            replay.record(tick, command);
        } else if (record == "collect-relic" && version >= 33) {
            CollectRelicCommand command;
            input >> tick >> command.monk >> command.relic;
            replay.record(tick, command);
        } else if (record == "deposit-relic" && version >= 33) {
            DepositRelicCommand command;
            input >> tick >> command.monk >> command.monastery;
            replay.record(tick, command);
        } else if (record == "buy-resource" && version >= 34) {
            BuyResourceCommand command;
            int player{};
            int resource{};
            input >> tick >> player >> resource;
            if (!valid_player(player) ||
                !in_range(
                    resource, MarketResource::food, MarketResource::stone
                )) {
                throw std::runtime_error("invalid buy-resource record");
            }
            command.player = *decode_player_wire(player);
            command.resource = static_cast<MarketResource>(resource);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "sell-resource" && version >= 34) {
            SellResourceCommand command;
            int player{};
            int resource{};
            input >> tick >> player >> resource;
            if (!valid_player(player) ||
                !in_range(
                    resource, MarketResource::food, MarketResource::stone
                )) {
                throw std::runtime_error("invalid sell-resource record");
            }
            command.player = *decode_player_wire(player);
            command.resource = static_cast<MarketResource>(resource);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "tribute" && version >= 58) {
            TributeResourceCommand command;
            int from{};
            int to{};
            int resource{};
            input >> tick >> from >> to >> resource >> command.amount;
            if (!valid_player(from) || !valid_player(to) ||
                !in_range(resource, ResourceKind::wood, ResourceKind::stone) ||
                command.amount <= 0) {
                throw std::runtime_error("invalid tribute record");
            }
            command.from = *decode_player_wire(from);
            command.to = *decode_player_wire(to);
            command.resource = static_cast<ResourceKind>(resource);
            replay.record(
                tick, *player_slot_from_legacy(command.from), command
            );
        } else if (record == "diplomacy" && version >= 35) {
            SetDiplomacyCommand command;
            int player{};
            int other{};
            int relation{};
            input >> tick >> player >> other >> relation;
            if (!valid_player(player) || !valid_player(other) ||
                !in_range(
                    relation, Diplomacy::ally, Diplomacy::enemy
                )) {
                throw std::runtime_error("invalid diplomacy record");
            }
            command.player = *decode_player_wire(player);
            command.other = *decode_player_wire(other);
            command.relation = static_cast<Diplomacy>(relation);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "trade-route" && version >= 35) {
            TradeRouteCommand command;
            input >> tick >> command.cart >> command.market;
            replay.record(tick, command);
        } else if (record == "civilization" && version >= 36) {
            SetCivilizationCommand command;
            int player{};
            int civilization{};
            input >> tick >> player >> civilization;
            if (!valid_player(player) ||
                !in_range(
                    civilization,
                    Civilization::generic,
                    Civilization::mayans
                )) {
                throw std::runtime_error("invalid civilization record");
            }
            command.player = *decode_player_wire(player);
            command.civilization =
                static_cast<Civilization>(civilization);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "embark" && version >= 37) {
            EmbarkCommand command;
            input >> tick >> command.unit >> command.transport;
            replay.record(tick, command);
        } else if (record == "disembark" && version >= 37) {
            DisembarkCommand command;
            input >> tick >> command.transport >>
                command.shore.x >> command.shore.y;
            replay.record(tick, command);
        } else if (record == "patrol" && version >= 25) {
            PatrolCommand command;
            input >> tick >> command.unit >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (record == "guard" && version >= 26) {
            GuardCommand command;
            input >> tick >> command.unit >> command.target >>
                command.target_is_building;
            replay.record(tick, command);
        } else if (record == "waypoint" && version >= 27) {
            QueueWaypointCommand command;
            input >> tick >> command.unit >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (record == "stance" && version >= 28) {
            SetStanceCommand command;
            int stance{};
            input >> tick >> command.unit >> stance;
            if (!in_range(
                    stance, UnitStance::aggressive, UnitStance::passive
                )) {
                throw std::runtime_error("invalid stance record");
            }
            command.stance = static_cast<UnitStance>(stance);
            replay.record(tick, command);
        } else if (record == "delete" && version >= 29) {
            DeleteEntityCommand command;
            input >> tick >> command.entity >> command.is_building;
            replay.record(tick, command);
        } else if (record == "pack-trebuchet" && version >= 46) {
            PackTrebuchetCommand command;
            input >> tick >> command.unit >> command.pack;
            replay.record(tick, command);
        } else if (record == "build") {
            ConstructBuildingCommand command;
            int kind{};
            input >> tick >> command.builder >> kind >>
                command.position.x >> command.position.y;
            if (!in_range(
                    kind, BuildingKind::town_center, BuildingKind::wonder
                )) {
                throw std::runtime_error("invalid build record");
            }
            command.kind = static_cast<BuildingKind>(kind);
            replay.record(tick, command);
        } else if (record == "queue") {
            QueueUnitCommand command;
            int kind{};
            input >> tick >> command.building >> kind;
            if (!in_range(
                    kind, UnitKind::villager, UnitKind::elite_woad_raider
                )) {
                throw std::runtime_error("invalid queue record");
            }
            command.kind = static_cast<UnitKind>(kind);
            replay.record(tick, command);
        } else if (record == "rally" && version >= 21) {
            SetRallyPointCommand command;
            input >> tick >> command.building >>
                command.destination.x >> command.destination.y;
            replay.record(tick, command);
        } else if (
            record == "cancel-production" && version >= 22
        ) {
            CancelProductionCommand command;
            input >> tick >> command.building;
            replay.record(tick, command);
        } else if (
            (record == "reseed" && version >= 2) ||
            (record == "queue-farm" && version >= 62)
        ) {
            ReseedFarmCommand command;
            input >> tick >> command.building;
            command.legacy_immediate = record == "reseed";
            replay.record(tick, command);
        } else if (record == "ungarrison" && version >= 20) {
            UngarrisonCommand command;
            input >> tick >> command.building;
            replay.record(tick, command);
        } else if (record == "advance" && version >= 3) {
            AdvanceAgeCommand command;
            input >> tick >> command.building;
            replay.record(tick, command);
        } else if (record == "research" && version >= 4) {
            ResearchTechnologyCommand command;
            int technology{};
            input >> tick >> command.building >> technology;
            if (!in_range(
                    technology,
                    Technology::wheelbarrow,
                    Technology::wonder_plans
                )) {
                throw std::runtime_error("invalid research record");
            }
            command.technology = static_cast<Technology>(technology);
            replay.record(tick, command);
        } else if (record == "resign" && version >= 60) {
            ResignCommand command;
            int player{};
            input >> tick >> player;
            if (!valid_player(player)) {
                throw std::runtime_error("invalid resign record");
            }
            command.player = *decode_player_wire(player);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "formation-kind" && version >= 61) {
            SetFormationKindCommand command;
            int player{};
            int kind{};
            input >> tick >> player >> kind;
            if (!valid_player(player) ||
                kind < static_cast<int>(FormationKind::compact) ||
                kind > static_cast<int>(FormationKind::flank)) {
                throw std::runtime_error("invalid formation-kind record");
            }
            command.player = *decode_player_wire(player);
            command.kind = static_cast<FormationKind>(kind);
            replay.record(
                tick, *player_slot_from_legacy(command.player), command
            );
        } else if (record == "formation-move" && version >= 61) {
            MoveFormationCommand command;
            int kind{};
            int order{};
            std::size_t count{};
            input >> tick >> kind >> order >> command.destination.x >>
                command.destination.y >> command.guard_target >>
                command.guard_target_is_building >> count;
            if (kind < static_cast<int>(FormationKind::compact) ||
                kind > static_cast<int>(FormationKind::flank) ||
                order < static_cast<int>(FormationOrderKind::move) ||
                order > static_cast<int>(
                    FormationOrderKind::queued_waypoint
                ) ||
                count == 0 || count > 200) {
                throw std::runtime_error("invalid formation replay record");
            }
            command.kind = static_cast<FormationKind>(kind);
            command.order = static_cast<FormationOrderKind>(order);
            command.units.resize(count);
            for (EntityId& id : command.units) input >> id;
            replay.record(tick, command);
        } else {
            throw std::runtime_error("unknown replay record: " + record);
        }
        if (!input) {
            throw std::runtime_error("malformed replay record: " + record);
        }
    }
    if (version < 63) return replay;
    if (native_sources.size() != replay.commands().size()) {
        throw std::runtime_error("missing replay command source");
    }
    Replay native;
    for (std::size_t index = 0; index < replay.commands().size(); ++index) {
        const ScheduledCommand& command = replay.commands()[index];
        if (native_sources[index]) {
            native.record(
                command.tick, *native_sources[index], command.command
            );
        } else {
            native.record(command.tick, command.command);
        }
    }
    return native;
}

}  // namespace aoe

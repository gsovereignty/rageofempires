#include "aoe/gameplay_test_api.hpp"

#include "aoe/game_command.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace aoe {
namespace {

std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u" << std::hex << std::setw(4)
                           << std::setfill('0')
                           << static_cast<unsigned>(character)
                           << std::dec << std::setfill(' ');
                } else {
                    output << character;
                }
        }
    }
    return output.str();
}

std::string error_response(std::string_view message) {
    return "{\"ok\":false,\"error\":\"" +
        json_escape(message) + "\"}";
}

std::string wire_name(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        if (character == ' ' || character == '-') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return result;
}

const Unit* unit_by_id(const Simulation& simulation, EntityId id) {
    const auto found = std::ranges::find_if(
        simulation.units(),
        [id](const Unit& unit) { return unit.id == id; }
    );
    return found == simulation.units().end() ? nullptr : &*found;
}

const Building* building_by_id(
    const Simulation& simulation,
    EntityId id
) {
    const auto found = std::ranges::find_if(
        simulation.buildings(),
        [id](const Building& building) { return building.id == id; }
    );
    return found == simulation.buildings().end() ? nullptr : &*found;
}

std::optional<BuildingKind> parse_building_kind(std::string_view value) {
    for (std::size_t index = 0; index < building_kind_count; ++index) {
        const auto kind = static_cast<BuildingKind>(index);
        if (wire_name(name(kind)) == value) return kind;
    }
    return std::nullopt;
}

std::optional<UnitKind> parse_unit_kind(std::string_view value) {
    for (std::size_t index = 0; index < unit_kind_count; ++index) {
        const auto kind = static_cast<UnitKind>(index);
        if (wire_name(name(kind)) == value) return kind;
    }
    return std::nullopt;
}

std::optional<Technology> parse_technology(std::string_view value) {
    for (std::size_t index = 0; index < technology_count; ++index) {
        const auto technology = static_cast<Technology>(index);
        if (wire_name(name(technology)) == value) return technology;
    }
    return std::nullopt;
}

std::optional<MarketResource> parse_market_resource(std::string_view value) {
    if (value == "food") return MarketResource::food;
    if (value == "wood") return MarketResource::wood;
    if (value == "stone") return MarketResource::stone;
    return std::nullopt;
}

bool idle(const Unit& unit) {
    return unit.garrisoned_in == 0 && !unit.moving && unit.path.empty() &&
        !unit.has_resource_target && unit.attack_target_id == 0 &&
        unit.repair_target_id == 0 && unit.garrison_target_id == 0 &&
        !unit.attack_moving && !unit.patrolling && !unit.attacking_ground &&
        unit.guard_target_id == 0 && unit.conversion_target_id == 0 &&
        unit.healing_target_id == 0 && unit.relic_target_id == 0 &&
        unit.trade_target_market_id == 0;
}

void append_unit(std::ostringstream& output, const Unit& unit) {
    const std::optional<Player> legacy_owner = unit.owner.legacy_player();
    output << "{\"id\":" << unit.id
           << ",\"kind\":\"" << json_escape(wire_name(name(unit.kind)))
           << "\""
           << ",\"owner\":\""
           << (legacy_owner ? json_escape(name(*legacy_owner))
                            : "player-" +
                                  std::to_string(unit.owner.stable_id()))
           << "\",\"x\":" << unit.position.x
           << ",\"y\":" << unit.position.y
           << ",\"destination_x\":" << unit.destination.x
           << ",\"destination_y\":" << unit.destination.y
           << ",\"hp\":" << unit.hit_points
           << ",\"moving\":" << (unit.moving ? "true" : "false")
           << ",\"idle\":" << (idle(unit) ? "true" : "false")
           << ",\"gathering\":"
           << (unit.has_resource_target ? "true" : "false")
           << ",\"returning_resource\":"
           << (unit.returning_resource ? "true" : "false")
           << ",\"carried_resource\":\""
           << json_escape(name(unit.carried_resource)) << "\""
           << ",\"carried_amount\":" << unit.carried_amount << '}';
}

void append_building(std::ostringstream& output, const Building& building) {
    const std::optional<Player> legacy_owner = building.owner.legacy_player();
    output << "{\"id\":" << building.id
           << ",\"kind\":\""
           << json_escape(wire_name(name(building.kind))) << "\""
           << ",\"owner\":\""
           << (legacy_owner ? json_escape(name(*legacy_owner))
                            : "player-" +
                                  std::to_string(building.owner.stable_id()))
           << "\",\"x\":" << building.position.x
           << ",\"y\":" << building.position.y
           << ",\"hp\":" << building.hit_points
           << ",\"completed\":"
           << (building.completed() ? "true" : "false")
           << ",\"production_queue_size\":"
           << building.production_queue.size()
           << ",\"age_research_target\":\""
           << json_escape(name(building.age_research_target)) << "\""
           << ",\"age_research_ticks_remaining\":"
           << building.age_research_ticks_remaining
           << ",\"technology_research_target\":\""
           << json_escape(wire_name(
                  name(building.technology_research_target)
              )) << "\""
           << ",\"technology_research_ticks_remaining\":"
           << building.technology_research_ticks_remaining << '}';
}

}  // namespace

GameplayTestApi::GameplayTestApi(std::filesystem::path directory)
    : directory_(std::move(directory)),
      commands_path_(directory_ / "commands.jsonl"),
      responses_path_(directory_ / "responses.jsonl") {
    std::filesystem::create_directories(directory_);
    std::ofstream{commands_path_, std::ios::app};
    std::ofstream{responses_path_, std::ios::app};
    command_offset_ = std::filesystem::file_size(commands_path_);
    std::ofstream{directory_ / "ready", std::ios::trunc} << "ready\n";
}

std::string GameplayTestApi::snapshot(
    const Simulation& simulation,
    Player player,
    bool include_units
) {
    const Economy economy = simulation.economy(player);
    std::vector<const Unit*> live_units;
    int idle_units = 0;
    for (const Unit& unit : simulation.units()) {
        if (unit.owner == player && unit.garrisoned_in == 0) {
            idle_units += idle(unit) ? 1 : 0;
        }
        if (unit.garrisoned_in == 0) live_units.push_back(&unit);
    }

    std::ostringstream output;
    output << "{\"ok\":true,\"tick\":" << simulation.tick_number()
           << ",\"outcome\":\"" << json_escape(name(simulation.outcome()))
           << "\",\"resources\":{\"wood\":" << economy.wood
           << ",\"food\":" << economy.food
           << ",\"gold\":" << economy.gold
           << ",\"stone\":" << economy.stone
           << "},\"population\":" << simulation.population(player)
           << ",\"population_capacity\":"
           << simulation.population_capacity(player)
           << ",\"age\":\"" << json_escape(name(simulation.age(player)))
           << "\""
           << ",\"idle_units\":" << idle_units
           << ",\"selected_units\":[";
    for (std::size_t index = 0;
         index < simulation.selected_units().size(); ++index) {
        if (index != 0) output << ',';
        output << simulation.selected_units()[index];
    }
    output << "],\"selected_building\":";
    if (simulation.selected_building()) {
        output << *simulation.selected_building();
    } else {
        output << "null";
    }
    if (include_units) {
        output << ",\"units\":[";
        for (std::size_t index = 0; index < live_units.size(); ++index) {
            if (index != 0) output << ',';
            append_unit(output, *live_units[index]);
        }
        output << ']';
    }
    output << '}';
    return output.str();
}

std::string GameplayTestApi::execute(
    Simulation& simulation,
    Player player,
    std::string_view command
) {
    std::istringstream input{std::string{command}};
    std::string operation;
    input >> operation;
    bool quiet = false;
    if (operation == "quiet") {
        quiet = true;
        std::string remainder;
        std::getline(input >> std::ws, remainder);
        input = std::istringstream{remainder};
        input >> operation;
    }
    const auto success = [&] {
        return quiet ? std::string{"{\"ok\":true}"}
                     : snapshot(simulation, player, false);
    };
    if (operation == "batch") {
        std::string commands;
        std::getline(input >> std::ws, commands);
        if (commands.empty()) {
            return error_response("batch requires semicolon-separated commands");
        }
        std::istringstream command_stream{commands};
        std::string item;
        int completed = 0;
        while (std::getline(command_stream, item, ';')) {
            const std::size_t first = item.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            const std::string result = execute(
                simulation, player, "quiet " + item.substr(first)
            );
            if (result.find("\"ok\":false") != std::string::npos) {
                return result;
            }
            ++completed;
        }
        if (completed == 0) {
            return error_response("batch contains no commands");
        }
        std::string result = snapshot(simulation, player, false);
        result.pop_back();
        return result + ",\"completed_commands\":" +
            std::to_string(completed) + "}";
    }
    if (operation == "state" || operation == "resources" ||
        operation == "idle_units") {
        return success();
    }
    if (operation == "list_units") {
        return snapshot(simulation, player, true);
    }
    if (operation == "list_buildings") {
        std::ostringstream output;
        output << "{\"ok\":true,\"buildings\":[";
        bool first = true;
        for (const Building& building : simulation.buildings()) {
            if (!first) output << ',';
            append_building(output, building);
            first = false;
        }
        output << "]}";
        return output.str();
    }
    if (operation == "observe") {
        std::string result = snapshot(simulation, player, true);
        result.pop_back();
        std::ostringstream buildings;
        buildings << ",\"buildings\":[";
        bool first = true;
        for (const Building& building : simulation.buildings()) {
            if (!first) buildings << ',';
            append_building(buildings, building);
            first = false;
        }
        buildings << "]}";
        return result + buildings.str();
    }
    if (operation == "select") {
        EntityId id{};
        if (!(input >> id)) return error_response("select requires unit id");
        const Unit* unit = unit_by_id(simulation, id);
        if (unit == nullptr || unit->owner != player ||
            !simulation.select_units({id}, player)) {
            return error_response("unit cannot be selected");
        }
        return success();
    }
    if (operation == "move") {
        EntityId id{};
        TilePosition destination{};
        if (!(input >> id >> destination.x >> destination.y)) {
            return error_response("move requires unit id, x, and y");
        }
        const Unit* unit = unit_by_id(simulation, id);
        if (unit == nullptr || unit->owner != player ||
            !simulation.command_unit(id, destination)) {
            return error_response("move command rejected");
        }
        return success();
    }
    if (operation == "attack_move") {
        EntityId id{};
        TilePosition destination{};
        if (!(input >> id >> destination.x >> destination.y)) {
            return error_response(
                "attack_move requires unit id, x, and y"
            );
        }
        const Unit* unit = unit_by_id(simulation, id);
        if (unit == nullptr || unit->owner != player ||
            !aoe::execute(
                simulation,
                AttackMoveCommand{id, destination}
            )) {
            return error_response("attack_move command rejected");
        }
        return success();
    }
    if (operation == "select_building") {
        EntityId id{};
        if (!(input >> id)) {
            return error_response("select_building requires building id");
        }
        const Building* building = building_by_id(simulation, id);
        if (building == nullptr || building->owner != player ||
            !simulation.select_building_at(building->position, player)) {
            return error_response("building cannot be selected");
        }
        return success();
    }
    if (operation == "select_building_at") {
        TilePosition position{};
        if (!(input >> position.x >> position.y)) {
            return error_response("select_building_at requires x and y");
        }
        if (!simulation.select_building_at(position, player)) {
            return error_response("building cannot be selected");
        }
        return success();
    }
    if (operation == "select_building_kind") {
        std::string kind_name;
        if (!(input >> kind_name)) {
            return error_response(
                "select_building_kind requires building kind"
            );
        }
        const auto kind = parse_building_kind(kind_name);
        if (!kind) return error_response("unknown building kind");
        const auto found = std::ranges::find_if(
            simulation.buildings(),
            [player, kind](const Building& building) {
                return building.owner == player && building.kind == *kind;
            }
        );
        if (found == simulation.buildings().end() ||
            !simulation.select_building_at(found->position, player)) {
            return error_response("building cannot be selected");
        }
        return success();
    }
    if (operation == "train") {
        EntityId building_id{};
        std::string kind_name;
        if (!(input >> building_id >> kind_name)) {
            return error_response("train requires building id and unit kind");
        }
        const Building* building = building_by_id(simulation, building_id);
        const auto kind = parse_unit_kind(kind_name);
        if (!kind) return error_response("unknown unit kind");
        if (building == nullptr || building->owner != player ||
            !simulation.queue_unit_at(building_id, *kind)) {
            return error_response("train command rejected");
        }
        return success();
    }
    if (operation == "construct") {
        EntityId villager_id{};
        std::string kind_name;
        TilePosition position{};
        if (!(input >> villager_id >> kind_name >>
              position.x >> position.y)) {
            return error_response(
                "construct requires villager id, building kind, x, and y"
            );
        }
        const Unit* villager = unit_by_id(simulation, villager_id);
        const auto kind = parse_building_kind(kind_name);
        if (!kind) return error_response("unknown building kind");
        if (villager == nullptr || villager->owner != player ||
            !aoe::execute(
                simulation,
                ConstructBuildingCommand{villager_id, *kind, position}
            )) {
            return error_response("construct command rejected");
        }
        return success();
    }
    if (operation == "research") {
        EntityId building_id{};
        std::string technology_name;
        if (!(input >> building_id >> technology_name)) {
            return error_response(
                "research requires building id and technology"
            );
        }
        const Building* building = building_by_id(simulation, building_id);
        const auto technology = parse_technology(technology_name);
        if (!technology) return error_response("unknown technology");
        if (building == nullptr || building->owner != player ||
            !aoe::execute(
                simulation,
                ResearchTechnologyCommand{building_id, *technology}
            )) {
            return error_response("research command rejected");
        }
        return success();
    }
    if (operation == "advance_age") {
        EntityId building_id{};
        if (!(input >> building_id)) {
            return error_response("advance_age requires building id");
        }
        const Building* building = building_by_id(simulation, building_id);
        if (building == nullptr || building->owner != player ||
            !simulation.advance_age_at(building_id)) {
            return error_response("advance_age command rejected");
        }
        return success();
    }
    if (operation == "market_buy" || operation == "market_sell") {
        EntityId market_id{};
        std::string resource_name;
        if (!(input >> market_id >> resource_name)) {
            return error_response(
                operation + " requires market id and resource"
            );
        }
        const Building* market = building_by_id(simulation, market_id);
        const auto resource = parse_market_resource(resource_name);
        if (!resource) return error_response("unknown market resource");
        if (market == nullptr || market->owner != player ||
            market->kind != BuildingKind::market || !market->completed() ||
            market->hit_points <= 0) {
            return error_response("market cannot be used");
        }
        const bool accepted = operation == "market_buy"
            ? simulation.buy_resource(player, *resource)
            : simulation.sell_resource(player, *resource);
        if (!accepted) {
            return error_response(operation + " command rejected");
        }
        return success();
    }
    if (operation == "gather") {
        EntityId villager{};
        EntityId target{};
        if (!(input >> villager >> target)) {
            return error_response("gather requires villager and target ids");
        }
        const Unit* unit = unit_by_id(simulation, villager);
        if (unit == nullptr || unit->owner != player ||
            !simulation.command_gather_unit(villager, target)) {
            return error_response("gather command rejected");
        }
        return success();
    }
    if (operation == "move_group" || operation == "attack_move_group") {
        TilePosition destination{};
        if (!(input >> destination.x >> destination.y)) {
            return error_response(
                operation + " requires x, y, and one or more unit ids"
            );
        }
        std::vector<EntityId> ids;
        EntityId id{};
        while (input >> id) ids.push_back(id);
        if (ids.empty()) {
            return error_response(operation + " requires unit ids");
        }
        for (const EntityId unit_id : ids) {
            const Unit* unit = unit_by_id(simulation, unit_id);
            const bool accepted = unit != nullptr && unit->owner == player &&
                (operation == "move_group"
                     ? simulation.command_unit(unit_id, destination)
                     : aoe::execute(
                           simulation,
                           AttackMoveCommand{unit_id, destination}
                       ));
            if (!accepted) {
                return error_response(
                    operation + " rejected unit " + std::to_string(unit_id)
                );
            }
        }
        return success();
    }
    if (operation == "advance") {
        int ticks{};
        if (!(input >> ticks) || ticks < 0 || ticks > 10000) {
            return error_response("advance requires 0..10000 ticks");
        }
        for (int tick = 0; tick < ticks; ++tick) simulation.update();
        return success();
    }
    if (operation == "advance_until_idle") {
        EntityId building_id{};
        int max_ticks{};
        if (!(input >> building_id >> max_ticks) || max_ticks < 0 ||
            max_ticks > 10000) {
            return error_response(
                "advance_until_idle requires building id and 0..10000 ticks"
            );
        }
        const Building* building = building_by_id(simulation, building_id);
        if (building == nullptr || building->owner != player) {
            return error_response("building cannot be observed");
        }
        int elapsed = 0;
        while (elapsed < max_ticks &&
               simulation.outcome() == MatchOutcome::ongoing) {
            building = building_by_id(simulation, building_id);
            if (building == nullptr ||
                (building->production_queue.empty() &&
                 building->age_research_ticks_remaining == 0 &&
                 building->technology_research_ticks_remaining == 0)) {
                break;
            }
            simulation.update();
            ++elapsed;
        }
        std::string result = snapshot(simulation, player, false);
        result.pop_back();
        return result + ",\"elapsed_ticks\":" + std::to_string(elapsed) +
            "}";
    }
    return error_response("unknown gameplay test command");
}

void GameplayTestApi::poll(
    Simulation& simulation,
    Player player,
    bool match_active,
    const HostCommand& host_command
) {
    const std::uintmax_t size = std::filesystem::file_size(commands_path_);
    if (size < command_offset_) command_offset_ = 0;
    if (size == command_offset_) return;

    std::ifstream input(commands_path_);
    input.seekg(static_cast<std::streamoff>(command_offset_));
    std::ofstream output(responses_path_, std::ios::app);
    std::string line;
    while (std::getline(input, line)) {
        command_offset_ = static_cast<std::uintmax_t>(input.tellg());
        if (input.eof()) command_offset_ = size;
        const std::size_t separator = line.find('\t');
        if (separator == std::string::npos || separator == 0) continue;
        const std::string id = line.substr(0, separator);
        const std::string_view command{line.data() + separator + 1,
                                       line.size() - separator - 1};
        const std::optional<std::string> host_result =
            host_command ? host_command(command) : std::nullopt;
        const std::string result = host_result
            ? *host_result
            : match_active
            ? execute(simulation, player, command)
            : error_response("no visible active match");
        output << "{\"id\":\"" << json_escape(id)
               << "\",\"result\":" << result << "}\n";
        output.flush();
    }
}

}  // namespace aoe

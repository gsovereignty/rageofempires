#include "aoe/gameplay_test_api.hpp"

#include <algorithm>
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

const Unit* unit_by_id(const Simulation& simulation, EntityId id) {
    const auto found = std::ranges::find_if(
        simulation.units(),
        [id](const Unit& unit) { return unit.id == id; }
    );
    return found == simulation.units().end() ? nullptr : &*found;
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
           << ",\"kind\":\"" << json_escape(name(unit.kind)) << "\""
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
           << ",\"idle_units\":" << idle_units
           << ",\"selected_units\":[";
    for (std::size_t index = 0;
         index < simulation.selected_units().size(); ++index) {
        if (index != 0) output << ',';
        output << simulation.selected_units()[index];
    }
    output << ']';
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
    if (operation == "state" || operation == "resources" ||
        operation == "idle_units") {
        return snapshot(simulation, player, false);
    }
    if (operation == "list_units") {
        return snapshot(simulation, player, true);
    }
    if (operation == "select") {
        EntityId id{};
        if (!(input >> id)) return error_response("select requires unit id");
        const Unit* unit = unit_by_id(simulation, id);
        if (unit == nullptr || unit->owner != player ||
            !simulation.select_units({id}, player)) {
            return error_response("unit cannot be selected");
        }
        return snapshot(simulation, player, false);
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
        return snapshot(simulation, player, false);
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
        return snapshot(simulation, player, false);
    }
    if (operation == "advance") {
        int ticks{};
        if (!(input >> ticks) || ticks < 0 || ticks > 10000) {
            return error_response("advance requires 0..10000 ticks");
        }
        for (int tick = 0; tick < ticks; ++tick) simulation.update();
        return snapshot(simulation, player, false);
    }
    return error_response("unknown gameplay test command");
}

void GameplayTestApi::poll(Simulation& simulation, Player player) {
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
        const std::string result = execute(
            simulation, player, line.substr(separator + 1)
        );
        output << "{\"id\":\"" << json_escape(id)
               << "\",\"result\":" << result << "}\n";
        output.flush();
    }
}

}  // namespace aoe

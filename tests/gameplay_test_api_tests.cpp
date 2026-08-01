#include "aoe/gameplay_test_api.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    aoe::Simulation simulation = aoe::Simulation::create_demo();
    const auto villager = simulation.units().front().id;
    const auto initial_tick = simulation.tick_number();

    const std::string listed = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "list_units"
    );
    require(listed.find("\"units\":[") != std::string::npos,
            "list_units must include semantic unit state");
    require(listed.find("\"resources\":") != std::string::npos,
            "state must include economy resources");

    const std::string selected = aoe::GameplayTestApi::execute(
        simulation,
        aoe::Player::blue,
        "select " + std::to_string(villager)
    );
    require(selected.find("\"ok\":true") != std::string::npos,
            "owned unit should be selectable by id");
    require(simulation.selected_units() ==
                std::vector<aoe::EntityId>{villager},
            "selection should update live simulation");

    const std::string advanced = aoe::GameplayTestApi::execute(
        simulation, aoe::Player::blue, "advance 5"
    );
    require(advanced.find("\"ok\":true") != std::string::npos,
            "advance should return state");
    require(simulation.tick_number() == initial_tick + 5,
            "advance should run requested deterministic ticks");
    require(aoe::GameplayTestApi::execute(
                simulation, aoe::Player::blue, "advance 10001"
            ).find("\"ok\":false") != std::string::npos,
            "advance must enforce bounded work");

    {
        aoe::GameMap map(8, 5);
        map.set_terrain({2, 1}, aoe::Terrain::gold_mine);
        map.set_resource_amount({2, 1}, 100);
        aoe::Simulation context_simulation(std::move(map));
        const aoe::EntityId worker = context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::blue,
            {1, 1}
        );
        context_simulation.add_building(
            aoe::BuildingKind::town_center,
            aoe::Player::blue,
            {4, 0}
        );
        context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {7, 4}
        );

        const std::string moved = aoe::GameplayTestApi::execute(
            context_simulation,
            aoe::Player::blue,
            "move " + std::to_string(worker) + " 2 1"
        );
        require(moved.find("\"ok\":true") != std::string::npos,
                "semantic move should accept resource context command");
        require(context_simulation.units().front().has_resource_target,
                "semantic move to gold must enter gathering context");
        require(context_simulation.units().front().resource_target ==
                    aoe::TilePosition(2, 1),
                "semantic move must retain clicked resource tile");
        context_simulation.update();
        require(context_simulation.units().front().carried_resource ==
                    aoe::ResourceKind::gold,
                "semantic resource context must run normal gather behavior");
    }

    {
        aoe::Simulation context_simulation(aoe::GameMap(6, 4));
        const aoe::EntityId scout = context_simulation.add_unit(
            aoe::UnitKind::scout_cavalry,
            aoe::Player::blue,
            {1, 1}
        );
        const aoe::EntityId enemy = context_simulation.add_unit(
            aoe::UnitKind::villager,
            aoe::Player::red,
            {2, 1}
        );

        const std::string moved = aoe::GameplayTestApi::execute(
            context_simulation,
            aoe::Player::blue,
            "move " + std::to_string(scout) + " 2 1"
        );
        require(moved.find("\"ok\":true") != std::string::npos,
                "semantic move should accept enemy context command");
        require(context_simulation.units().front().attack_target_id == enemy,
                "semantic move to enemy must enter attack context");
    }

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "aoe-gameplay-test-api-tests";
    std::filesystem::remove_all(directory);
    aoe::GameplayTestApi api(directory);
    {
        std::ofstream commands(
            directory / "commands.jsonl", std::ios::app
        );
        commands << "request-1\tstate\n";
    }
    api.poll(simulation, aoe::Player::blue);
    std::ifstream responses(directory / "responses.jsonl");
    const std::string response{
        std::istreambuf_iterator<char>{responses},
        std::istreambuf_iterator<char>{}
    };
    require(response.find("\"id\":\"request-1\"") !=
                std::string::npos,
            "file boundary must correlate response id");
    std::filesystem::remove_all(directory);
    return 0;
}
